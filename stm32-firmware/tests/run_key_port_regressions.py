from pathlib import Path
import argparse, re, shutil, subprocess, sys, tempfile

p=argparse.ArgumentParser(description='Key GPIO port and production scan function against frozen scanner; no hardware/network.')
p.add_argument('--firmware-root',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--overlay',type=Path)
p.add_argument('--clang',default=shutil.which('clang') or r'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe')
p.add_argument('--windows-sdk',type=Path,default=Path(r'C:\Program Files (x86)\Windows Kits\10'))
a=p.parse_args()
def read(rel):
    q=a.overlay / rel if a.overlay else a.firmware_root / rel
    return (q if q.exists() else a.firmware_root / rel).read_text(encoding='utf-8')
def function(t,signature):
    start=t.index(signature);opening=t.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(t[end]=='{')-(t[end]=='}');end+=1
    return t[start:end]
def run(cmd):
    r=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if r.returncode:
        print(r.stdout+r.stderr);sys.exit(r.returncode)
    return r
kernel=sorted((a.windows_sdk / 'Lib').glob('*/um/x64/kernel32.lib'))
if not kernel:sys.exit('Windows SDK kernel32.lib required for host output; use --windows-sdk.')
with tempfile.TemporaryDirectory(prefix='stm32-key-port-') as td:
    out=Path(td);inc=out / 'include';inc.mkdir();public=out / 'public';public.mkdir()
    port_header=read('modules/middleware/inc/mw_key_port.h')
    (inc / 'mw_key_port.h').write_text(port_header,encoding='utf-8')
    (public / 'mw_key_port.h').write_text(port_header,encoding='utf-8')
    api=public / 'api.c';api.write_text('#include "mw_key_port.h"\nuint8_t read_keys(void){return KeyPort_ReadPressedMask();}\n',encoding='utf-8')
    flags=['--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-O0','-Wall','-Wextra','-Werror']
    run([a.clang,*flags,'-fsyntax-only','-I'+str(public),str(api)])
    run([a.clang,*flags,'-x','c++','-fsyntax-only','-I'+str(public),str(api)])
    (inc / 'gpio.h').write_text('''#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H
#include <stdint.h>
#define GPIOA 0U
#define GPIOB 1U
#define GPIO_PIN_4 (1U<<4)
#define GPIO_PIN_5 (1U<<5)
#define GPIO_PIN_6 (1U<<6)
#define GPIO_PIN_7 (1U<<7)
#define RESET 0U
#define GPIO_PIN_RESET 0U
#define GPIO_PIN_SET 1U
unsigned HAL_GPIO_ReadPin(unsigned port,unsigned pin);
#endif
''',encoding='utf-8')
    key_type=re.search(r'typedef struct Key\s*\{.*?\}\s*Key;',read('Core/Inc/Key.h'),re.S)
    assert key_type
    (inc / 'Key.h').write_text('#include "gpio.h"\n'+key_type.group(0)+'\nextern Key Keys[4];\n',encoding='utf-8')
    scan=function(read('Core/Src/Key.c'),'void Key_Scan(void)')
    assert not re.search(r'\b(?:HAL_GPIO_ReadPin|GPIOA|GPIOB|GPIO_PIN_[4567])\b',scan)
    assert scan.count('KeyPort_ReadPressedMask()')==1
    current=out / 'current_scan.c';current.write_text('#include "Key.h"\n#include "mw_key_port.h"\n'+scan+'\n',encoding='utf-8')
    frozen=out / 'key_scan_before_port.c';frozen.write_text(read('tests/fixtures/key_scan_before_port.c'),encoding='utf-8')
    adapter=out / 'app_key_port.c';adapter.write_text(read('modules/app/src/app_key_port.c'),encoding='utf-8')
    old_obj=out / 'old.obj'
    run([a.clang,*flags,'-I'+str(inc),'-DKeys=LegacyKeys','-DKey_Scan=Legacy_Key_Scan','-c',str(frozen),'-o',str(old_obj)])
    harness=r'''
#include "Key.h"
#include "mw_key_port.h"
Key Keys[4],LegacyKeys[4];
void Key_Scan(void);
void Legacy_Key_Scan(void);
static uint8_t pressed;
static unsigned active,checks,steps;
typedef struct {unsigned used,bad,ports[8],pins[8];} Trace;
static Trace traces[2];
__declspec(dllimport) void * __stdcall GetStdHandle(unsigned);
__declspec(dllimport) int __stdcall WriteFile(void *,const void *,unsigned,unsigned *,void *);
static void text(const char *s){unsigned n=0,w;while(s[n])++n;WriteFile(GetStdHandle((unsigned)-11),s,n,&w,0);}
static void number(unsigned v){char b[16];unsigned n=0,i;do{b[n++]=(char)('0'+v%10);v/=10;}while(v);for(i=0;i<n/2;++i){char t=b[i];b[i]=b[n-1-i];b[n-1-i]=t;}b[n]=0;text(b);}
#define CHECK(x) do{++checks;if(!(x)){text("FAIL at generated test.c line ");number(__LINE__);text("\n");return __LINE__;}}while(0)
#define STEP(mask,consume) do{int r=step((uint8_t)(mask),(uint8_t)(consume));if(r)return r;}while(0)
unsigned HAL_GPIO_ReadPin(unsigned port,unsigned pin){
    unsigned index=4;Trace *t=&traces[active];
    if(port==GPIOA&&pin==GPIO_PIN_4)index=0;
    else if(port==GPIOA&&pin==GPIO_PIN_5)index=1;
    else if(port==GPIOB&&pin==GPIO_PIN_7)index=2;
    else if(port==GPIOA&&pin==GPIO_PIN_6)index=3;
    if(index==4){t->bad=1;return GPIO_PIN_SET;}
    if(t->used>=8){t->bad=1;return GPIO_PIN_SET;}
    t->ports[t->used]=port;t->pins[t->used]=pin;++t->used;
    return (pressed & (1U<<index))?GPIO_PIN_RESET:GPIO_PIN_SET;
}
static void reset(void){
    unsigned i;
    for(i=0;i<4;++i){
        Keys[i].State=Keys[i].Pin_state=Keys[i].ClickFlag=Keys[i].LongClickFlag=0;
        Keys[i].Presstime=Keys[i].Releasetime=0;
        LegacyKeys[i]=Keys[i];
    }
}
static int step(uint8_t mask,uint8_t consume){
    unsigned i;
    pressed=mask;
    for(i=0;i<2;++i){traces[i].used=traces[i].bad=0;}
    active=0;Legacy_Key_Scan();active=1;Key_Scan();++steps;
    CHECK(traces[0].used==4);CHECK(traces[1].used==4);CHECK(!traces[0].bad&&!traces[1].bad);
    for(i=0;i<4;++i){
        CHECK(traces[0].ports[i]==traces[1].ports[i]);CHECK(traces[0].pins[i]==traces[1].pins[i]);
        CHECK(Keys[i].State==LegacyKeys[i].State);CHECK(Keys[i].Pin_state==LegacyKeys[i].Pin_state);
        CHECK(Keys[i].ClickFlag==LegacyKeys[i].ClickFlag);CHECK(Keys[i].LongClickFlag==LegacyKeys[i].LongClickFlag);
        CHECK(Keys[i].Presstime==LegacyKeys[i].Presstime);CHECK(Keys[i].Releasetime==LegacyKeys[i].Releasetime);
        /* Model immediate/delayed main-loop consumption equally in both versions.
           Real Key_Func integration remains in the existing control suite. */
        if((consume&(1U<<i))&&Keys[i].ClickFlag){
            Keys[i].ClickFlag=Keys[i].LongClickFlag=Keys[i].State=0;
            LegacyKeys[i].ClickFlag=LegacyKeys[i].LongClickFlag=LegacyKeys[i].State=0;
        }
    }
    return 0;
}
int main(void){
    unsigned i,j,k,rng=0x62C3A17U,remaining=0;uint8_t held=0;
    const unsigned expected_ports[]={GPIOA,GPIOA,GPIOB,GPIOA};
    const unsigned expected_pins[]={GPIO_PIN_4,GPIO_PIN_5,GPIO_PIN_7,GPIO_PIN_6};
    const unsigned press_counts[]={0,1,3,4,149,150,151};
    /* Independent pin/polarity/order oracle: all 16 combinations. */
    for(i=0;i<16;++i){
        pressed=(uint8_t)i;active=1;traces[1].used=traces[1].bad=0;
        CHECK(KeyPort_ReadPressedMask()==i);CHECK(traces[1].used==4);CHECK(!traces[1].bad);
        for(j=0;j<4;++j){CHECK(traces[1].ports[j]==expected_ports[j]);CHECK(traces[1].pins[j]==expected_pins[j]);}
        reset();STEP(i,0);
    }
    /* Independent boundary expectations plus frozen/current comparison. */
    for(i=0;i<4;++i)for(j=0;j<7;++j){
        reset();
        for(k=0;k<press_counts[j];++k){STEP(1U<<i,0);}
        CHECK(Keys[i].ClickFlag==0);CHECK(Keys[i].LongClickFlag==0);
        for(k=0;k<5;++k){STEP(0,0);CHECK(Keys[i].ClickFlag==0);}
        STEP(0,0);
        CHECK(Keys[i].ClickFlag==(press_counts[j]>3));
        CHECK(Keys[i].LongClickFlag==(press_counts[j]>150));
    }
    /* Same masks over time: long holds, bounce, multiple keys and delayed consumption. */
    reset();
    for(i=0;i<4096;++i){
        rng=rng*1664525U+1013904223U;
        if(remaining==0){held=(uint8_t)((rng>>12)&15U);remaining=(rng>>20)%190U+1U;}
        --remaining;
        STEP(i%97U<5U?(uint8_t)(rng&15U):held,(i%11U==0U)?0U:15U);
    }
    /* Preserve existing default-state recovery and uint32 counter wrap behavior. */
    reset();
    for(i=0;i<4;++i){Keys[i].State=LegacyKeys[i].State=7;}
    STEP(15,0);for(i=0;i<4;++i){CHECK(Keys[i].State==0);}
    for(i=0;i<4;++i){Keys[i].Presstime=LegacyKeys[i].Presstime=0xFFFFFFFEU;}
    STEP(15,0);STEP(15,0);STEP(15,0);
    for(i=0;i<4;++i){CHECK(Keys[i].Presstime==1);}
    for(i=0;i<4;++i){Keys[i].State=LegacyKeys[i].State=1;Keys[i].Releasetime=LegacyKeys[i].Releasetime=0xFFFFFFFFU;}
    STEP(0,0);for(i=0;i<4;++i){CHECK(Keys[i].Releasetime==0);}
    text("PASS: key port/scanner assertions: ");number(checks);text("; differential scan steps: ");number(steps);text("\n");
    return 0;
}
'''
    test=out / 'test.c';test.write_text(harness,encoding='utf-8')
    exe=out / 'test.exe'
    run([a.clang,*flags,'-I'+str(inc),'-fuse-ld=lld','-nostdlib','-Wl,/entry:main,/subsystem:console',str(test),str(current),str(adapter),str(old_obj),str(kernel[-1]),'-o',str(exe)])
    r=run([str(exe)]);print(r.stdout+r.stderr,end='')
print('PASS: 16 pin combinations, frozen/current scan state and GPIO order, debounce/long-press boundaries, bounce/delayed consumption, counter wrap and HAL-free port header. No hardware/network.')

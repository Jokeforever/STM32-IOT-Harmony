from pathlib import Path
import argparse,re,shutil,subprocess,sys,tempfile

p=argparse.ArgumentParser(description='Production alarm GPIO port and BEEP compatibility macros; independent pin/polarity oracle, no hardware/network.')
p.add_argument('--firmware-root',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--overlay',type=Path)
p.add_argument('--clang',default=shutil.which('clang') or r'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe')
p.add_argument('--windows-sdk',type=Path,default=Path(r'C:\Program Files (x86)\Windows Kits\10'))
a=p.parse_args()
def read(rel):
    q=a.overlay / rel if a.overlay else a.firmware_root / rel
    return (q if q.exists() else a.firmware_root / rel).read_text(encoding='utf-8')
def run(cmd):
    r=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if r.returncode:print(r.stdout+r.stderr);sys.exit(r.returncode)
    return r
kernel=sorted((a.windows_sdk / 'Lib').glob('*/um/x64/kernel32.lib'))
if not kernel:sys.exit('Windows SDK kernel32.lib required for host output; use --windows-sdk.')
with tempfile.TemporaryDirectory(prefix='stm32-alarm-port-') as td:
    out=Path(td);inc=out / 'include';inc.mkdir();public=out / 'public';public.mkdir()
    header=read('modules/middleware/inc/mw_alarm_port.h')
    (inc / 'mw_alarm_port.h').write_text(header,encoding='utf-8');(public / 'mw_alarm_port.h').write_text(header,encoding='utf-8')
    api=public / 'api.c';api.write_text('#include "mw_alarm_port.h"\nvoid api_check(void){AlarmPort_SetIndicator(1U);AlarmPort_SetBuzzer(0U);}\n',encoding='utf-8')
    flags=['--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-O0','-Wall','-Wextra','-Werror']
    run([a.clang,*flags,'-fsyntax-only','-I'+str(public),str(api)])
    run([a.clang,*flags,'-x','c++','-fsyntax-only','-I'+str(public),str(api)])
    (inc / 'gpio.h').write_text('''#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H
#include <stdint.h>
#define GPIOB 1U
#define GPIOC 2U
#define GPIO_PIN_12 (1U<<12)
#define GPIO_PIN_13 (1U<<13)
#define GPIO_PIN_RESET 0U
#define GPIO_PIN_SET 1U
void HAL_GPIO_WritePin(unsigned port,unsigned pin,unsigned value);
#endif
''',encoding='utf-8')
    # Compile the actual public compatibility header with unrelated dependencies absent.
    for name in ['main.h','usart3.h','LED.h','bump.h','fan.h','jdq.h']:(inc / name).write_text('/* Unused dependency for BEEP macro checks. */\n',encoding='utf-8')
    (inc / 'beep.h').write_text(read('Core/Inc/beep.h'),encoding='utf-8')
    old=read('tests/fixtures/beep_before_alarm_port.h')
    macros=[]
    for name in ['BEEP_ON','BEEP_OFF']:
        definition=re.search(r'^#define\s+'+name+r'\s+([^\r\n]+)',old,re.M);assert definition,name
        assert 'HAL_GPIO_WritePin' in definition[1]
        macros.append('#define LEGACY_'+name+' '+definition[1])
    (inc / 'legacy_beep_macros.h').write_text('\n'.join(macros)+'\n',encoding='utf-8')
    assert not re.search(r'\bHAL_[A-Za-z0-9_]+\b',read('Core/Src/beep.c'))
    assert not re.search(r'\b(?:GPIOB|GPIO_PIN_12|HAL_GPIO_WritePin)\b',read('Core/Inc/beep.h'))
    port=out / 'app_alarm_port.c';port.write_text(read('modules/app/src/app_alarm_port.c'),encoding='utf-8')
    harness=r'''
#include "gpio.h"
#include "mw_alarm_port.h"
#include "beep.h"
#include "legacy_beep_macros.h"
typedef struct {unsigned port,pin,value;} Event;
static Event events[8];static unsigned used,bad,indicator_level,buzzer_level,checks;
__declspec(dllimport) void *GetStdHandle(uint32_t);
__declspec(dllimport) int WriteFile(void*,const void*,uint32_t,uint32_t*,void*);
static void text(const char *s){uint32_t n=0,w;while(s[n])++n;WriteFile(GetStdHandle((uint32_t)-11),s,n,&w,0);}
static void number(unsigned n){char b[16];unsigned i=15;b[i]=0;do{b[--i]=(char)('0'+n%10);n/=10;}while(n);text(b+i);}
#define CHECK(x) do{++checks;if(!(x)){text("FAIL alarm port line ");number(__LINE__);text("\n");return __LINE__;}}while(0)
void HAL_GPIO_WritePin(unsigned port,unsigned pin,unsigned value){
    if(used>=8){bad=1;return;}
    events[used].port=port;events[used].pin=pin;events[used].value=value;++used;
    if(port==GPIOC&&pin==GPIO_PIN_13)indicator_level=value;
    else if(port==GPIOB&&pin==GPIO_PIN_12)buzzer_level=value;
    else bad=1;
    if(value!=GPIO_PIN_RESET&&value!=GPIO_PIN_SET)bad=1;
}
static void reset(void){used=bad=0;indicator_level=GPIO_PIN_SET;buzzer_level=GPIO_PIN_RESET;}
int main(void){
    unsigned i,j;Event old_events[2];
    for(i=0;i<256;++i){
        unsigned value=i?GPIO_PIN_RESET:GPIO_PIN_SET;
        reset();AlarmPort_SetIndicator((uint8_t)i);
        CHECK(!bad);CHECK(used==1);CHECK(events[0].port==GPIOC);CHECK(events[0].pin==GPIO_PIN_13);
        CHECK(events[0].value==value);CHECK(indicator_level==value);CHECK(buzzer_level==GPIO_PIN_RESET);
        reset();AlarmPort_SetBuzzer((uint8_t)i);
        CHECK(!bad);CHECK(used==1);CHECK(events[0].port==GPIOB);CHECK(events[0].pin==GPIO_PIN_12);
        CHECK(events[0].value==value);CHECK(buzzer_level==value);CHECK(indicator_level==GPIO_PIN_SET);
    }
    /* Independent outputs and original indicator-then-buzzer order. */
    for(i=0;i<4;++i){
        reset();AlarmPort_SetIndicator((uint8_t)(i&1U));AlarmPort_SetBuzzer((uint8_t)(i&2U));
        CHECK(!bad);CHECK(used==2);CHECK(events[0].port==GPIOC&&events[0].pin==GPIO_PIN_13);
        CHECK(events[1].port==GPIOB&&events[1].pin==GPIO_PIN_12);
        CHECK(indicator_level==((i&1U)?GPIO_PIN_RESET:GPIO_PIN_SET));CHECK(buzzer_level==((i&2U)?GPIO_PIN_RESET:GPIO_PIN_SET));
    }
    /* Repeated same-value requests must still perform one original GPIO write. */
    reset();for(i=0;i<8;++i)AlarmPort_SetIndicator(1U);CHECK(!bad&&used==8);
    for(i=0;i<8;++i){CHECK(events[i].port==GPIOC&&events[i].pin==GPIO_PIN_13&&events[i].value==GPIO_PIN_RESET);}
    reset();for(i=0;i<8;++i)AlarmPort_SetBuzzer(0U);CHECK(!bad&&used==8);
    for(i=0;i<8;++i){CHECK(events[i].port==GPIOB&&events[i].pin==GPIO_PIN_12&&events[i].value==GPIO_PIN_SET);}
    /* Compare actual compatibility macros with frozen original direct writes. */
    reset();LEGACY_BEEP_ON;LEGACY_BEEP_OFF;CHECK(!bad&&used==2);
    old_events[0]=events[0];old_events[1]=events[1];reset();BEEP_ON;BEEP_OFF;CHECK(!bad&&used==2);
    for(i=0;i<2;++i){CHECK(events[i].port==old_events[i].port);CHECK(events[i].pin==old_events[i].pin);CHECK(events[i].value==old_events[i].value);}
    CHECK(buzzer_level==GPIO_PIN_SET);
    for(i=0;i<2;++i){
        reset();if(i)BEEP_ON;else BEEP_OFF;
        CHECK(!bad&&used==1);CHECK(events[0].port==GPIOB&&events[0].pin==GPIO_PIN_12);
        CHECK(events[0].value==(i?GPIO_PIN_RESET:GPIO_PIN_SET));
    }
    /* Mixed on/off sequences clear one channel without modifying the other. */
    for(j=0;j<4;++j){
        reset();AlarmPort_SetIndicator(1U);AlarmPort_SetBuzzer(1U);
        AlarmPort_SetIndicator((uint8_t)(j&1U));AlarmPort_SetBuzzer((uint8_t)(j&2U));
        CHECK(!bad&&used==4);CHECK(indicator_level==((j&1U)?GPIO_PIN_RESET:GPIO_PIN_SET));CHECK(buzzer_level==((j&2U)?GPIO_PIN_RESET:GPIO_PIN_SET));
    }
    text("PASS: alarm output port assertions: ");number(checks);text("\n");return 0;
}
'''
    test=out / 'test.c';test.write_text(harness,encoding='utf-8');exe=out / 'test.exe'
    run([a.clang,*flags,'-I'+str(inc),'-fuse-ld=lld','-nostdlib','-Wl,/entry:main,/subsystem:console',str(test),str(port),str(kernel[-1]),'-o',str(exe)])
    r=run([str(exe)]);print(r.stdout+r.stderr,end='')
print('PASS: all 256 request values per output, active-low pins, independent channels, repeated writes, BEEP compatibility and HAL-free C/C++ contract. Startup/order also covered by alarm integration suite. No hardware/network.')

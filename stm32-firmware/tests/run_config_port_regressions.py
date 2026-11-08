from pathlib import Path
import argparse,shutil,subprocess,sys,tempfile
parser=argparse.ArgumentParser(description='Storage-call equivalence against the frozen pre-port configuration code.')
parser.add_argument('--firmware-root',type=Path,default=Path(__file__).resolve().parents[1])
parser.add_argument('--overlay',type=Path)
parser.add_argument('--clang',default=shutil.which('clang') or r'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe')
args=parser.parse_args()
def read(rel):
    p=args.overlay/rel if args.overlay else args.firmware_root/rel
    return (p if p.exists() else args.firmware_root/rel).read_text(encoding='utf-8')
harness=r"""
#include "mw_config.h"
#include "app_config.h"
#include <stddef.h>
void Legacy_Config_Load(void);
uint8_t Legacy_Config_Save(void);
void Legacy_Config_Process(void);
void Legacy_Config_MarkDirty(void);
uint8_t Legacy_Config_IsDirty(void);
typedef struct {unsigned kind,offset,length;uint32_t value;unsigned ok;} Event;
typedef struct {uint8_t memory[256];RuntimeConfig runtime;Event trace[16];unsigned events,overflow,read_fail,write_fail,corrupt;} Context;
static Context contexts[2];
static unsigned active;
static uint32_t now;
static const RuntimeConfig defaults={2,0,50,25,2000,100,1000,30,75,200,20,1500,0,0};
int _fltused=0;
void *memset(void *p,int c,size_t n){uint8_t *b=p;while(n--)*b++=(uint8_t)c;return p;}
void *memcpy(void *p,const void *q,size_t n){uint8_t *b=p;const uint8_t *a=q;while(n--)*b++=*a++;return p;}
static uint32_t hash(const uint8_t *p,uint16_t n){uint32_t v=2166136261U;while(n--)v=(v^*p++)*16777619U;return v;}
static void event(unsigned kind,unsigned offset,unsigned length,uint32_t value,unsigned ok)
{
    Context *c=&contexts[active];Event *e;
    if(c->events>=16){c->overflow=1;return;}
    e=&c->trace[c->events++];e->kind=kind;e->offset=offset;e->length=length;e->value=value;e->ok=ok;
}
uint32_t HAL_GetTick(void){event(3,0,0,now,1);return now;}
uint8_t EEPROM24C02_Read(uint8_t offset,uint8_t *data,uint16_t length)
{
    Context *c=&contexts[active];unsigned ok=!c->read_fail&&(unsigned)offset+length<=256;
    if(ok)memcpy(data,c->memory+offset,length);
    event(1,offset,length,ok?hash(data,length):0,ok);return (uint8_t)ok;
}
uint8_t EEPROM24C02_Write(uint8_t offset,const uint8_t *data,uint16_t length)
{
    Context *c=&contexts[active];unsigned ok=!c->write_fail&&(unsigned)offset+length<=256;
    event(2,offset,length,hash(data,length),ok);
    if(ok){memcpy(c->memory+offset,data,length);if(c->corrupt)c->memory[offset]^=1;}
    return (uint8_t)ok;
}
const RuntimeConfig *AppConfig_Get(void){return &contexts[active].runtime;}
void AppConfig_GetDefaults(RuntimeConfig *out){*out=defaults;}
uint8_t AppConfig_Restore(const RuntimeConfig *r){contexts[active].runtime=*r;return 1;}
static int same_runtime(const RuntimeConfig *a,const RuntimeConfig *b)
{
    return a->led_mode==b->led_mode&&a->bump_mode==b->bump_mode&&a->led_manual_pwm==b->led_manual_pwm&&
      a->temp_threshold==b->temp_threshold&&a->co2_threshold==b->co2_threshold&&a->ldr_low==b->ldr_low&&a->ldr_high==b->ldr_high&&
      a->soil_low==b->soil_low&&a->soil_high==b->soil_high&&a->pid_target_lux==b->pid_target_lux&&a->pid_dead_zone==b->pid_dead_zone&&
      a->pid_kp_x1000==b->pid_kp_x1000&&a->pid_ki_x1000==b->pid_ki_x1000&&a->pid_kd_x1000==b->pid_kd_x1000;
}
static int same_memory(void){unsigned i;for(i=0;i<256;i++)if(contexts[0].memory[i]!=contexts[1].memory[i])return 0;return 1;}
static int same_trace(void)
{
    unsigned i;if(contexts[0].overflow||contexts[1].overflow||contexts[0].events!=contexts[1].events)return 0;
    for(i=0;i<contexts[0].events;i++){
      const Event *a=&contexts[0].trace[i],*b=&contexts[1].trace[i];
      if(a->kind!=b->kind||a->offset!=b->offset||a->length!=b->length||a->value!=b->value||a->ok!=b->ok)return 0;
    }return 1;
}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
#define RUN(op) do{int line=run(op);if(line)return line;}while(0)
static int run(unsigned operation)
{
    uint8_t old_result=0,new_result=0;
    contexts[0].events=contexts[1].events=0;contexts[0].overflow=contexts[1].overflow=0;
    active=0;
    switch(operation){case 0:Legacy_Config_Load();break;case 1:Legacy_Config_MarkDirty();break;case 2:Legacy_Config_Process();break;case 3:old_result=Legacy_Config_Save();break;}
    active=1;
    switch(operation){case 0:Config_Load();break;case 1:Config_MarkDirty();break;case 2:Config_Process();break;case 3:new_result=Config_Save();break;}
    CHECK(old_result==new_result);CHECK(same_runtime(&contexts[0].runtime,&contexts[1].runtime));
    CHECK(same_memory());CHECK(same_trace());CHECK(Legacy_Config_IsDirty()==Config_IsDirty());
    return 0;
}
int main(void)
{
    unsigned i,rng=0x12345678U;
    static const uint32_t increments[]={0,1,10,999,1000,1001,5000};
    memset(contexts[0].memory,255,256);memset(contexts[1].memory,255,256);
    contexts[0].runtime=contexts[1].runtime=defaults;
    now=0;RUN(0);CHECK(Config_IsDirty()!=0);
    now=999;RUN(2);CHECK(Config_IsDirty()!=0);
    now=1000;RUN(2);CHECK(Config_IsDirty()==0);
    /* Restart debounce on the last change; then cross uint32 wrap at999/1000ms. */
    now=2000;RUN(1);now=2500;RUN(1);now=3499;RUN(2);CHECK(Config_IsDirty()!=0);
    now=3500;RUN(2);CHECK(Config_IsDirty()==0);
    now=0xFFFFFF00U;RUN(1);now=743;RUN(2);CHECK(Config_IsDirty()!=0);
    now=744;RUN(2);CHECK(Config_IsDirty()==0);
    /* Failed background save retains dirty state and retries only after another second. */
    RUN(1);contexts[0].write_fail=contexts[1].write_fail=1;
    now+=1000;RUN(2);CHECK(Config_IsDirty()!=0);
    contexts[0].write_fail=contexts[1].write_fail=0;
    now+=999;RUN(2);CHECK(Config_IsDirty()!=0);
    now+=1;RUN(2);CHECK(Config_IsDirty()==0);
    for(i=0;i<1000;i++)
    {
        unsigned operation;
        rng=rng*1664525U+1013904223U;now+=increments[i%7];
        contexts[0].read_fail=contexts[1].read_fail=(i%13==0);
        contexts[0].write_fail=contexts[1].write_fail=(i%11==0);
        contexts[0].corrupt=contexts[1].corrupt=(i%17==0);
        if(i%3==0){contexts[0].runtime.temp_threshold=contexts[1].runtime.temp_threshold=(uint8_t)(rng%101);
          contexts[0].runtime.pid_target_lux=contexts[1].runtime.pid_target_lux=(int16_t)((rng>>10)%1000);}
        operation=(rng>>20)%4;RUN(operation);
    }
    return 0;
}
"""
legacy=read('tests/fixtures/config_before_port.c')
renames=''.join('#define '+name+' Legacy_'+name+'\n' for name in ['Config_Load','Config_Save','Config_Process','Config_MarkDirty','Config_IsDirty'])
with tempfile.TemporaryDirectory(prefix='stm32-config-port-') as td:
    out=Path(td)
    def put(name,t):
        p=out/name;p.write_text(t,encoding='utf-8');return str(p)
    for name,rel in [('mw_config.h','modules/middleware/inc/mw_config.h'),('mw_config_port.h','modules/middleware/inc/mw_config_port.h'),('app_config.h','modules/app/inc/app_config.h')]:put(name,read(rel))
    put('main.h','#include <stdint.h>\nuint32_t HAL_GetTick(void);\n')
    put('i2c.h','#include <stdint.h>\n#define EEPROM_24C02_SIZE 256U\nuint32_t HAL_GetTick(void);\nuint8_t EEPROM24C02_Read(uint8_t,uint8_t*,uint16_t);\nuint8_t EEPROM24C02_Write(uint8_t,const uint8_t*,uint16_t);\n')
    put('string.h','#include <stddef.h>\nvoid *memset(void*,int,size_t);\n')
    sources=[put('test.c',harness),put('legacy.c',renames+legacy),put('mw_config.c',read('modules/middleware/src/mw_config.c')),put('app_config_port.c',read('modules/app/src/app_config_port.c'))]
    cmd=[args.clang,'--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-fuse-ld=lld','-nostdlib','-O0',
         '-Wall','-Wextra','-Werror','-Wl,/entry:main,/subsystem:console','-I'+str(out),*sources,'-o',str(out/'test.exe')]
    r=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if r.returncode:print(r.stdout+r.stderr);sys.exit(r.returncode)
    r=subprocess.run([str(out/'test.exe')])
    if r.returncode:
        print('FAIL at test.c line',r.returncode)
        if 0<r.returncode<=len(harness.splitlines()):print(harness.splitlines()[r.returncode-1])
        sys.exit(1)
print('PASS: 1000 legacy/new storage operations plus debounce/retry/wrap vectors; call traces, status, bytes, runtime and dirty flags match.')

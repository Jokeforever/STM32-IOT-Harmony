from pathlib import Path
import argparse,re,shutil,subprocess,sys,tempfile

p=argparse.ArgumentParser(description='Full production OLED page rendering against frozen pre-separation source; host boundary mocks only.')
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
versions=sorted((a.windows_sdk / 'Lib').glob('*/ucrt/x64/ucrt.lib'))
if not versions:sys.exit('Windows SDK ucrt.lib required; use --windows-sdk.')
ucrt=versions[-1];kernel=ucrt.parents[2] / 'um/x64/kernel32.lib';assert kernel.exists()
with tempfile.TemporaryDirectory(prefix='stm32-display-') as td:
    out=Path(td);inc=out / 'include';inc.mkdir()
    def header(name,value):(inc / name).write_text(value,encoding='utf-8')
    for name,rel in [('app_config.h','modules/app/inc/app_config.h'),('app_sensors.h','modules/app/inc/app_sensors.h'),('sensor_data.h','modules/middleware/inc/sensor_data.h'),('mw_alarm.h','modules/middleware/inc/mw_alarm.h'),('LED.h','Core/Inc/LED.h'),('mw_pid.h','modules/middleware/inc/mw_pid.h'),('mw_led_port.h','modules/middleware/inc/mw_led_port.h'),('bump.h','Core/Inc/bump.h'),('Display.h','Core/Inc/Display.h'),('oled.h','Core/Inc/oled.h')]:
        header(name,read(rel))
    header('main.h','#include <stdint.h>\n')
    header('stm32f1xx_hal.h','#include <stdint.h>\n')
    header('TS.h','/* Unused ADC boundary; page renderer does not acquire soil data. */\n')
    header('stdio.h','int sprintf(char*,const char*,...);\n')
    production=out / 'Display.c';production.write_text(read('Core/Src/Display.c'),encoding='utf-8')
    fixture=out / 'display_before_separation.c';fixture.write_text(read('tests/fixtures/display_before_separation.c'),encoding='utf-8')
    for rel,fn in [('Core/Src/LED.c','LED_Display'),('Core/Src/bump.c','Bump_Display')]:
        assert not re.search(r'void\s+'+fn+r'\s*\(void\)\s*\{',read(rel))
    flags=['--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-O0','-Wall','-Wextra','-Werror','-I'+str(inc)]
    names=['Display'+str(i)+'_Init' for i in range(1,6)]+['Refresh'+str(i)+'_Data' for i in range(1,6)]+['Display_Data','LED_Display','Bump_Display','bufdat']
    old=out / 'legacy.obj'
    run([a.clang,*flags,*['-D'+n+'=Legacy_'+n for n in names],'-c',str(fixture),'-o',str(old)])
    harness=r'''
#include "Display.h"
#include "app_config.h"
#include "app_sensors.h"
#include <stddef.h>
#include <stdarg.h>
int _fltused=0;
__declspec(dllimport) int __stdio_common_vsprintf(unsigned long long,char*,size_t,const char*,void*,va_list);
/* Real host CRT formatting, shared by both implementations; not hand-built values. */
int sprintf(char *out,const char *format,...){int n;va_list ap;va_start(ap,format);n=__stdio_common_vsprintf(0,out,(size_t)-1,format,0,ap);va_end(ap);return n;}
__declspec(dllimport) void *GetStdHandle(uint32_t);
__declspec(dllimport) int WriteFile(void*,const void*,uint32_t,uint32_t*,void*);
static void text(const char *s){uint32_t n=0,w;while(s[n])++n;WriteFile(GetStdHandle((uint32_t)-11),s,n,&w,0);}
static void number(unsigned n){char b[16];unsigned i=15;b[i]=0;do{b[--i]=(char)('0'+n%10);n/=10;}while(n);text(b+i);}
#define CHECK(x) do{++checks;if(!(x)){text("FAIL display test line ");number(__LINE__);text("\n");return __LINE__;}}while(0)
#define PAIR(op) do{int r=pair(op);if(r)return r;}while(0)
typedef struct {unsigned kind,x,y,arg;char string[32];} Event;
typedef struct {unsigned used,bad,config_reads,sensor_reads;Event events[32];} Context;
static Context contexts[2];static unsigned active,checks,cases;
static RuntimeConfig runtime={2,0,50,25,2000,100,1000,30,75,200,20,1500,0,0,20};
static SensorData sensors={23,57,456,61,900,11};
float water_vol=0.5f;
volatile DisplayScreen CurrentScreen=DISPLAY_NONE;
uint8_t Monitor_para1;
volatile Abnormal abnormal[3];
LED_Control led_ctrl;
const RuntimeConfig *AppConfig_Get(void){++contexts[active].config_reads;return &runtime;}
const SensorData *SensorService_GetData(void){++contexts[active].sensor_reads;return &sensors;}
static Event *event(unsigned kind,unsigned x,unsigned y,unsigned arg){
    Context *c=&contexts[active];Event *e;
    if(c->used>=32){c->bad=1;return 0;}
    e=&c->events[c->used++];e->kind=kind;e->x=x;e->y=y;e->arg=arg;e->string[0]=0;return e;
}
void OLED_ShowCHinese(uint8_t x,uint8_t y,uint8_t glyph){(void)event(1,x,y,glyph);}
void OLED_ShowString(uint8_t x,uint8_t y,uint8_t *s,uint8_t size){
    Event *e=event(2,x,y,size);unsigned i=0;if(!e)return;
    while(s[i]&&i<31){e->string[i]=(char)s[i];++i;}
    e->string[i]=0;if(s[i])contexts[active].bad=1;
}
void OLED_Clear(void){(void)event(3,0,0,0);}
static unsigned equal(const char *a,const char *b){while(*a&&*a==*b){++a;++b;}return *a==*b;}
static uint32_t hash(const void *p,size_t n){const unsigned char *s=p;uint32_t h=2166136261U;while(n--){h^=*s++;h*=16777619U;}return h;}
static uint32_t state(void){return hash(&runtime,sizeof(runtime))^hash(&sensors,sizeof(sensors))^hash(&water_vol,sizeof(water_vol))^hash(&led_ctrl,sizeof(led_ctrl))^(uint32_t)CurrentScreen^(uint32_t)Monitor_para1;}
LEGACY_PROTOTYPES
static void invoke(unsigned legacy,unsigned op){
    if(legacy){switch(op){case 0:Legacy_Display1_Init();break;case 1:Legacy_Display2_Init();break;case 2:Legacy_Display3_Init();break;case 3:Legacy_Display4_Init();break;case 4:Legacy_Display5_Init();break;case 5:Legacy_Refresh1_Data();break;case 6:Legacy_Refresh2_Data();break;case 7:Legacy_Refresh3_Data();break;case 8:Legacy_Refresh4_Data();break;case 9:Legacy_Refresh5_Data();break;case 10:Legacy_Display_Data();break;case 11:Legacy_LED_Display();break;default:Legacy_Bump_Display();break;}}
    else{switch(op){case 0:Display1_Init();break;case 1:Display2_Init();break;case 2:Display3_Init();break;case 3:Display4_Init();break;case 4:Display5_Init();break;case 5:Refresh1_Data();break;case 6:Refresh2_Data();break;case 7:Refresh3_Data();break;case 8:Refresh4_Data();break;case 9:Refresh5_Data();break;case 10:Display_Data();break;case 11:LED_Display();break;default:Bump_Display();break;}}
}
static void clear(unsigned index){contexts[index].used=contexts[index].bad=contexts[index].config_reads=contexts[index].sensor_reads=0;}
static int expected_parameter_page(unsigned op);
static int pair(unsigned op){
    if(op==3||op==4||op==8||op==9||(op==10&&(CurrentScreen==DISPLAY_SCREEN4||CurrentScreen==DISPLAY_SCREEN5)))return expected_parameter_page(op);
    unsigned i;uint32_t before=state();++cases;clear(0);clear(1);
    active=0;invoke(1,op);CHECK(state()==before);active=1;invoke(0,op);CHECK(state()==before);
    CHECK(!contexts[0].bad&&!contexts[1].bad);CHECK(contexts[0].used==contexts[1].used);
    CHECK(contexts[0].config_reads==contexts[1].config_reads);CHECK(contexts[0].sensor_reads==contexts[1].sensor_reads);
    for(i=0;i<contexts[0].used;++i){
        const Event *a=&contexts[0].events[i],*b=&contexts[1].events[i];
        CHECK(a->kind==b->kind);CHECK(a->x==b->x);CHECK(a->y==b->y);CHECK(a->arg==b->arg);CHECK(equal(a->string,b->string));
    }
    return 0;
}
static int expected_mode(unsigned pump,unsigned mode){
    static const unsigned glyphs[3][2]={{10,11},{8,9},{12,13}};
    unsigned i;Context *c=&contexts[1];clear(1);active=1;
    if(pump){runtime.bump_mode=(uint8_t)mode;Bump_Display();}else{runtime.led_mode=(uint8_t)mode;LED_Display();}
    CHECK(!c->bad);CHECK(c->config_reads==1);CHECK(c->sensor_reads==0);CHECK(c->used==(mode<3?2U:0U));
    if(mode<3)for(i=0;i<2;++i){CHECK(c->events[i].kind==1);CHECK(c->events[i].x==(pump?(i?72U:54U):(i?80U:64U)));CHECK(c->events[i].y==3);CHECK(c->events[i].arg==glyphs[mode][i]);}
    return 0;
}
static int expected_string(unsigned index,unsigned x,unsigned y,const char *value){
    const Event *e=&contexts[1].events[index];
    CHECK(e->kind==2);CHECK(e->x==x);CHECK(e->y==y);CHECK(e->arg==16);CHECK(equal(e->string,value));return 0;
}
static int expected_parameter_page(unsigned op){
    static const unsigned ys[4]={0,2,4,6},selections[4]={0,1,2,6};
    static const char *labels[4]={"LL:","LH:","TH:","TL:"};
    static const char *soil_labels[3]={"BL:","BH:","CO:"};
    unsigned soil_page=(op==4||op==9||(op==10&&CurrentScreen==DISPLAY_SCREEN5));
    unsigned init=(op==3||op==4),rows=soil_page?3U:4U;
    unsigned i;uint32_t before=state();Context *c=&contexts[1];
    ++cases;clear(1);active=1;invoke(0,op);CHECK(state()==before);CHECK(!c->bad);CHECK(c->sensor_reads==0);
    CHECK(c->used==(init?rows:2U*rows));
    for(i=0;i<rows;++i){
        const Event *e=&c->events[init?i:2*i];
        CHECK(e->kind==2&&e->arg==16&&e->y==ys[i]);
        if(init){CHECK(e->x==12&&equal(e->string,soil_page?soil_labels[i]:labels[i]));}
        else {
            const Event *n=&c->events[2*i+1];
            CHECK(e->x==0&&equal(e->string,Monitor_para1==(soil_page?3U+i:selections[i])?">":" "));
            CHECK(n->kind==2&&n->arg==16&&n->x==48&&n->y==ys[i]);
            /* All output pixels fit in 128x64 at the real 8x16 ASCII font. */
            CHECK(n->string[5]==0&&n->x+5*8<=128&&n->y+1<8);
        }
    }
    return 0;
}
int main(void){
    unsigned i,j,k,rng=0x37E2A61U;
    const float volumes[]={0.0f,0.1f,0.25f,1.5f,10.0f,123.4f,9999.0f};
    for(i=0;i<256;++i){int r=expected_mode(0,i);if(r)return r;r=expected_mode(1,i);if(r)return r;PAIR(11);PAIR(12);}
    runtime.led_mode=1;runtime.bump_mode=0;
    for(i=0;i<13;++i){PAIR(i);}
    /* Independent expected visible values, beyond differential comparison. */
    PAIR(5);CHECK(contexts[1].used==3);
    {int r=expected_string(0,36,0,":23  ");if(r)return r;r=expected_string(1,36,3,":57  ");if(r)return r;r=expected_string(2,36,6,":900  ");if(r)return r;}
    PAIR(6);CHECK(contexts[1].used==3);
    {int r=expected_string(0,36,0,":456 ");if(r)return r;}
    PAIR(7);CHECK(contexts[1].used==4);
    {int r=expected_string(0,72,0,":61  ");if(r)return r;r=expected_string(3,36,6,":0.5  ");if(r)return r;}
    /* Independent literal expectations for four-row values, units and selected cursor. */
    runtime.ldr_low=0;runtime.ldr_high=9999;runtime.temp_threshold=100;runtime.temp_low_threshold=0;
    Monitor_para1=6;PAIR(3);PAIR(8);
    {int r=expected_string(1,48,0,"   0 ");if(r)return r;r=expected_string(3,48,2,"9999 ");if(r)return r;
     r=expected_string(5,48,4,"100C ");if(r)return r;r=expected_string(7,48,6,"  0C ");if(r)return r;}
    runtime.temp_threshold=30;runtime.temp_low_threshold=20;Monitor_para1=2;PAIR(8);
    {int r=expected_string(5,48,4," 30C ");if(r)return r;r=expected_string(7,48,6," 20C ");if(r)return r;}
    for(i=0;i<7;++i){Monitor_para1=(uint8_t)i;PAIR(8);}
    /* Page5 independent values and cursor selection, including shortened strings. */
    runtime.soil_low=0;runtime.soil_high=100;runtime.co2_threshold=4000;Monitor_para1=3;PAIR(4);PAIR(9);
    {int r=expected_string(1,48,0,"  0% ");if(r)return r;r=expected_string(3,48,2,"100% ");if(r)return r;
     r=expected_string(5,48,4,"4000 ");if(r)return r;}
    runtime.soil_low=30;runtime.soil_high=75;runtime.co2_threshold=350;Monitor_para1=5;PAIR(9);
    {int r=expected_string(1,48,0," 30% ");if(r)return r;r=expected_string(3,48,2," 75% ");if(r)return r;
     r=expected_string(5,48,4," 350 ");if(r)return r;}
    for(i=0;i<8;++i){Monitor_para1=(uint8_t)i;PAIR(9);}
    /* Every valid page crossed with valid LED/pump mode combinations. */
    for(i=0;i<6;++i)for(j=0;j<3;++j)for(k=0;k<3;++k){CurrentScreen=(DisplayScreen)i;runtime.led_mode=(uint8_t)j;runtime.bump_mode=(uint8_t)k;PAIR(10);}
    CurrentScreen=(DisplayScreen)-1;PAIR(10);CHECK(contexts[1].used==9);
    CurrentScreen=(DisplayScreen)255;PAIR(10);CHECK(contexts[1].used==9);
    for(i=0;i<1024;++i){
        rng=rng*1664525U+1013904223U;
        sensors.temperature_c=(uint8_t)(rng%256U);sensors.humidity_pct=(uint8_t)((rng>>8)%101U);
        sensors.light_lux=(uint16_t)(rng>>16);sensors.soil_moisture_pct=(uint16_t)((rng>>9)%101U);
        sensors.co2_ppm=(uint16_t)rng;sensors.valid_mask=(uint8_t)(i&11U);
        runtime.led_mode=(uint8_t)(i%4U);runtime.bump_mode=(uint8_t)((i/4U)%4U);
        runtime.ldr_low=(uint16_t)(rng%10000U);runtime.ldr_high=(uint16_t)((rng>>3)%10000U);
        runtime.temp_threshold=(uint8_t)(rng%101U);runtime.temp_low_threshold=(uint8_t)((rng>>7)%100U);Monitor_para1=(uint8_t)(i%8U);runtime.soil_low=(uint16_t)((rng>>4)%101U);runtime.soil_high=(uint16_t)((rng>>6)%101U);runtime.co2_threshold=(uint16_t)(350U+rng%3651U);
        water_vol=volumes[i%7U];CurrentScreen=(DisplayScreen)(i%7U);PAIR(i%13U);
    }
    text("PASS: full display assertions: ");number(checks);text("; verified page cases: ");number(cases);text("\n");return 0;
}
'''
    prototypes='\n'.join('void Legacy_'+n+'(void);' for n in names if n!='bufdat')
    harness=harness.replace('LEGACY_PROTOTYPES',prototypes)
    test=out / 'test.c';test.write_text(harness,encoding='utf-8')
    exe=out / 'test.exe'
    run([a.clang,*flags,'-fuse-ld=lld','-nostdlib','-Wl,/entry:main,/subsystem:console',str(test),str(production),str(old),str(ucrt),str(kernel),'-o',str(exe)])
    r=run([str(exe)]);print(r.stdout+r.stderr,end='')
print('PASS: full production Display.c, real host CRT formatting, unchanged pages/modes versus frozen source; both parameter pages have independent cursor/value/layout checks; coordinates/glyphs/text/order and read-only state checked. No hardware/network.')

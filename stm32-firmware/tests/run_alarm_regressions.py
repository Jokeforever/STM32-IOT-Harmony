from pathlib import Path
import argparse,re,shutil,subprocess,sys,tempfile

p=argparse.ArgumentParser(description='Portable alarm rules and full beep adapter versus frozen legacy judgment/output; no hardware/network.')
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
with tempfile.TemporaryDirectory(prefix='stm32-alarms-') as td:
    out=Path(td);inc=out / 'include';inc.mkdir();public=out / 'public';public.mkdir()
    def header(name,value):(inc / name).write_text(value,encoding='utf-8')
    for name,rel in [('mw_alarm.h','modules/middleware/inc/mw_alarm.h'),('sensor_data.h','modules/middleware/inc/sensor_data.h')]:
        value=read(rel);header(name,value);(public / name).write_text(value,encoding='utf-8')
    core=out / 'mw_alarm.c';core.write_text(read('modules/middleware/src/mw_alarm.c'),encoding='utf-8')
    flags=['--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-O0','-Wall','-Wextra','-Werror']
    api=public / 'api.c';api.write_text('#include "mw_alarm.h"\nAlarmThresholds limits;Abnormal states[ALARM_COUNT];\n',encoding='utf-8')
    run([a.clang,*flags,'-fsyntax-only','-I'+str(public),str(api),str(core)])
    run([a.clang,*flags,'-x','c++','-fsyntax-only','-I'+str(public),str(api)])
    assert not re.search(r'\b(?:HAL_[A-Za-z0-9_]+|AppConfig_[A-Za-z0-9_]+|OLED_[A-Za-z0-9_]+)\b',read('modules/middleware/src/mw_alarm.c'))
    for name,rel in [('main.h','Core/Inc/main.h'),('beep.h','Core/Inc/beep.h'),('Display.h','Core/Inc/Display.h'),('LED.h','Core/Inc/LED.h'),('bump.h','Core/Inc/bump.h'),('fan.h','Core/Inc/fan.h'),('jdq.h','Core/Inc/jdq.h'),('oled.h','Core/Inc/oled.h'),('mw_pid.h','modules/middleware/inc/mw_pid.h'),('mw_led_port.h','modules/middleware/inc/mw_led_port.h'),('app_config.h','modules/app/inc/app_config.h'),('app_sensors.h','modules/app/inc/app_sensors.h')]:
        header(name,read(rel))
    header('mw_alarm_port.h',read('modules/middleware/inc/mw_alarm_port.h'))
    header('beep_before_alarm_port.h',read('tests/fixtures/beep_before_alarm_port.h'))
    header('gpio.h','#include \"stm32f1xx_hal.h\"\n')
    header('TS.h','/* Unused ADC boundary; alarms only consume SensorData. */\n')
    header('usart3.h','/* Unused UART boundary; CO2 validity comes from SensorData. */\n')
    header('stdio.h','/* Renderer prototypes only; alarm adapter must not format output. */\n')
    header('stm32f1xx_hal.h','''#ifndef MOCK_HAL_H
#define MOCK_HAL_H
#include <stdint.h>
#define GPIOB 1U
#define GPIOC 2U
#define GPIO_PIN_12 (1U<<12)
#define GPIO_PIN_13 (1U<<13)
#define GPIO_PIN_RESET 0U
#define GPIO_PIN_SET 1U
void HAL_GPIO_WritePin(unsigned port,unsigned pin,unsigned value);
#endif
''')
    production=out / 'beep.c';production.write_text(read('Core/Src/beep.c'),encoding='utf-8')
    fixture=out / 'alarm_before_extract.c';fixture.write_text(read('tests/fixtures/alarm_before_extract.c').replace('#include \"beep.h\"','#include \"beep_before_alarm_port.h\"',1),encoding='utf-8')
    assert not re.search(r'#include\s+"Display.h"|\bDisplayScreen\b',read('Core/Src/beep.c'))
    port=out / 'app_alarm_port.c';port.write_text(read('modules/app/src/app_alarm_port.c'),encoding='utf-8')
    old_obj=out / 'old.obj'
    names=['Beep_Init','IS_Normal','Handler','abnormal']
    run([a.clang,*flags,'-I'+str(inc),*['-D'+n+'=Legacy_'+n for n in names],'-c',str(fixture),'-o',str(old_obj)])
    harness=r'''
#include "mw_alarm.h"
#include "beep.h"
#include "Display.h"
#include "app_config.h"
#include "app_sensors.h"
volatile Abnormal abnormal[3],Legacy_abnormal[3];
volatile DisplayScreen CurrentScreen=DISPLAY_NONE;
void Legacy_Beep_Init(void);void Legacy_IS_Normal(void);void Legacy_Handler(void);
static RuntimeConfig config={2,0,50,25,2000,100,300,30,75,200,20,1500,0,0,20};
static SensorData data={25,50,200,50,1000,11};
typedef struct {unsigned port,pin,value;} Event;
typedef struct {unsigned used,bad,config_reads,sensor_reads;Event events[4];} Context;
static Context contexts[2];static unsigned active,checks,cases;
__declspec(dllimport) void *GetStdHandle(uint32_t);
__declspec(dllimport) int WriteFile(void*,const void*,uint32_t,uint32_t*,void*);
static void text(const char *s){uint32_t n=0,w;while(s[n])++n;WriteFile(GetStdHandle((uint32_t)-11),s,n,&w,0);}
static void number(unsigned n){char b[16];unsigned i=15;b[i]=0;do{b[--i]=(char)('0'+n%10);n/=10;}while(n);text(b+i);}
#define CHECK(x) do{++checks;if(!(x)){text("FAIL alarm test line ");number(__LINE__);text("\n");return __LINE__;}}while(0)
#define PAIR(expected) do{int r=pair(expected);if(r)return r;}while(0)
const RuntimeConfig *AppConfig_Get(void){++contexts[active].config_reads;return &config;}
const SensorData *SensorService_GetData(void){++contexts[active].sensor_reads;return &data;}
void HAL_GPIO_WritePin(unsigned port,unsigned pin,unsigned value){
    Context *c=&contexts[active];Event *e;
    if(c->used>=4){c->bad=1;return;}
    if(!((port==GPIOB&&pin==GPIO_PIN_12)||(port==GPIOC&&pin==GPIO_PIN_13)))c->bad=1;
    e=&c->events[c->used++];e->port=port;e->pin=pin;e->value=value;
}
static void clear(unsigned n){contexts[n].used=contexts[n].bad=contexts[n].config_reads=contexts[n].sensor_reads=0;}
static uint32_t hash(const void *p,unsigned n){const unsigned char *s=p;uint32_t h=2166136261U;while(n--){h^=*s++;h*=16777619U;}return h;}
static int pair(const Abnormal *expected){
    unsigned i;AlarmThresholds limits;Abnormal direct[3];
    uint32_t config_before=hash(&config,sizeof(config)),data_before=hash(&data,sizeof(data));
    limits.light_low=config.ldr_low;limits.light_high=config.ldr_high;limits.soil_low=config.soil_low;limits.soil_high=config.soil_high;limits.co2_threshold=config.co2_threshold;
    for(i=0;i<3;++i){abnormal[i]=(Abnormal)66;Legacy_abnormal[i]=(Abnormal)77;direct[i]=(Abnormal)88;}
    CHECK(Alarm_Evaluate(&data,&limits,direct)==1);
    clear(0);active=0;Legacy_IS_Normal();clear(1);active=1;IS_Normal();++cases;
    CHECK(hash(&config,sizeof(config))==config_before);CHECK(hash(&data,sizeof(data))==data_before);
    for(i=0;i<3;++i){CHECK(abnormal[i]==Legacy_abnormal[i]);CHECK(direct[i]==Legacy_abnormal[i]);if(expected){CHECK(direct[i]==expected[i]);}}
    CHECK(!contexts[0].bad&&!contexts[1].bad);CHECK(contexts[0].used==2);CHECK(contexts[1].used==2);
    CHECK(contexts[0].sensor_reads==1);CHECK(contexts[1].sensor_reads==1);CHECK(contexts[1].config_reads==1);
    for(i=0;i<2;++i){CHECK(contexts[0].events[i].port==contexts[1].events[i].port);CHECK(contexts[0].events[i].pin==contexts[1].events[i].pin);CHECK(contexts[0].events[i].value==contexts[1].events[i].value);}
    CHECK(contexts[1].events[0].port==GPIOC&&contexts[1].events[0].pin==GPIO_PIN_13);
    CHECK(contexts[1].events[0].value==(direct[0]==NONE?GPIO_PIN_SET:GPIO_PIN_RESET));
    CHECK(contexts[1].events[1].port==GPIOB&&contexts[1].events[1].pin==GPIO_PIN_12);
    CHECK(contexts[1].events[1].value==(direct[1]==NONE?GPIO_PIN_SET:GPIO_PIN_RESET));
    return 0;
}
int main(void){
    unsigned i,j,k,v,rng=0x597AC12U;Abnormal expected[3],out[3];
    const uint16_t light_values[]={99,100,200,300,301};
    const uint16_t soil_values[]={29,30,50,75,76};
    const uint16_t co2_values[]={1999,2000,2001};
    const Abnormal light_results[]={DARK,NONE,NONE,NONE,BRIGHT};
    const Abnormal soil_results[]={DROUGHT,NONE,NONE,NONE,DAMP};
    AlarmThresholds limits={100,300,30,75,2000};
    CHECK(NONE==0&&BRIGHT==1&&DARK==2&&DAMP==3&&DROUGHT==4&&HIGHCO2==5);
    CHECK(ALARM_LIGHT_INDEX==0&&ALARM_SOIL_INDEX==1&&ALARM_CO2_INDEX==2&&ALARM_COUNT==3);
    for(i=0;i<5;++i)for(j=0;j<5;++j)for(k=0;k<3;++k)for(v=0;v<2;++v){
        data.light_lux=light_values[i];data.soil_moisture_pct=soil_values[j];data.co2_ppm=co2_values[k];
        data.valid_mask=(uint8_t)(v?SENSOR_VALID_CO2:(SENSOR_VALID_TEMP|SENSOR_VALID_HUMI));
        expected[0]=light_results[i];expected[1]=soil_results[j];expected[2]=(v&&k==2)?HIGHCO2:NONE;PAIR(expected);
    }
    /* Representable endpoints and high-first precedence, even for reversed limits. */
    config.ldr_low=65535;config.ldr_high=0;config.soil_low=65535;config.soil_high=0;config.co2_threshold=65535;
    data.light_lux=1;data.soil_moisture_pct=1;data.co2_ppm=65535;data.valid_mask=255;
    expected[0]=BRIGHT;expected[1]=DAMP;expected[2]=NONE;PAIR(expected);
    config.ldr_low=config.ldr_high=0;config.soil_low=config.soil_high=0;config.co2_threshold=0;
    data.light_lux=data.soil_moisture_pct=data.co2_ppm=0;
    expected[0]=expected[1]=expected[2]=NONE;PAIR(expected);
    data.light_lux=data.soil_moisture_pct=data.co2_ppm=65535;
    expected[0]=BRIGHT;expected[1]=DAMP;expected[2]=HIGHCO2;PAIR(expected);
    /* Many successive inputs exercise mixed alarms and clearing stale states. */
    for(i=0;i<4096;++i){
        rng=rng*1664525U+1013904223U;
        data.light_lux=(uint16_t)rng;data.soil_moisture_pct=(uint16_t)(rng>>16);data.co2_ppm=(uint16_t)(rng>>8);data.valid_mask=(uint8_t)i;
        config.ldr_low=(uint16_t)(rng>>4);config.ldr_high=(uint16_t)(rng>>12);
        config.soil_low=(uint16_t)(rng>>2);config.soil_high=(uint16_t)(rng>>14);config.co2_threshold=(uint16_t)(rng>>3);
        PAIR(0);
    }
    /* Null inputs cannot partially overwrite caller results. */
    out[0]=BRIGHT;out[1]=DROUGHT;out[2]=HIGHCO2;
    CHECK(Alarm_Evaluate(0,&limits,out)==0);CHECK(out[0]==BRIGHT&&out[1]==DROUGHT&&out[2]==HIGHCO2);
    CHECK(Alarm_Evaluate(&data,0,out)==0);CHECK(out[0]==BRIGHT&&out[1]==DROUGHT&&out[2]==HIGHCO2);
    CHECK(Alarm_Evaluate(&data,&limits,0)==0);
    /* Handler still only drives the two original outputs, in original order. */
    for(i=0;i<6;++i)for(j=0;j<6;++j)for(k=0;k<6;++k){
        abnormal[0]=Legacy_abnormal[0]=(Abnormal)i;abnormal[1]=Legacy_abnormal[1]=(Abnormal)j;abnormal[2]=Legacy_abnormal[2]=(Abnormal)k;
        clear(0);active=0;Legacy_Handler();clear(1);active=1;Handler();
        CHECK(contexts[0].used==2&&contexts[1].used==2);CHECK(!contexts[0].bad&&!contexts[1].bad);
        for(v=0;v<2;++v){CHECK(contexts[0].events[v].port==contexts[1].events[v].port);CHECK(contexts[0].events[v].pin==contexts[1].events[v].pin);CHECK(contexts[0].events[v].value==contexts[1].events[v].value);}
        CHECK(contexts[1].events[0].value==(i?GPIO_PIN_RESET:GPIO_PIN_SET));CHECK(contexts[1].events[1].value==(j?GPIO_PIN_RESET:GPIO_PIN_SET));
    }
    clear(0);active=0;Legacy_Beep_Init();clear(1);active=1;Beep_Init();
    CHECK(contexts[0].used==1&&contexts[1].used==1);
    CHECK(contexts[1].events[0].port==GPIOB&&contexts[1].events[0].pin==GPIO_PIN_12&&contexts[1].events[0].value==GPIO_PIN_SET);
    CHECK(contexts[0].events[0].port==contexts[1].events[0].port&&contexts[0].events[0].pin==contexts[1].events[0].pin&&contexts[0].events[0].value==contexts[1].events[0].value);
    text("PASS: alarm rule/integration assertions: ");number(checks);text("; legacy/current judgment cases: ");number(cases);text("\n");return 0;
}
'''
    test=out / 'test.c';test.write_text(harness,encoding='utf-8')
    exe=out / 'test.exe'
    run([a.clang,*flags,'-I'+str(inc),'-fuse-ld=lld','-nostdlib','-Wl,/entry:main,/subsystem:console',str(test),str(production),str(core),str(port),str(old_obj),str(kernel[-1]),'-o',str(exe)])
    r=run([str(exe)]);print(r.stdout+r.stderr,end='')
print('PASS: independent threshold/equality/validity vectors, frozen/current adapter and GPIO trace, mixed alarms/recovery, numeric/null boundaries and HAL-free rule compilation. No hardware/network.')

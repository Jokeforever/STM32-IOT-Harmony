from pathlib import Path
import argparse,re,shutil,subprocess,sys,tempfile

p=argparse.ArgumentParser(description='Actual LED and F103 port against frozen pre-port implementation; duty conversion and public-header checks.')
p.add_argument('--firmware-root',type=Path,default=Path(__file__).resolve().parents[1])
p.add_argument('--overlay',type=Path)
p.add_argument('--clang',default=shutil.which('clang') or r'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe')
p.add_argument('--windows-sdk',type=Path,default=Path(r'C:\Program Files (x86)\Windows Kits\10'))
a=p.parse_args()
def read(rel):
    candidate=a.overlay/rel if a.overlay else a.firmware_root/rel
    return (candidate if candidate.exists() else a.firmware_root/rel).read_text(encoding='utf-8')
kernel=sorted((a.windows_sdk/'Lib').glob('*/um/x64/kernel32.lib'))
if not kernel:sys.exit('Windows SDK kernel32.lib is required; use --windows-sdk.')
with tempfile.TemporaryDirectory(prefix='stm32-led-port-') as td:
    out=Path(td);inc=out/'include';inc.mkdir();public=out/'public';public.mkdir()
    def header(name,value):(inc/name).write_text(value,encoding='utf-8')
    for name,rel in [('LED.h','Core/Inc/LED.h'),('mw_led_port.h','modules/middleware/inc/mw_led_port.h'),('mw_pid.h','modules/middleware/inc/mw_pid.h'),('app_config.h','modules/app/inc/app_config.h')]:
        value=read(rel);header(name,value)
        if name!='app_config.h':(public/name).write_text(value,encoding='utf-8')
    header('led_before_port.h',read('tests/fixtures/led_before_port.h'))
    header('stm32f1xx_hal.h',r'''
#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
typedef struct {uint32_t ARR;} TIM_TypeDef;
typedef struct {TIM_TypeDef *Instance;} TIM_HandleTypeDef;
typedef enum {HAL_OK=0,HAL_ERROR=1} HAL_StatusTypeDef;
extern TIM_HandleTypeDef htim3;
#define TIM_CHANNEL_2 4U
#define GPIOA 0U
#define GPIO_PIN_7 (1U<<7)
#define GPIO_PIN_RESET 0U
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef*,uint32_t);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef*,uint32_t);
void HAL_GPIO_WritePin(unsigned,unsigned,unsigned);
void mock_compare(TIM_HandleTypeDef*,uint32_t,uint32_t);
#define __HAL_TIM_GET_AUTORELOAD(h) ((h)->Instance->ARR)
#define __HAL_TIM_SET_COMPARE(h,c,v) mock_compare(h,c,v)
#endif
''')
    for name in ['Key.h','tim.h','gpio.h']:header(name,'#include "stm32f1xx_hal.h"\n')
    header('oled.h','void OLED_ShowCHinese(int x,int y,int symbol);\n')
    header('math.h','static inline float fabsf(float v){return v<0?-v:v;}\n')
    header('stddef.h','typedef __SIZE_TYPE__ size_t;\n#define NULL ((void*)0)\n')
    api=public/'header_check.c'
    api.write_text('#include "LED.h"\n#include "mw_led_port.h"\nLED_Control led;\nLedPortResult result;\n',encoding='utf-8')
    cmd=[a.clang,'--target=x86_64-pc-windows-msvc','-ffreestanding','-fsyntax-only','-Wall','-Wextra','-Werror','-I'+str(public),str(api)]
    r=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if r.returncode:print(r.stdout+r.stderr);sys.exit(r.returncode)
    assert not re.search(r'#include\s+[<"](?:stm32|Key|oled)',read('Core/Inc/LED.h'))
    assert not re.search(r'\b(?:HAL_[A-Za-z0-9_]+|__HAL_[A-Za-z0-9_]+|htim3|TIM_CHANNEL_2)\b',read('Core/Src/LED.c'))
    harness=r'''
#include "LED.h"
#include "stm32f1xx_hal.h"
#include "app_config.h"
int _fltused=0;
static uint32_t tick;
static RuntimeConfig runtime={2,0,50,25,2000,100,1000,30,75,200,20,1500,0,0};
const RuntimeConfig *AppConfig_Get(void){return &runtime;}
static unsigned set_calls;static AppConfigKey set_key;static int32_t set_value;
AppConfigResult AppConfig_Set(AppConfigKey key,int32_t value){++set_calls;set_key=key;set_value=value;return APP_CONFIG_UNCHANGED;}
typedef struct {unsigned kind,a,b,c;} Event;
typedef struct {Event events[32];unsigned used,overflow,compare,running,level;} Context;
static Context contexts[2];static unsigned active,checks,bad_boundary;
static HAL_StatusTypeDef start_result=HAL_OK,stop_result=HAL_OK;
static TIM_TypeDef timer={999};TIM_HandleTypeDef htim3={&timer};
__declspec(dllimport) void *GetStdHandle(uint32_t);
__declspec(dllimport) int WriteFile(void*,const void*,uint32_t,uint32_t*,void*);
static void text(const char *s){uint32_t n=0,w;while(s[n])++n;WriteFile(GetStdHandle((uint32_t)-11),s,n,&w,0);}
static void number(unsigned n){char b[16];unsigned i=15;b[i]=0;do{b[--i]=(char)('0'+n%10);n/=10;}while(n);text(b+i);}
#define CHECK(c) do{++checks;if(!(c)){text("FAIL LED test line ");number(__LINE__);text("\n");return __LINE__;}}while(0)
static void event(unsigned kind,unsigned a,unsigned b,unsigned c){Context *x=&contexts[active];Event *e;if(x->used>=32){x->overflow=1;return;}e=&x->events[x->used++];e->kind=kind;e->a=a;e->b=b;e->c=c;}
uint32_t HAL_GetTick(void){event(1,tick,0,0);return tick;}
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *h,uint32_t channel){if(h!=&htim3||channel!=TIM_CHANNEL_2)bad_boundary=1;event(2,channel,0,0);if(start_result==HAL_OK)contexts[active].running=1;return start_result;}
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *h,uint32_t channel){if(h!=&htim3||channel!=TIM_CHANNEL_2)bad_boundary=1;event(3,channel,0,0);if(stop_result==HAL_OK)contexts[active].running=0;return stop_result;}
void HAL_GPIO_WritePin(unsigned port,unsigned pin,unsigned value){if(port!=GPIOA||pin!=GPIO_PIN_7)bad_boundary=1;event(4,port,pin,value);contexts[active].level=value;}
void mock_compare(TIM_HandleTypeDef *h,uint32_t channel,uint32_t value){if(h!=&htim3||channel!=TIM_CHANNEL_2)bad_boundary=1;event(5,channel,value,0);contexts[active].compare=value;}
void OLED_ShowCHinese(int x,int y,int glyph){event(6,(unsigned)x,(unsigned)y,(unsigned)glyph);}
extern LED_Control Legacy_led_ctrl;extern uint16_t ccr,Legacy_ccr;
void Legacy_LED_Init(void);void Legacy_LED_Set(int);void Legacy_LED_PID_Reset(void);void Legacy_LED_Update(int16_t);
void Legacy_LED_SetPWM(int16_t);void Legacy_Mode_Switch(void);void Legacy_LED_Func(void);void Legacy_LED_Display(void);
static void invoke(unsigned old,unsigned op,int16_t value){
    active=old?0:1;
    if(old){switch(op){case 0:Legacy_LED_Init();break;case 1:Legacy_Mode_Switch();break;case 2:Legacy_LED_Update(value);break;case 3:Legacy_LED_Func();break;case 4:Legacy_LED_SetPWM(value);break;case 5:Legacy_LED_PID_Reset();break;case 6:Legacy_LED_Display();break;default:Legacy_LED_Set(value);break;}}
    else{switch(op){case 0:LED_Init();break;case 1:Mode_Switch();break;case 2:LED_Update(value);break;case 3:LED_Func();break;case 4:LED_SetPWM(value);break;case 5:LED_PID_Reset();break;case 6:LED_Display();break;default:LED_Set(value);break;}}
}
static int pair(unsigned op,int16_t value){
    unsigned i;
    contexts[0].used=contexts[1].used=0;contexts[0].overflow=contexts[1].overflow=0;
    invoke(1,op,value);invoke(0,op,value);
    CHECK(!contexts[0].overflow&&!contexts[1].overflow);CHECK(contexts[0].used==contexts[1].used);
    for(i=0;i<contexts[0].used;i++){
        const Event *a=&contexts[0].events[i],*b=&contexts[1].events[i];
        CHECK(a->kind==b->kind);CHECK(a->a==b->a);CHECK(a->b==b->b);CHECK(a->c==b->c);
    }
    CHECK(contexts[0].compare==contexts[1].compare);CHECK(contexts[0].running==contexts[1].running);CHECK(contexts[0].level==contexts[1].level);
    CHECK(led_ctrl.enabled==Legacy_led_ctrl.enabled);CHECK(led_ctrl.current_lux==Legacy_led_ctrl.current_lux);CHECK(led_ctrl.pwm_value==Legacy_led_ctrl.pwm_value);
    CHECK(led_ctrl.pid.integral==Legacy_led_ctrl.pid.integral);CHECK(led_ctrl.pid.last_error==Legacy_led_ctrl.pid.last_error);
    CHECK(led_ctrl.pid.last_tick==Legacy_led_ctrl.pid.last_tick);CHECK(led_ctrl.pid.initialized==Legacy_led_ctrl.pid.initialized);
    CHECK(ccr==Legacy_ccr);CHECK(!bad_boundary);return 0;
}
int main(void){
    static const uint32_t intervals[]={0,1,999,1000,5000,5001,10000};
    static const int16_t pwm_values[]={-32768,-1,0,1,300,999,1000,1001,32767};
    static const uint32_t arr_values[]={0,1,9,99,999,9999,65534};
    static const uint32_t bad_arr_values[]={65535,65536,0xFFFFFFFFU};
    unsigned i,j,rng=0x12345678U,old_writes;uint16_t saved_pwm;uint32_t expected;
    tick=0xFFFFFF00U;CHECK(pair(0,0)==0);
    for(i=0;i<3;i++){runtime.led_mode=(uint8_t)i;CHECK(pair(1,0)==0);CHECK(pair(6,0)==0);CHECK(pair(3,0)==0);}
    for(i=0;i<sizeof(pwm_values)/sizeof(pwm_values[0]);i++)CHECK(pair(4,pwm_values[i])==0);
    for(i=0;i<4096;i++){
        unsigned op=i%8;rng=rng*1664525U+1013904223U;tick+=intervals[i%7];
        runtime.led_mode=(uint8_t)(i%3);runtime.led_manual_pwm=(uint8_t)(rng%101);
        runtime.pid_target_lux=(int16_t)(rng%1000);runtime.pid_dead_zone=(uint16_t)((rng>>10)%201);
        runtime.pid_kp_x1000=(int32_t)((rng>>12)%10001);runtime.pid_ki_x1000=(int32_t)((rng>>5)%10001);runtime.pid_kd_x1000=(int32_t)((rng>>3)%10001);
        if(i%17==0){led_ctrl.enabled=Legacy_led_ctrl.enabled=(uint8_t)((i/17)%2);}
        CHECK(pair(op,(int16_t)(rng>>16))==0);
    }
    CHECK(set_calls==2*(4096/8));CHECK(set_key==APP_CFG_LED_MODE);
    CHECK(set_value==(int16_t)(rng>>16));
    start_result=HAL_ERROR;stop_result=HAL_ERROR;
    runtime.led_mode=0;CHECK(pair(1,0)==0);runtime.led_mode=2;CHECK(pair(1,0)==0);
    start_result=stop_result=HAL_OK;
    /* 0..1200 permille and multiple real-timer periods; reference uses uint64 arithmetic. */
    active=1;
    for(i=0;i<sizeof(arr_values)/sizeof(arr_values[0]);i++){
        timer.ARR=arr_values[i];
        for(j=0;j<=1200;j++){
            contexts[1].used=contexts[1].overflow=0;
            expected=(uint32_t)(((uint64_t)timer.ARR+1U)*(j>1000?1000:j)/1000U);
            CHECK(LedPort_SetDuty((uint16_t)j)==LED_PORT_OK);CHECK(contexts[1].compare==expected);
            CHECK(contexts[1].used==1);CHECK(contexts[1].events[0].kind==5);CHECK(!bad_boundary);
        }
    }
    for(i=0;i<sizeof(bad_arr_values)/sizeof(bad_arr_values[0]);i++){
        timer.ARR=bad_arr_values[i];contexts[1].used=0;old_writes=contexts[1].compare;
        CHECK(LedPort_SetDuty(1000)==LED_PORT_INVALID_PERIOD);CHECK(contexts[1].used==0);CHECK(contexts[1].compare==old_writes);
        saved_pwm=led_ctrl.pwm_value;LED_SetPWM(300);CHECK(led_ctrl.pwm_value==saved_pwm);CHECK(contexts[1].used==0);
    }
    timer.ARR=9999;contexts[1].used=0;LED_SetPWM(300);CHECK(led_ctrl.pwm_value==300);CHECK(contexts[1].compare==3000);
    LED_SetPWM(-1);CHECK(led_ctrl.pwm_value==0);CHECK(contexts[1].compare==0);
    LED_SetPWM(32767);CHECK(led_ctrl.pwm_value==1000);CHECK(contexts[1].compare==10000);
    contexts[1].used=0;LedPort_Enable();LedPort_Disable();CHECK(contexts[1].used==3);
    CHECK(contexts[1].events[0].kind==2&&contexts[1].events[1].kind==3&&contexts[1].events[2].kind==4);
    CHECK(contexts[1].events[2].b==GPIO_PIN_7&&contexts[1].events[2].c==0);
    tick=0xFFFFFFFFU;CHECK(LedPort_NowMs()==tick);tick=0;CHECK(LedPort_NowMs()==0);CHECK(!bad_boundary);
    text("PASS: LED port differential and mapping assertions: ");number(checks);text("\n");return 0;
}
'''
    (out/'test.c').write_text(harness,encoding='utf-8')
    sources=[]
    for rel in ['Core/Src/LED.c','modules/app/src/app_led_port.c','modules/middleware/src/mw_pid.c']:
        file=out/Path(rel).name;file.write_text(read(rel),encoding='utf-8');sources.append(str(file))
    fixture=out/'led_before_port.c';fixture.write_text(read('tests/fixtures/led_before_port.c'),encoding='utf-8')
    names=['LED_Set','LED_Init','LED_PID_Reset','LED_Update','LED_SetPWM','Mode_Switch','LED_Func','LED_Display','led_ctrl','ccr']
    obj=out/'legacy.obj'
    flags=['--target=x86_64-pc-windows-msvc','-ffreestanding','-fno-builtin','-O0','-Wall','-Wextra','-Werror','-I'+str(inc)]
    r=subprocess.run([a.clang,*flags,*['-D'+name+'=Legacy_'+name for name in names],'-c',str(fixture),'-o',str(obj)],capture_output=True,text=True,encoding='utf-8',errors='replace')
    if r.returncode:print(r.stdout+r.stderr);sys.exit(r.returncode)
    cmd=[a.clang,*flags,'-fuse-ld=lld','-nostdlib','-Wl,/entry:main,/subsystem:console',str(out/'test.c'),*sources,str(obj),str(kernel[-1]),'-o',str(out/'test.exe')]
    r=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if r.returncode:print(r.stdout+r.stderr);sys.exit(r.returncode)
    r=subprocess.run([str(out/'test.exe')],capture_output=True,text=True,encoding='utf-8',errors='replace')
    print(r.stdout+r.stderr,end='')
    if r.returncode:sys.exit(1)
    print('PASS: 4096 frozen/current LED steps, HAL event order/state, duty conversion, invalid-period guard, time wrap and HAL-free public headers. No hardware/network.')

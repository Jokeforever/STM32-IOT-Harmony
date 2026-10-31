from pathlib import Path
import subprocess
import sys

import argparse
import shutil
import tempfile
parser = argparse.ArgumentParser(description='Windows native regression tests; no hardware required.')
parser.add_argument('--clang', default=shutil.which('clang') or r'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe')
args = parser.parse_args()
REAL = Path(__file__).resolve().parents[1]
STAGED = REAL
CLANG = Path(args.clang)
temporary = tempfile.TemporaryDirectory(prefix='stm32-control-tests-')
OUT = Path(temporary.name)
INC = OUT / 'include'
INC.mkdir(parents=True, exist_ok=True)

def read(relative):
    p = STAGED / relative
    return (p if p.exists() else REAL / relative).read_text(encoding='utf-8')

def header(name, text):
    (INC / name).write_text(text, encoding='utf-8')

header('stm32f1xx_hal.h', r'''
#ifndef MOCK_HAL_H
#define MOCK_HAL_H
#include <stdint.h>
typedef struct { int unused; } TIM_HandleTypeDef;
extern TIM_HandleTypeDef htim3;
#define GPIOA 0
#define GPIOB 1
#define GPIOC 2
#define GPIO_PIN_5 (1U << 5)
#define GPIO_PIN_6 (1U << 6)
#define GPIO_PIN_7 (1U << 7)
#define GPIO_PIN_12 (1U << 12)
#define GPIO_PIN_13 (1U << 13)
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define TIM_CHANNEL_2 2
uint32_t HAL_GetTick(void);
void HAL_GPIO_WritePin(int port, unsigned pin, int value);
void HAL_TIM_PWM_Start(TIM_HandleTypeDef *h, int channel);
void HAL_TIM_PWM_Stop(TIM_HandleTypeDef *h, int channel);
void mock_compare(unsigned value);
#define __HAL_TIM_SET_COMPARE(h, c, v) mock_compare(v)
#endif
''')
for name in ('main.h', 'fan.h', 'jdq.h', 'beep.h'):
    header(name, read('Core/Inc/' + name))
header('LED.h', read('Core/Inc/LED.h').replace('#include "Key.h"', '#include "Key.h"\n#include "stm32f1xx_hal.h"'))
header('Key.h', '#ifndef MOCK_KEY_H\n#define MOCK_KEY_H\ntypedef struct { int unused; } Key;\n#endif\n')
header('oled.h', '#ifndef MOCK_OLED_H\n#define MOCK_OLED_H\nvoid OLED_ShowCHinese(int x, int y, int symbol);\n#endif\n')
header('usart3.h', '#include <stdint.h>\n#define CO2_THREHOLD 2000\n')
header('bump.h', '''#ifndef MOCK_BUMP_H
#define MOCK_BUMP_H
#include <stdint.h>
typedef enum { Bump_OFF=0, Bump_CALIBRATION=1, Bump_AUTO=2 } Bump_Mode;
#define HUMI_THRESHOLD_LOW 30
#define HUMI_THRESHOLD_HIGH 75
typedef enum { NONE=0, BRIGHT, DARK, DAMP, DROUGHT, HIGHCO2 } Abnormal;
typedef int DisplayScreen;
#endif
''')
header('mw_config.h', read('modules/middleware/inc/mw_config.h'))
header('i2c.h', '''#include "stm32f1xx_hal.h"
uint8_t EEPROM24C02_Read(uint8_t addr, uint8_t *buf, uint16_t len);
uint8_t EEPROM24C02_Write(uint8_t addr, const uint8_t *buf, uint16_t len);
''')
header('math.h', 'static inline float fabsf(float x) { return x < 0 ? -x : x; }\n')
header('string.h', '#include <stddef.h>\nvoid *memset(void *p, int c, size_t n);\n')

harness = r'''
#include "stm32f1xx_hal.h"
#include "main.h"
#include "LED.h"
#include "bump.h"
#include "jdq.h"
#include "beep.h"
#include "mw_config.h"
#include <stddef.h>
TIM_HandleTypeDef htim3;
Key Keys[4];
uint8_t temp=25, humi=40, sensor_valid=11, LED_Manu_PWM=50;
uint16_t ppm=350, moist=50, light=200, CO2_Threhold=2000, Bump_threhold[2]={30,75};
Bump_Mode CurrentBump_mode=Bump_OFF, LastBump_mode=Bump_OFF;
extern System_Mode current_mode, last_mode;
extern uint8_t temp_threhold;
extern uint16_t LDR_threhold[2];
volatile Abnormal abnormal[3];
volatile DisplayScreen CurrentScreen;
static uint32_t tick;
static unsigned pwm;
static int pins[3][16];
static uint8_t eeprom[256];
int _fltused = 0;
uint32_t HAL_GetTick(void) { return tick; }
void HAL_GPIO_WritePin(int port, unsigned mask, int value) {
    unsigned i; for (i=0;i<16;i++) if (mask & (1U<<i)) pins[port][i]=value;
}
void HAL_TIM_PWM_Start(TIM_HandleTypeDef *h, int c) { (void)h; (void)c; }
void HAL_TIM_PWM_Stop(TIM_HandleTypeDef *h, int c) { (void)h; (void)c; }
void mock_compare(unsigned v) { pwm=v; }
void OLED_ShowCHinese(int x,int y,int s) { (void)x;(void)y;(void)s; }
void *memset(void *p,int c,size_t n) { uint8_t *b=p; while(n--) *b++=(uint8_t)c; return p; }
void *memcpy(void *p,const void *s,size_t n) { uint8_t *b=p;const uint8_t *a=s;while(n--) *b++=*a++;return p; }
uint8_t EEPROM24C02_Read(uint8_t a,uint8_t *b,uint16_t n) {
    unsigned i; if ((unsigned)a+n>256) return 0; for(i=0;i<n;i++) b[i]=eeprom[a+i]; return 1;
}
uint8_t EEPROM24C02_Write(uint8_t a,const uint8_t *b,uint16_t n) {
    unsigned i; if ((unsigned)a+n>256) return 0; for(i=0;i<n;i++) eeprom[a+i]=b[i]; return 1;
}
#define CHECK(x) do { if (!(x)) return __LINE__; } while(0)
static int near(float a,float b) { float d=a-b; return d>-0.01f && d<0.01f; }
static void pid(float kp,float ki,float kd,uint32_t start,int error) {
    tick=start; LED_Init(); led_ctrl.target_lux=200; led_ctrl.dead_zone=0;
    led_ctrl.k_p=kp;led_ctrl.k_i=ki;led_ctrl.k_d=kd;LED_PID_Reset();
    LED_Update((int16_t)(200-error));
}
int main(void) {
    /* 真正调用生产代码：温度/CO2 的四种组合和后执行报警的覆盖回归。 */
    LDR_threhold[0]=100;LDR_threhold[1]=1000;temp_threhold=25;
    temp=28;ppm=350;sensor_valid=11;Monitor_Temp();IS_Normal();
    CHECK(pins[GPIOB][5]==GPIO_PIN_RESET); CHECK(pins[GPIOB][6]==GPIO_PIN_SET);
    temp=25;ppm=2500;Monitor_Temp();IS_Normal();CHECK(pins[GPIOB][5]==GPIO_PIN_RESET);
    temp=28;ppm=2500;Monitor_Temp();IS_Normal();CHECK(pins[GPIOB][5]==GPIO_PIN_RESET);
    temp=25;ppm=350;Monitor_Temp();IS_Normal();CHECK(pins[GPIOB][5]==GPIO_PIN_SET);
    temp=22;Monitor_Temp();CHECK(pins[GPIOB][6]==GPIO_PIN_RESET);
    temp=28;Monitor_Temp();CHECK(pins[GPIOB][6]==GPIO_PIN_SET);
    temp=22;sensor_valid=0;Monitor_Temp();CHECK(pins[GPIOB][6]==GPIO_PIN_SET);
    temp=28;ppm=2500;Monitor_Temp();IS_Normal();
    CHECK(pins[GPIOB][5]==GPIO_PIN_SET);CHECK(abnormal[2]==NONE);
    sensor_valid=SENSOR_VALID_CO2;Monitor_Temp();CHECK(pins[GPIOB][5]==GPIO_PIN_RESET);
    sensor_valid=SENSOR_VALID_TEMP;ppm=350;temp=26;Monitor_Temp();CHECK(pins[GPIOB][5]==GPIO_PIN_SET);
    temp=24;Monitor_Temp();CHECK(pins[GPIOB][6]==GPIO_PIN_SET);
    temp_threhold=0;temp=0;Monitor_Temp();CHECK(pins[GPIOB][6]==GPIO_PIN_SET);
    temp_threhold=100;temp=100;Monitor_Temp();CHECK(pins[GPIOB][5]==GPIO_PIN_SET);
    /* PID 积分必须使用实际 0.5/1/1.5 秒，而不是全部裁成 0.1 秒。 */
    pid(0,1,0,0,100);tick=1000;LED_Update(100);CHECK(near(led_ctrl.integral,100));CHECK(pwm==100);
    pid(0,1,0,0,100);tick=500;LED_Update(100);CHECK(near(led_ctrl.integral,50));
    pid(0,1,0,0,100);tick=1500;LED_Update(100);CHECK(near(led_ctrl.integral,150));
    pid(0,0,1,0,10);tick=1000;LED_Update(180);CHECK(pwm==10);
    pid(0,1,0,0xFFFFFF00U,100);tick=744;LED_Update(100);CHECK(near(led_ctrl.integral,100));
    pid(0,1,0,0,100);LED_Update(100);CHECK(near(led_ctrl.integral,0));
    pid(0,1,0,0,100);led_ctrl.integral=900;tick=10000;LED_Update(100);
    CHECK(near(led_ctrl.integral,100));CHECK(pwm==100);
    pid(1.5f,0,0,0,100);tick=1000;LED_Update(100);CHECK(pwm==150);
    LED_SetPWM(-50);CHECK(pwm==0);LED_SetPWM(1500);CHECK(pwm==1000);
    /* 空白配置/双槽保存和重读：新的默认值200，已有有效500不被强制覆盖。 */
    memset(eeprom,255,sizeof(eeprom));Config_Load();
    CHECK(led_ctrl.target_lux==200);CHECK(CurrentBump_mode==Bump_OFF);
    tick+=1000;Config_Process();CHECK(Config_IsDirty()==0);
    led_ctrl.target_lux=500;CHECK(Config_Save()!=0);led_ctrl.target_lux=123;Config_Load();
    CHECK(led_ctrl.target_lux==500);
    return 0;
}
'''
(OUT / 'test.c').write_text(harness, encoding='utf-8')
source_rel = ['Core/Src/LED.c','Core/Src/jdq.c','Core/Src/beep.c','modules/middleware/src/mw_config.c']
sources = []
for rel in source_rel:
    p = OUT / Path(rel).name
    p.write_text(read(rel), encoding='utf-8')
    sources.append(str(p))
cmd = [str(CLANG), '--target=x86_64-pc-windows-msvc', '-ffreestanding', '-fno-builtin',
       '-fuse-ld=lld', '-nostdlib', '-O0', '-Werror', '-Wall', '-Wextra', '-Wno-misleading-indentation',
       '-Wl,/entry:main,/subsystem:console', '-I'+str(INC), str(OUT/'test.c'), *sources,
       '-o',str(OUT/'test.exe')]
r = subprocess.run(cmd, capture_output=True, text=True)
print(r.stdout+r.stderr)
if r.returncode:sys.exit(r.returncode)
r = subprocess.run([str(OUT/'test.exe')])
if r.returncode:
    print('FAIL at test.c line',r.returncode)
    lines=harness.splitlines()
    if 0<r.returncode<=len(lines):print(lines[r.returncode-1])
    sys.exit(1)
print('PASS: production fan/relay/alarm/PID/config functions with GPIO, time and EEPROM mocks.')
print('Assertions:', harness.count('CHECK(')-1)

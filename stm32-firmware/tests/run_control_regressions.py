from pathlib import Path
import subprocess
import sys

import argparse
import shutil
import tempfile
parser = argparse.ArgumentParser(description='Windows native regression tests; no hardware required.')
parser.add_argument('--clang', default=shutil.which('clang') or r'F:\DEVECO\DevEco Studio\sdk\default\openharmony\native\llvm\bin\clang.exe')
parser.add_argument('--firmware-root', type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument('--overlay', type=Path)
args = parser.parse_args()
REAL = args.firmware_root
STAGED = args.overlay or REAL
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
typedef struct { void *Instance; } TIM_HandleTypeDef;
#define TIM1 ((void *)1)
extern TIM_HandleTypeDef htim3;
#define GPIOA 0
#define GPIOB 1
#define GPIOC 2
extern uint32_t mock_irq_mask;
extern unsigned inject_completion;
void AppConfig_CompletePumpFromISR(void);
static inline uint32_t __get_PRIMASK(void) {return mock_irq_mask;}
static inline void __disable_irq(void) {if(inject_completion){inject_completion=0;AppConfig_CompletePumpFromISR();}mock_irq_mask=1;}
static inline void __set_PRIMASK(uint32_t mask) {mock_irq_mask=mask;}
#define RESET 0
#define GPIO_PIN_1 (1U << 1)
#define GPIO_PIN_4 (1U << 4)
int HAL_GPIO_ReadPin(int port, unsigned pin);
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
#define __HAL_TIM_GET_AUTORELOAD(h) 999U
#endif
''')
for name in ('main.h', 'fan.h', 'jdq.h', 'beep.h'):
    header(name, read('Core/Inc/' + name))
header('sensor_data.h', read('modules/middleware/inc/sensor_data.h'))
header('mw_alarm.h', read('modules/middleware/inc/mw_alarm.h'))
header('mw_climate.h',read('modules/middleware/inc/mw_climate.h'))
header('mw_climate_port.h', read('modules/middleware/inc/mw_climate_port.h'))
header('mw_alarm_port.h', read('modules/middleware/inc/mw_alarm_port.h'))
header('app_sensors.h', read('modules/app/inc/app_sensors.h'))
header('app_config.h', read('modules/app/inc/app_config.h'))
header('mw_pid.h', read('modules/middleware/inc/mw_pid.h'))
header('LED.h', read('Core/Inc/LED.h'))
header('mw_led_port.h', read('modules/middleware/inc/mw_led_port.h'))
header('mw_key_port.h', read('modules/middleware/inc/mw_key_port.h'))
header('tim.h', '#include \"stm32f1xx_hal.h\"\n')
header('gpio.h', '#include \"stm32f1xx_hal.h\"\n')
header('Key.h', '#ifndef MOCK_KEY_H\n#define MOCK_KEY_H\n#include "stm32f1xx_hal.h"\ntypedef struct { uint8_t State, Pin_state, ClickFlag, LongClickFlag; uint32_t Presstime, Releasetime; } Key;\n#define LDR_STEP 100\n#define CO2_STEP 100\n#define TEMP_STEP 1\n#define BUMP_STEP 2\nvoid Key_Func(void);\n#include "LED.h"\n#include "bump.h"\n#endif\n')
header('oled.h', '#ifndef MOCK_OLED_H\n#define MOCK_OLED_H\nvoid OLED_Clear(void);\nvoid Display1_Init(void);\nvoid Display2_Init(void);\nvoid Display3_Init(void);\nvoid Display4_Init(void);\nvoid Display5_Init(void);\nvoid OLED_ShowCHinese(int x, int y, int symbol);\n#endif\n')
header('usart3.h', '#include <stdint.h>\n#define CO2_THREHOLD 2000\n')
header('bump.h', '''#ifndef MOCK_BUMP_H
#define MOCK_BUMP_H
#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "oled.h"
typedef enum { Bump_OFF=0, Bump_CALIBRATION=1, Bump_AUTO=2 } Bump_Mode;
#define HUMI_THRESHOLD_LOW 30
#define AREA 0.08f
#define WATER_DEPTH 10.0f
#define WATER_DENSITY 1.0f
#define FLOW_RATE 1.2f
#define MIN_WATER_VOLUME 0.1f
void Bump_Set(int mode);
void BUMP_Init(void);
#define HUMI_THRESHOLD_HIGH 75
#include "mw_alarm.h"
typedef enum { DISPLAY_NONE=0, DISPLAY_SCREEN1, DISPLAY_SCREEN2, DISPLAY_SCREEN3, DISPLAY_SCREEN4, DISPLAY_SCREEN5 } DisplayScreen;
#define BUMP_ON HAL_GPIO_WritePin(GPIOB,GPIO_PIN_1,GPIO_PIN_SET)
#define BUMP_OFF HAL_GPIO_WritePin(GPIOB,GPIO_PIN_1,GPIO_PIN_RESET)
#endif
''')
header('Display.h', '#include \"bump.h\"\n')
header('mw_config.h', read('modules/middleware/inc/mw_config.h'))
header('mw_command.h', read('modules/middleware/inc/mw_command.h'))
header('app_commands.h', read('modules/app/inc/app_commands.h'))
header('mw_config_port.h', read('modules/middleware/inc/mw_config_port.h'))
header('i2c.h', '''#include "stm32f1xx_hal.h"
#define EEPROM_24C02_SIZE 256U
uint8_t EEPROM24C02_Read(uint8_t addr, uint8_t *buf, uint16_t len);
uint8_t EEPROM24C02_Write(uint8_t addr, const uint8_t *buf, uint16_t len);
''')
header('math.h', 'static inline float fabsf(float x) { return x < 0 ? -x : x; }\n')
header('string.h', '#include <stddef.h>\nvoid *memset(void *p, int c, size_t n);void *memcpy(void*,const void*,size_t);\nint strcmp(const char *a,const char *b);\n')

harness = r'''
#include "stm32f1xx_hal.h"
#include "main.h"
#include "LED.h"
#include "Key.h"
#include "bump.h"
#include "jdq.h"
#include "beep.h"
#include "mw_config.h"
#include "mw_config_port.h"
#include "app_sensors.h"
#include "app_config.h"
#include <stddef.h>
TIM_HandleTypeDef htim3;
Key Keys[4];
static SensorData test_data = {25,40,200,50,350,11};
const SensorData *SensorService_GetData(void) { return &test_data; }
#define temp test_data.temperature_c
#define humi test_data.humidity_pct
#define sensor_valid test_data.valid_mask
#define ppm test_data.co2_ppm
#define moist test_data.soil_moisture_pct
#define light test_data.light_lux
float water_vol;
extern volatile uint32_t time_ms, hp_ms_total, hp_ms_count, hp_state_start;
extern volatile uint8_t hp_running, hp_is_on;
extern uint8_t Monitor_para1;
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
#define temp_threhold (AppConfig_Get()->temp_threshold)
#define CO2_Threhold (AppConfig_Get()->co2_threshold)
#define CurrentBump_mode (AppConfig_Get()->bump_mode)
volatile Abnormal abnormal[3];
volatile DisplayScreen CurrentScreen;
static uint32_t tick;
static unsigned pwm;
static unsigned pwm_writes;
static int pins[3][16];
static uint8_t eeprom[256];
static unsigned fail_write,fail_read,corrupt_write;
static unsigned read_calls,write_calls;
uint32_t mock_irq_mask;
unsigned inject_completion;
int _fltused = 0;
uint32_t HAL_GetTick(void) { return tick; }
void HAL_GPIO_WritePin(int port, unsigned mask, int value) {
    unsigned i; for (i=0;i<16;i++) if (mask & (1U<<i)) pins[port][i]=value;
}
void HAL_TIM_PWM_Start(TIM_HandleTypeDef *h, int c) { (void)h; (void)c; }
void HAL_TIM_PWM_Stop(TIM_HandleTypeDef *h, int c) { (void)h; (void)c; }
void mock_compare(unsigned v) { pwm=v; pwm_writes++; }
int HAL_GPIO_ReadPin(int port,unsigned pin) { (void)port;(void)pin;return 1; }
void OLED_Clear(void) {}
void Display1_Init(void) {}
void Display2_Init(void) {}
void Display3_Init(void) {}
void Display4_Init(void) {}
void Display5_Init(void) {}
void OLED_ShowCHinese(int x,int y,int s) { (void)x;(void)y;(void)s; }
void *memset(void *p,int c,size_t n) { uint8_t *b=p; while(n--) *b++=(uint8_t)c; return p; }
void *memcpy(void *p,const void *s,size_t n) { uint8_t *b=p;const uint8_t *a=s;while(n--) *b++=*a++;return p; }
uint8_t EEPROM24C02_Read(uint8_t a,uint8_t *b,uint16_t n) {
    unsigned i; read_calls++; if (fail_read || (unsigned)a+n>256) return 0; for(i=0;i<n;i++) b[i]=eeprom[a+i]; return 1;
}
uint8_t EEPROM24C02_Write(uint8_t a,const uint8_t *b,uint16_t n) {
    unsigned i; write_calls++; if (fail_write || (unsigned)a+n>256) return 0; for(i=0;i<n;i++) eeprom[a+i]=b[i]; if(corrupt_write)eeprom[a]^=1; return 1;
}
static const uint8_t legacy_v2[] = {49,71,70,67,2,0,48,0,7,0,2,0,65,33,252,8,120,0,8,7,25,0,85,0,217,4,255,15,244,1,17,0,219,5,0,0,123,0,0,0,200,1,0,0,211,10,200,96};
static const uint8_t expected_v3_next[] = {49,71,70,67,3,0,52,0,8,0,2,0,65,30,252,8,120,0,8,7,25,0,85,0,217,4,255,15,244,1,17,0,219,5,0,0,123,0,0,0,200,1,0,0,20,0,0,0,185,227,201,211};
static const uint8_t legacy_v1[] = {49,71,70,67,1,0,32,0,5,0,2,0,60,31,152,8,100,0,220,5,25,0,80,0,217,4,255,15,95,251,98,80};
static const uint8_t legacy_invalid_values[] = {49,71,70,67,2,0,48,0,7,0,255,255,255,255,136,19,208,7,100,0,90,0,20,0,217,4,255,15,255,255,201,0,255,255,255,255,17,39,0,0,255,255,255,255,6,123,49,18};
static unsigned assertions;
#define CHECK(x) do { assertions++;if (!(x)) return __LINE__; } while(0)
static int near(float a,float b) { float d=a-b; return d>-0.01f && d<0.01f; }
static void pid(float kp,float ki,float kd,uint32_t start,int error) {
    tick=start; LED_Init(); (void)AppConfig_Set(APP_CFG_PID_TARGET,200);(void)AppConfig_Set(APP_CFG_PID_DEAD_ZONE,0);
    (void)AppConfig_Set(APP_CFG_PID_KP,(int32_t)(kp*1000.0f+0.5f));
    (void)AppConfig_Set(APP_CFG_PID_KI,(int32_t)(ki*1000.0f+0.5f));
    (void)AppConfig_Set(APP_CFG_PID_KD,(int32_t)(kd*1000.0f+0.5f));LED_PID_Reset();
    LED_Update((int16_t)(200-error));
}

/* 调用新的生产命令入口；完整JSON解析另由命令回归覆盖。 */
#include "app_commands.h"
int strcmp(const char *a,const char *b) {while(*a && *b && *a==*b){a++;b++;}return (unsigned char)*a-(unsigned char)*b;}
static void cloud_set(const char *name,int value) {
    CommandEntry command;
    command.key=Command_FindKey(name);command.value=value;
    (void)AppCommands_Execute(&command,1);
}

int main(void) {
    AppConfig_Init();
    /* 真正调用生产代码：温度/CO2 的四种组合和后执行报警的覆盖回归。 */
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,100);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,1000);(void)AppConfig_Set(APP_CFG_TEMP_THRESHOLD,25);
    temp=28;ppm=350;sensor_valid=11;Monitor_Temp();IS_Normal();
    CHECK(pins[GPIOB][5]==GPIO_PIN_RESET); CHECK(pins[GPIOB][6]==GPIO_PIN_SET);
    temp=25;ppm=2500;Monitor_Temp();IS_Normal();CHECK(pins[GPIOB][5]==GPIO_PIN_RESET);
    temp=28;ppm=2500;Monitor_Temp();IS_Normal();CHECK(pins[GPIOB][5]==GPIO_PIN_RESET);
    temp=24;ppm=350;Monitor_Temp();IS_Normal();CHECK(pins[GPIOB][5]==GPIO_PIN_SET);
    temp=19;Monitor_Temp();CHECK(pins[GPIOB][6]==GPIO_PIN_RESET);
    temp=28;Monitor_Temp();CHECK(pins[GPIOB][6]==GPIO_PIN_SET);
    temp=19;sensor_valid=0;Monitor_Temp();CHECK(pins[GPIOB][6]==GPIO_PIN_SET);
    temp=28;ppm=2500;Monitor_Temp();IS_Normal();
    CHECK(pins[GPIOB][5]==GPIO_PIN_SET);CHECK(abnormal[2]==NONE);
    sensor_valid=SENSOR_VALID_CO2;Monitor_Temp();CHECK(pins[GPIOB][5]==GPIO_PIN_RESET);
    sensor_valid=SENSOR_VALID_TEMP;ppm=350;temp=26;Monitor_Temp();CHECK(pins[GPIOB][5]==GPIO_PIN_RESET);
    temp=24;Monitor_Temp();CHECK(pins[GPIOB][6]==GPIO_PIN_SET);
    (void)AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,0);(void)AppConfig_Set(APP_CFG_TEMP_THRESHOLD,1);temp=0;Monitor_Temp();CHECK(pins[GPIOB][6]==GPIO_PIN_SET);
    (void)AppConfig_Set(APP_CFG_TEMP_THRESHOLD,100);temp=100;Monitor_Temp();CHECK(pins[GPIOB][5]==GPIO_PIN_SET);
    /* 实际按键：上限至少大于下限，饱和增减不改采样值。 */
    CurrentScreen=DISPLAY_SCREEN4;temp=25;
    (void)AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,0);
    (void)AppConfig_Set(APP_CFG_TEMP_THRESHOLD,1);Monitor_para1=2;Keys[3].ClickFlag=1;Key_Func();
    CHECK(temp_threhold==1);CHECK(temp==25);
    (void)AppConfig_Set(APP_CFG_TEMP_THRESHOLD,2);Monitor_para1=2;Keys[3].ClickFlag=1;Key_Func();
    CHECK(temp_threhold==1);CHECK(temp==25);
    (void)AppConfig_Set(APP_CFG_TEMP_THRESHOLD,25);Monitor_para1=2;Keys[3].ClickFlag=1;Key_Func();
    CHECK(temp_threhold==24);CHECK(temp==25);
    /* 本机四项导航、每次1℃、保持选择和双阈值交叉保护。 */
    AppConfig_Init();CurrentScreen=DISPLAY_SCREEN4;Monitor_para1=0;
    Keys[1].ClickFlag=1;Key_Func();CHECK(Monitor_para1==1);
    Keys[1].ClickFlag=1;Key_Func();CHECK(Monitor_para1==2);
    Keys[1].ClickFlag=1;Key_Func();CHECK(Monitor_para1==6);
    Keys[2].ClickFlag=1;Key_Func();CHECK(AppConfig_Get()->temp_low_threshold==21);CHECK(Monitor_para1==6);
    Keys[2].ClickFlag=1;Key_Func();CHECK(AppConfig_Get()->temp_low_threshold==22);CHECK(Monitor_para1==6);
    Keys[3].ClickFlag=1;Key_Func();CHECK(AppConfig_Get()->temp_low_threshold==21);CHECK(Monitor_para1==6);
    Keys[1].ClickFlag=1;Key_Func();CHECK(Monitor_para1==0);
    Monitor_para1=2;Keys[2].ClickFlag=1;Key_Func();CHECK(temp_threhold==31);CHECK(Monitor_para1==2);
    (void)AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,30);Monitor_para1=6;
    Keys[2].ClickFlag=1;Key_Func();CHECK(AppConfig_Get()->temp_low_threshold==30);CHECK(temp_threhold==31);
    Monitor_para1=2;Keys[3].ClickFlag=1;Key_Func();CHECK(temp_threhold==31);
    (void)AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,0);Monitor_para1=6;
    Keys[3].ClickFlag=1;Key_Func();CHECK(AppConfig_Get()->temp_low_threshold==0);
    (void)AppConfig_Set(APP_CFG_TEMP_THRESHOLD,100);(void)AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,99);
    Keys[2].ClickFlag=1;Key_Func();CHECK(AppConfig_Get()->temp_low_threshold==99);
    Monitor_para1=2;Keys[2].ClickFlag=1;Key_Func();CHECK(temp_threhold==100);
    CurrentScreen=DISPLAY_SCREEN4;Monitor_para1=6;Keys[0].ClickFlag=1;Key_Func();
    CHECK(CurrentScreen==DISPLAY_SCREEN5);CHECK(Monitor_para1==3);
    Keys[1].ClickFlag=1;Key_Func();CHECK(Monitor_para1==4);
    Keys[1].ClickFlag=1;Key_Func();CHECK(Monitor_para1==5);
    Keys[1].ClickFlag=1;Key_Func();CHECK(Monitor_para1==3);
    CurrentScreen=DISPLAY_SCREEN3;Monitor_para1=5;Keys[0].ClickFlag=1;Key_Func();
    CHECK(CurrentScreen==DISPLAY_SCREEN4);CHECK(Monitor_para1==0);
    /* 真正通过按键改下限，再经生产v3存储与加载验证。 */
    AppConfig_Init();memset(eeprom,255,sizeof(eeprom));Config_Load();Monitor_para1=6;
    Keys[2].ClickFlag=1;Key_Func();CHECK(AppConfig_Get()->temp_low_threshold==21);CHECK(Config_IsDirty());
    tick+=1000;Config_Process();CHECK(!Config_IsDirty());
    AppConfig_Init();Config_Load();CHECK(AppConfig_Get()->temp_threshold==30);CHECK(AppConfig_Get()->temp_low_threshold==21);
    AppConfig_Init();
    /* PID 积分必须使用实际 0.5/1/1.5 秒，而不是全部裁成 0.1 秒。 */
    pid(0,1,0,0,100);tick=1000;LED_Update(100);CHECK(near(led_ctrl.pid.integral,100));CHECK(pwm==100);
    pid(0,1,0,0,100);tick=500;LED_Update(100);CHECK(near(led_ctrl.pid.integral,50));
    pid(0,1,0,0,100);tick=1500;LED_Update(100);CHECK(near(led_ctrl.pid.integral,150));
    pid(0,0,1,0,10);tick=1000;LED_Update(180);CHECK(pwm==10);
    pid(0,1,0,0xFFFFFF00U,100);tick=744;LED_Update(100);CHECK(near(led_ctrl.pid.integral,100));
    pid(0,1,0,0,100);LED_Update(100);CHECK(near(led_ctrl.pid.integral,0));
    pid(0,1,0,0,100);led_ctrl.pid.integral=900;tick=10000;LED_Update(100);
    CHECK(near(led_ctrl.pid.integral,100));CHECK(pwm==100);
    pid(1.5f,0,0,0,100);tick=1000;LED_Update(100);CHECK(pwm==150);
    LED_SetPWM(-50);CHECK(pwm==0);LED_SetPWM(1500);CHECK(pwm==1000);
    /* 空白配置/双槽保存和重读：新的默认值200，已有有效500不被强制覆盖。 */
    memset(eeprom,255,sizeof(eeprom));Config_Load();
    CHECK(AppConfig_Get()->pid_target_lux==200);CHECK(CurrentBump_mode==Bump_OFF);
    tick+=1000;Config_Process();CHECK(Config_IsDirty()==0);
    (void)AppConfig_Set(APP_CFG_PID_TARGET,500);CHECK(Config_Save()!=0);(void)AppConfig_Set(APP_CFG_PID_TARGET,123);Config_Load();
    CHECK(AppConfig_Get()->pid_target_lux==500);

    /* Runtime/storage are different structures; v3 storage offsets are checked; v1/v2 records remain readable. */
    CHECK(sizeof(DeviceConfig)==52);CHECK(offsetof(DeviceConfig,crc32)==48);
    /* Read an independently packed old v2 record, then migrate to independently packed v3 bytes in the other slot. */
    memset(eeprom,255,sizeof(eeprom));memcpy(eeprom,legacy_v2,sizeof(legacy_v2));Config_Load();
    CHECK(AppConfig_Get()->pid_target_lux==500);CHECK(AppConfig_Get()->temp_threshold==30);
    CHECK(AppConfig_Get()->co2_threshold==2300);CHECK(AppConfig_Get()->soil_low==25);CHECK(AppConfig_Get()->soil_high==85);
    CHECK(AppConfig_Get()->led_manual_pwm==65);CHECK(AppConfig_Get()->pid_kp_x1000==1499);
    CHECK(AppConfig_Get()->pid_ki_x1000==123);CHECK(AppConfig_Get()->pid_kd_x1000==456);
    CHECK(Config_Save()!=0);
    {unsigned i;for(i=0;i<sizeof(expected_v3_next);i++)CHECK(eeprom[64+i]==expected_v3_next[i]);}
    /* Failed write/readback/corrupted target do not destroy the previous valid record. */
    (void)AppConfig_Set(APP_CFG_TEMP_THRESHOLD,40);fail_write=1;CHECK(Config_Save()==0);fail_write=0;Config_Load();CHECK(temp_threhold==30);
    (void)AppConfig_Set(APP_CFG_TEMP_THRESHOLD,40);fail_read=1;CHECK(Config_Save()==0);fail_read=0;
    /* Reload the known fixture before testing corruption: a readback failure may have written a valid slot. */
    memset(eeprom,255,sizeof(eeprom));memcpy(eeprom,legacy_v2,sizeof(legacy_v2));Config_Load();
    (void)AppConfig_Set(APP_CFG_TEMP_THRESHOLD,40);corrupt_write=1;CHECK(Config_Save()==0);corrupt_write=0;Config_Load();CHECK(temp_threhold==30);
    memset(eeprom,255,sizeof(eeprom));memcpy(eeprom,legacy_v1,sizeof(legacy_v1));Config_Load();
    CHECK(temp_threhold==30);CHECK(AppConfig_Get()->pid_target_lux==200);CHECK(AppConfig_Get()->pid_kp_x1000==1500);
    CHECK(Config_IsDirty()!=0);tick+=1000;Config_Process();CHECK(Config_IsDirty()==0);
    /* Direct setters (used by cloud) reject invalid scalars; key adjustments saturate. */
    CHECK(AppConfig_Set(APP_CFG_TEMP_THRESHOLD,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_TEMP_THRESHOLD,101)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_CO2_THRESHOLD,349)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_CO2_THRESHOLD,4001)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set((AppConfigKey)999,1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_GetValue(APP_CFG_TEMP_THRESHOLD,0)==0);
    CHECK(AppConfig_Restore(0)==0);
    (void)AppConfig_Set(APP_CFG_SOIL_HIGH,75);(void)AppConfig_Set(APP_CFG_SOIL_LOW,30);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,99);CHECK(AppConfig_Get()->soil_low==75);
    (void)AppConfig_Set(APP_CFG_SOIL_HIGH,1);CHECK(AppConfig_Get()->soil_high==75);
    (void)AppConfig_Adjust(APP_CFG_SOIL_LOW,(-2147483647-1));CHECK(AppConfig_Get()->soil_low==0);
    (void)AppConfig_Adjust(APP_CFG_SOIL_HIGH,2147483647);CHECK(AppConfig_Get()->soil_high==100);
    /* Actual key code: near limits and paired boundaries. */
    CurrentScreen=DISPLAY_SCREEN4;
    (void)AppConfig_Set(APP_CFG_TEMP_THRESHOLD,99);Monitor_para1=2;Keys[2].ClickFlag=1;Key_Func();CHECK(temp_threhold==100);
    (void)AppConfig_Set(APP_CFG_LIGHT_HIGH,1000);(void)AppConfig_Set(APP_CFG_LIGHT_LOW,50);
    Monitor_para1=0;Keys[3].ClickFlag=1;Key_Func();CHECK(AppConfig_Get()->ldr_low==0);
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,950);Monitor_para1=0;Keys[2].ClickFlag=1;Key_Func();CHECK(AppConfig_Get()->ldr_low==1000);
    CurrentScreen=DISPLAY_SCREEN5;(void)AppConfig_Set(APP_CFG_CO2_THRESHOLD,375);
    Monitor_para1=5;Keys[3].ClickFlag=1;Key_Func();CHECK(CO2_Threhold==350);
    (void)AppConfig_Set(APP_CFG_CO2_THRESHOLD,3950);Monitor_para1=5;Keys[2].ClickFlag=1;Key_Func();CHECK(CO2_Threhold==4000);
    /* Both input paths converge; key mode changes now notify delayed persistence. */
    CurrentScreen=DISPLAY_NONE;LED_Set(2);Mode_Switch();Keys[2].ClickFlag=1;Key_Func();CHECK(AppConfig_Get()->led_mode==0);
    CHECK(Config_IsDirty()!=0);tick+=1000;Config_Process();CHECK(Config_IsDirty()==0);
    LED_Set(0);CHECK(Config_IsDirty()==0); /* same value: don't wear EEPROM */
    LED_Set(2);Keys[2].ClickFlag=1;Keys[2].LongClickFlag=1;Key_Func();CHECK(AppConfig_Get()->led_mode==1);
    Keys[2].ClickFlag=1;Keys[2].LongClickFlag=1;Key_Func();CHECK(AppConfig_Get()->led_mode==2);
    /* Real timer completion updates only the pulse mode, with no dirty/timer side effect. */
    Bump_Set(1);tick+=1000;Config_Process();CHECK(Config_IsDirty()==0);
    hp_running=1;hp_ms_count=0;hp_ms_total=10;hp_is_on=1;hp_state_start=tick;
    {TIM_HandleTypeDef timer={TIM1};HAL_TIM_PeriodElapsedCallback(&timer);}
    CHECK(CurrentBump_mode==0);CHECK(Config_IsDirty()==0);CHECK(hp_running==0);
    Bump_Set(2);AppConfig_CompletePumpFromISR();CHECK(CurrentBump_mode==2);Bump_Set(0);
    Bump_Set(1);tick+=1000;Config_Process();inject_completion=1;Bump_Set(1);
    CHECK(CurrentBump_mode==1);CHECK(Config_IsDirty()!=0);CHECK(mock_irq_mask==0);
    mock_irq_mask=1;Bump_Set(0);CHECK(mock_irq_mask==1);mock_irq_mask=0;
    /* Updating PID configuration resets dynamic history; repeated values do not. */
    led_ctrl.pid.integral=42;CHECK(AppConfig_Set(APP_CFG_PID_KI,1000)==APP_CONFIG_CHANGED);CHECK(near(led_ctrl.pid.integral,0));
    led_ctrl.pid.integral=42;CHECK(AppConfig_Set(APP_CFG_PID_KI,1000)==APP_CONFIG_UNCHANGED);CHECK(near(led_ctrl.pid.integral,42));
    {RuntimeConfig invalid=*AppConfig_Get();invalid.temp_threshold=101;CHECK(AppConfig_Restore(&invalid)==0);}


    /* All14 parameter keys: inclusive bounds, both invalid edges, and safe huge adjustments. */

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_LED_MODE,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_LED_MODE,3)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_LED_MODE,0)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->led_mode==0);
    CHECK(AppConfig_Set(APP_CFG_LED_MODE,2)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->led_mode==2);
    (void)AppConfig_Adjust(APP_CFG_LED_MODE,(-2147483647-1));CHECK(AppConfig_Get()->led_mode==0);
    (void)AppConfig_Adjust(APP_CFG_LED_MODE,2147483647);CHECK(AppConfig_Get()->led_mode==2);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_BUMP_MODE,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_BUMP_MODE,3)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_BUMP_MODE,0)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->bump_mode==0);
    CHECK(AppConfig_Set(APP_CFG_BUMP_MODE,2)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->bump_mode==2);
    (void)AppConfig_Adjust(APP_CFG_BUMP_MODE,(-2147483647-1));CHECK(AppConfig_Get()->bump_mode==0);
    (void)AppConfig_Adjust(APP_CFG_BUMP_MODE,2147483647);CHECK(AppConfig_Get()->bump_mode==2);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_MANUAL_PWM,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_MANUAL_PWM,101)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_MANUAL_PWM,0)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->led_manual_pwm==0);
    CHECK(AppConfig_Set(APP_CFG_MANUAL_PWM,100)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->led_manual_pwm==100);
    (void)AppConfig_Adjust(APP_CFG_MANUAL_PWM,(-2147483647-1));CHECK(AppConfig_Get()->led_manual_pwm==0);
    (void)AppConfig_Adjust(APP_CFG_MANUAL_PWM,2147483647);CHECK(AppConfig_Get()->led_manual_pwm==100);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_TEMP_THRESHOLD,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_TEMP_THRESHOLD,101)==APP_CONFIG_REJECTED);
    (void)AppConfig_Set(APP_CFG_TEMP_LOW_THRESHOLD,0);
    CHECK(AppConfig_Set(APP_CFG_TEMP_THRESHOLD,1)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->temp_threshold==1);
    CHECK(AppConfig_Set(APP_CFG_TEMP_THRESHOLD,100)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->temp_threshold==100);
    (void)AppConfig_Adjust(APP_CFG_TEMP_THRESHOLD,(-2147483647-1));CHECK(AppConfig_Get()->temp_threshold==1);
    (void)AppConfig_Adjust(APP_CFG_TEMP_THRESHOLD,2147483647);CHECK(AppConfig_Get()->temp_threshold==100);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_CO2_THRESHOLD,349)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_CO2_THRESHOLD,4001)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_CO2_THRESHOLD,350)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->co2_threshold==350);
    CHECK(AppConfig_Set(APP_CFG_CO2_THRESHOLD,4000)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->co2_threshold==4000);
    (void)AppConfig_Adjust(APP_CFG_CO2_THRESHOLD,(-2147483647-1));CHECK(AppConfig_Get()->co2_threshold==350);
    (void)AppConfig_Adjust(APP_CFG_CO2_THRESHOLD,2147483647);CHECK(AppConfig_Get()->co2_threshold==4000);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_LIGHT_LOW,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_LIGHT_LOW,10000)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_LIGHT_LOW,0)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->ldr_low==0);
    CHECK(AppConfig_Set(APP_CFG_LIGHT_LOW,9999)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->ldr_low==9999);
    (void)AppConfig_Adjust(APP_CFG_LIGHT_LOW,(-2147483647-1));CHECK(AppConfig_Get()->ldr_low==0);
    (void)AppConfig_Adjust(APP_CFG_LIGHT_LOW,2147483647);CHECK(AppConfig_Get()->ldr_low==9999);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_LIGHT_HIGH,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_LIGHT_HIGH,10000)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_LIGHT_HIGH,0)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->ldr_high==0);
    CHECK(AppConfig_Set(APP_CFG_LIGHT_HIGH,9999)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->ldr_high==9999);
    (void)AppConfig_Adjust(APP_CFG_LIGHT_HIGH,(-2147483647-1));CHECK(AppConfig_Get()->ldr_high==0);
    (void)AppConfig_Adjust(APP_CFG_LIGHT_HIGH,2147483647);CHECK(AppConfig_Get()->ldr_high==9999);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_SOIL_LOW,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_SOIL_LOW,101)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_SOIL_LOW,0)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->soil_low==0);
    CHECK(AppConfig_Set(APP_CFG_SOIL_LOW,100)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->soil_low==100);
    (void)AppConfig_Adjust(APP_CFG_SOIL_LOW,(-2147483647-1));CHECK(AppConfig_Get()->soil_low==0);
    (void)AppConfig_Adjust(APP_CFG_SOIL_LOW,2147483647);CHECK(AppConfig_Get()->soil_low==100);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_SOIL_HIGH,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_SOIL_HIGH,101)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_SOIL_HIGH,0)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->soil_high==0);
    CHECK(AppConfig_Set(APP_CFG_SOIL_HIGH,100)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->soil_high==100);
    (void)AppConfig_Adjust(APP_CFG_SOIL_HIGH,(-2147483647-1));CHECK(AppConfig_Get()->soil_high==0);
    (void)AppConfig_Adjust(APP_CFG_SOIL_HIGH,2147483647);CHECK(AppConfig_Get()->soil_high==100);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_PID_TARGET,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_PID_TARGET,1000)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_PID_TARGET,0)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->pid_target_lux==0);
    CHECK(AppConfig_Set(APP_CFG_PID_TARGET,999)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->pid_target_lux==999);
    (void)AppConfig_Adjust(APP_CFG_PID_TARGET,(-2147483647-1));CHECK(AppConfig_Get()->pid_target_lux==0);
    (void)AppConfig_Adjust(APP_CFG_PID_TARGET,2147483647);CHECK(AppConfig_Get()->pid_target_lux==999);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_PID_DEAD_ZONE,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_PID_DEAD_ZONE,201)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_PID_DEAD_ZONE,0)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->pid_dead_zone==0);
    CHECK(AppConfig_Set(APP_CFG_PID_DEAD_ZONE,200)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->pid_dead_zone==200);
    (void)AppConfig_Adjust(APP_CFG_PID_DEAD_ZONE,(-2147483647-1));CHECK(AppConfig_Get()->pid_dead_zone==0);
    (void)AppConfig_Adjust(APP_CFG_PID_DEAD_ZONE,2147483647);CHECK(AppConfig_Get()->pid_dead_zone==200);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_PID_KP,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_PID_KP,10001)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_PID_KP,0)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->pid_kp_x1000==0);
    CHECK(AppConfig_Set(APP_CFG_PID_KP,10000)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->pid_kp_x1000==10000);
    (void)AppConfig_Adjust(APP_CFG_PID_KP,(-2147483647-1));CHECK(AppConfig_Get()->pid_kp_x1000==0);
    (void)AppConfig_Adjust(APP_CFG_PID_KP,2147483647);CHECK(AppConfig_Get()->pid_kp_x1000==10000);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_PID_KI,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_PID_KI,10001)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_PID_KI,0)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->pid_ki_x1000==0);
    CHECK(AppConfig_Set(APP_CFG_PID_KI,10000)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->pid_ki_x1000==10000);
    (void)AppConfig_Adjust(APP_CFG_PID_KI,(-2147483647-1));CHECK(AppConfig_Get()->pid_ki_x1000==0);
    (void)AppConfig_Adjust(APP_CFG_PID_KI,2147483647);CHECK(AppConfig_Get()->pid_ki_x1000==10000);

    AppConfig_Init();
    (void)AppConfig_Set(APP_CFG_LIGHT_LOW,0);(void)AppConfig_Set(APP_CFG_LIGHT_HIGH,9999);
    (void)AppConfig_Set(APP_CFG_SOIL_LOW,0);(void)AppConfig_Set(APP_CFG_SOIL_HIGH,100);
    CHECK(AppConfig_Set(APP_CFG_PID_KD,-1)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_PID_KD,10001)==APP_CONFIG_REJECTED);
    CHECK(AppConfig_Set(APP_CFG_PID_KD,0)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->pid_kd_x1000==0);
    CHECK(AppConfig_Set(APP_CFG_PID_KD,10000)!=APP_CONFIG_REJECTED);CHECK(AppConfig_Get()->pid_kd_x1000==10000);
    (void)AppConfig_Adjust(APP_CFG_PID_KD,(-2147483647-1));CHECK(AppConfig_Get()->pid_kd_x1000==0);
    (void)AppConfig_Adjust(APP_CFG_PID_KD,2147483647);CHECK(AppConfig_Get()->pid_kd_x1000==10000);

    AppConfig_Init();
    cloud_set("tempthrehold",33);CHECK(AppConfig_Get()->temp_threshold==33);
    cloud_set("tempthrehold",-1);CHECK(AppConfig_Get()->temp_threshold==33);
    cloud_set("CO2threhold",375);CHECK(CO2_Threhold==375);
    cloud_set("CO2threhold",5000);CHECK(CO2_Threhold==375);
    cloud_set("droughtthrehold",35);CHECK(AppConfig_Get()->soil_low==35);
    cloud_set("moistthrehold",80);CHECK(AppConfig_Get()->soil_high==80);
    cloud_set("droughtthrehold",90);CHECK(AppConfig_Get()->soil_low==35); /* 云端交叉范围现在拒绝，保留原35。 */
    cloud_set("led",1);CHECK(AppConfig_Get()->led_mode==1);
    cloud_set("led",99);CHECK(AppConfig_Get()->led_mode==1);
    cloud_set("bump",2);CHECK(AppConfig_Get()->bump_mode==2);
    cloud_set("bump",-1);CHECK(AppConfig_Get()->bump_mode==2);
    CurrentScreen=DISPLAY_SCREEN5;Monitor_para1=5;Keys[3].ClickFlag=1;Key_Func();CHECK(CO2_Threhold==350);
    /* CRC-valid but out-of-range old records retain the old recovery rules. */
    memset(eeprom,255,sizeof(eeprom));memcpy(eeprom,legacy_invalid_values,sizeof(legacy_invalid_values));Config_Load();
    CHECK(AppConfig_Get()->led_mode==2);CHECK(AppConfig_Get()->bump_mode==0);
    CHECK(AppConfig_Get()->led_manual_pwm==100);CHECK(temp_threhold==30);CHECK(CO2_Threhold==2000);
    CHECK(AppConfig_Get()->ldr_low==100);CHECK(AppConfig_Get()->ldr_high==1000);
    CHECK(AppConfig_Get()->soil_low==30);CHECK(AppConfig_Get()->soil_high==75);
    CHECK(AppConfig_Get()->pid_target_lux==200);CHECK(AppConfig_Get()->pid_dead_zone==20);
    CHECK(AppConfig_Get()->pid_kp_x1000==1500);CHECK(AppConfig_Get()->pid_ki_x1000==0);CHECK(AppConfig_Get()->pid_kd_x1000==0);

    /* LED适配只在READY写硬件；手动PWM也同步诊断状态。 */
    AppConfig_Init();LED_Init();
    (void)AppConfig_Set(APP_CFG_MANUAL_PWM,65);LED_Set(1);LED_Func();
    CHECK(pwm==650);CHECK(led_ctrl.pwm_value==650);
    LED_Set(2);Mode_Switch();
    {unsigned before=pwm_writes;
     LED_Update(0);CHECK(pwm_writes==before);CHECK(pwm==650);
     tick+=1000;LED_Update(0);CHECK(pwm_writes==before+1);CHECK(pwm==300);
     tick+=1000;before=pwm_writes;LED_Update(200);CHECK(pwm_writes==before);CHECK(pwm==300);
     led_ctrl.enabled=0;LED_Update(1);CHECK(pwm_writes==before);CHECK(led_ctrl.current_lux==200);
     led_ctrl.enabled=1;
    }
    /* 真实适配函数：先检查范围再窄化，不把大偏移截成8位地址。 */
    {uint8_t byte=77;unsigned r=read_calls,w=write_calls;
     CHECK(ConfigPort_Read(0,0,1)==0);CHECK(ConfigPort_Read(0,&byte,0)==0);
     CHECK(ConfigPort_Read(256,&byte,1)==0);CHECK(ConfigPort_Read(255,&byte,2)==0);
     CHECK(ConfigPort_Read(65535,&byte,1)==0);CHECK(read_calls==r);
     CHECK(ConfigPort_Write(0,0,1)==0);CHECK(ConfigPort_Write(0,&byte,0)==0);
     CHECK(ConfigPort_Write(256,&byte,1)==0);CHECK(ConfigPort_Write(255,&byte,2)==0);
     CHECK(ConfigPort_Write(65535,&byte,1)==0);CHECK(write_calls==w);
     CHECK(ConfigPort_Write(255,&byte,1)!=0);byte=0;CHECK(ConfigPort_Read(255,&byte,1)!=0);CHECK(byte==77);
     fail_write=1;CHECK(ConfigPort_Write(0,&byte,1)==0);fail_write=0;
     fail_read=1;CHECK(ConfigPort_Read(0,&byte,1)==0);fail_read=0;
     tick=0xFFFFFFF0U;CHECK(ConfigPort_NowMs()==tick);tick=32;CHECK(ConfigPort_NowMs()==32);
    }
    return assertions==EXPECTED_ASSERTIONS?0:32000;
}
'''
expected_assertions=harness.count('CHECK(')-1+51
harness=harness.replace('EXPECTED_ASSERTIONS',str(expected_assertions))
(OUT / 'test.c').write_text(harness, encoding='utf-8')
source_rel = ['Core/Src/LED.c','Core/Src/jdq.c','Core/Src/beep.c','Core/Src/Key.c','Core/Src/bump.c','modules/app/src/app_config.c','modules/app/src/app_config_port.c','modules/middleware/src/mw_config.c','modules/middleware/src/mw_pid.c','modules/middleware/src/mw_command.c','modules/app/src/app_commands.c','modules/app/src/app_led_port.c','modules/app/src/app_key_port.c','modules/middleware/src/mw_climate.c','modules/app/src/app_climate_port.c','modules/middleware/src/mw_alarm.c','modules/app/src/app_alarm_port.c']
sources = []
for rel in source_rel:
    p = OUT / Path(rel).name
    p.write_text(read(rel), encoding='utf-8')
    sources.append(str(p))
cmd = [str(CLANG), '--target=x86_64-pc-windows-msvc', '-ffreestanding', '-fno-builtin',
       '-fuse-ld=lld', '-nostdlib', '-O0', '-Werror', '-Wall', '-Wextra', '-Wno-misleading-indentation',
       '-Wl,/entry:main,/subsystem:console', '-I'+str(INC), str(OUT/'test.c'), *sources,
       '-o',str(OUT/'test.exe')]
r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
print(r.stdout+r.stderr)
if r.returncode:sys.exit(r.returncode)
r = subprocess.run([str(OUT/'test.exe')])
if r.returncode:
    print('FAIL at test.c line',r.returncode)
    lines=harness.splitlines()
    if 0<r.returncode<=len(lines):print(lines[r.returncode-1])
    sys.exit(1)
print('PASS: production fan/relay/alarm/PID/config functions with GPIO, time and EEPROM mocks.')
print('Assertions:',expected_assertions)
print('PASS: legacy EEPROM bytes/v1 migration/failure recovery, shared setters/key boundaries, mode persistence and ISR completion.')

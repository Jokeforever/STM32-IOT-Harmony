#ifndef _LED_H_
#define _LED_H_


#include "Key.h"
#include "oled.h"



#define LED_OFF		0

#define LED_ON		1

#define LED_AUTO	2

void LED_Init(void);

void LED_Set(int mode);


#define LDR_THREHOLD_HIGH 1000
#define LDR_THREHOLD_LOW  100

#define PWM_TIMER    htim3
#define PWM_CHANNEL     TIM_CHANNEL_2
#define PWM_MAX        1000  /* 对应100%占空比 */

/* 控制结构体 */
typedef struct {
    uint8_t enabled;      /* 使能标志 */
    int16_t target_lux;     /* 目标光照(lux) */
    int16_t current_lux;    /* 当前光照(lux) */
    uint16_t pwm_value;   /* 当前PWM值 */
    float k_p;            /* 比例系数 */
    uint16_t dead_zone;   /* 死区范围(lux) */
} LED_Control;

/* API函数 */
void LED_Update(int16_t current_lux);

// 系统工作模式
typedef enum {
    MODE_OFF = 0,      // 关闭模式
    MODE_CALIBRATION = 1,   // 标准模式
    MODE_AUTO_PID = 2, // 自动PID模式
} System_Mode;



//// PWM参数
//#define PWM_TIMER       htim3
//#define PWM_CHANNEL     TIM_CHANNEL_2
//#define PWM_PERIOD      999  // ARR值，决定PWM频率
//#define PWM_MAX_DUTY    100.0f // 最大占空比(%)
//#define PWM_MIN_DUTY    0.0f   // 最小占空比(%)

//// 光强测量
//#define LIGHT_MIN_LUX   0
//#define LIGHT_MAX_LUX   1000
//#define TARGET_LUX      500  // 目标光照强度



void LED_Func(void);

void LED_Display(void);

void LED_SetPWM(int16_t pwm);

//void PID_Init(PID_Controller *pid, float Kp, float Ki, float Kd, 
//              float setpoint, float output_max, float output_min) ;


//void Light_PID_Init(void);

//float PID_Compute(PID_Controller *pid, float measured);

//void PID_LED_Control(void);

void Mode_Switch(void) ;


#endif

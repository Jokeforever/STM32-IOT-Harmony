#ifndef _LED_H_
#define _LED_H_


#include <stdint.h>
#include "mw_led_port.h"
#include "mw_pid.h"



#define LED_OFF		0

#define LED_ON		1

#define LED_AUTO	2

void LED_Init(void);

void LED_Set(int mode);


#define LDR_THREHOLD_HIGH 1000
#define LDR_THREHOLD_LOW  100

#define PWM_MAX ((int)LED_PORT_DUTY_SCALE) /* 归一化千分比，保持有符号限幅比较 */
#define LED_DEFAULT_TARGET_LUX 200
#define LED_CONTROL_INTERVAL_MS 1000U
#define LED_PID_MAX_GAP_MS (5U * LED_CONTROL_INTERVAL_MS)

/* LED运行状态；目标/系数/死区由AppConfig统一保存。 */
typedef struct {
    uint8_t enabled;
    int16_t current_lux;
    uint16_t pwm_value;
    PID_State pid; /* 独立算法状态，时间由LED适配层传入。 */
} LED_Control;

extern LED_Control led_ctrl;

void LED_PID_Reset(void);

/* API函数 */
void LED_Update(int16_t current_lux);

// 系统工作模式
typedef enum {
    MODE_OFF = 0,      // 关闭模式
    MODE_CALIBRATION = 1,   // 标准模式
    MODE_AUTO_PID = 2, // 自动PID模式
} System_Mode;



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

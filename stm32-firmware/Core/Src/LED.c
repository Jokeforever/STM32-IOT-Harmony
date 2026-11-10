#include "app_config.h"
/*
 * MODULE OVERVIEW
 * 文件：LED.c
 * 作用：LED PWM 控制、自动模式位置式 PID、手动模式和状态显示。
 * 关键函数：
 *   LED_Init()       初始化LED运行状态和PWM
 *   LED_Set()        切换关闭/常亮/自动模式
 *   LED_Update()     读取配置/时间、调用独立PID，有新结果才写PWM
 *   LED_PID_Reset()  清零积分和历史误差
 * 算法位于modules/middleware/src/mw_pid.c，不访问HAL、配置服务或PWM。
 * 注意：
 *   自动模式每 1 秒读取一次光照并执行一次 PID。
 *   默认 Ki=0、Kd=0，保持原有 P 控制效果，后续再调参。
 */
// PWM与时基通过端口访问，显示适配仍保留原OLED实现。
#include "mw_led_port.h"

//硬件驱动
#include "LED.h"
#include "oled.h"





 LED_Control led_ctrl = {0};
 uint16_t ccr = 0;


void LED_Set( int mode)
{
    (void)AppConfig_Set(APP_CFG_LED_MODE, mode);
}


void LED_Init(void)
{
    led_ctrl.enabled = 1;
    led_ctrl.pwm_value = 300;

    LED_PID_Reset();

    LED_SetPWM(led_ctrl.pwm_value);
}

void LED_PID_Reset(void)
{
    PID_Reset(&led_ctrl.pid, LedPort_NowMs());
}

void LED_Update(int16_t current_lux)
{
    const RuntimeConfig *runtime;
    PID_Config config;
    uint16_t output;

    if (!led_ctrl.enabled) return;
    led_ctrl.current_lux = current_lux;
    runtime = AppConfig_Get();
    config.target = runtime->pid_target_lux;
    config.dead_zone = runtime->pid_dead_zone;
    config.kp_x1000 = runtime->pid_kp_x1000;
    config.ki_x1000 = runtime->pid_ki_x1000;
    config.kd_x1000 = runtime->pid_kd_x1000;
    config.output_max = PWM_MAX;
    config.nominal_period_ms = LED_CONTROL_INTERVAL_MS;
    config.max_gap_ms = LED_PID_MAX_GAP_MS;

    if (PID_Update(&led_ctrl.pid, &config, current_lux, LedPort_NowMs(), &output) == PID_OUTPUT_READY)
        LED_SetPWM((int16_t)output);
}

void LED_SetPWM(int16_t pwm) {
    if (pwm < 0) pwm = 0;
    if (pwm > PWM_MAX) pwm = PWM_MAX;
    if (LedPort_SetDuty((uint16_t)pwm) == LED_PORT_OK)
        led_ctrl.pwm_value = (uint16_t)pwm;
}

void Mode_Switch(void) 
{
	LED_PID_Reset();
	
    switch (AppConfig_Get()->led_mode) {
        case MODE_OFF:
            // 关闭LED
            LedPort_Disable();
            break;         
        case MODE_AUTO_PID:
            // 自动PID模式
            LedPort_Enable();
            break;
            
        case MODE_CALIBRATION:
						LedPort_Enable();

            break;
    }
}



void LED_Func(void)
{

        switch (AppConfig_Get()->led_mode) {
						case MODE_OFF:

						break;
												
            case MODE_CALIBRATION:
							ccr = 10 * AppConfig_Get()->led_manual_pwm;
            LED_SetPWM((int16_t)ccr);
             break;
						
            case MODE_AUTO_PID:
            break;

        }
}

void LED_Display(void)
{
	 switch (AppConfig_Get()->led_mode)
				{    
						case MODE_OFF:
						OLED_ShowCHinese(64,3,10);// 
						OLED_ShowCHinese(80,3,11);// 
						break;
												
            case MODE_CALIBRATION:
						OLED_ShowCHinese(64,3,8);// 
						OLED_ShowCHinese(80,3,9);// 
             break;
						
            case MODE_AUTO_PID:
						OLED_ShowCHinese(64,3,12);// 
						OLED_ShowCHinese(80,3,13);// 
						break;
				}
}

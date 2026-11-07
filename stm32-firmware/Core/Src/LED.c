#include "app_config.h"
/*
 * MODULE OVERVIEW
 * 文件：LED.c
 * 作用：LED PWM 控制、自动模式位置式 PID、手动模式和状态显示。
 * 关键函数：
 *   LED_Init()       初始化 PID 默认参数和 PWM
 *   LED_Set()        切换关闭/常亮/自动模式
 *   LED_Update()     位置式 PID 计算并直接设置 PWM
 *   LED_PID_Reset()  清零积分和历史误差
 * 注意：
 *   自动模式每 1 秒读取一次光照并执行一次 PID。
 *   默认 Ki=0、Kd=0，保持原有 P 控制效果，后续再调参。
 */
//单片机头文件
#include "stm32f1xx_hal.h"

//硬件驱动
#include "LED.h"
#include "mw_config.h"
#include <math.h>



extern TIM_HandleTypeDef htim3;
extern Key Keys[4];

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
    led_ctrl.integral = 0.0f;
    led_ctrl.last_error = 0.0f;
    led_ctrl.last_tick = HAL_GetTick();
    led_ctrl.pid_initialized = 0U;
}

void LED_Update(int16_t current_lux)
{
    float error;
    float dt;
    float p_term;
    float i_term;
    float d_term;
    float output;
    float integral_limit;
    int32_t new_pwm;
    uint32_t now;
    uint32_t elapsed_ms;

    if (!led_ctrl.enabled)
    {
        return;
    }

    led_ctrl.current_lux = current_lux;
    error = (float)AppConfig_Get()->pid_target_lux - (float)current_lux;

    if (fabsf(error) <= (float)AppConfig_Get()->pid_dead_zone)
    {
        led_ctrl.integral = 0.0f;
        led_ctrl.last_error = error;
        led_ctrl.last_tick = HAL_GetTick();
        return;
    }

    now = HAL_GetTick();
    if (led_ctrl.pid_initialized == 0U)
    {
        led_ctrl.last_tick = now;
        led_ctrl.last_error = error;
        led_ctrl.integral = 0.0f;
        led_ctrl.pid_initialized = 1U;
        return;
    }

    elapsed_ms = now - led_ctrl.last_tick;
    if (elapsed_ms == 0U)
    {
        return;
    }
    if (elapsed_ms > LED_PID_MAX_GAP_MS)
    {
        /* 长时间网络等待后重建历史，避免积分突增和微分冲击。 */
        led_ctrl.integral = 0.0f;
        led_ctrl.last_error = error;
        elapsed_ms = LED_CONTROL_INTERVAL_MS;
    }
    dt = (float)elapsed_ms / 1000.0f;

    led_ctrl.integral += error * dt;

    if (((float)AppConfig_Get()->pid_ki_x1000 / 1000.0f) > 0.0f)
    {
        integral_limit = (float)PWM_MAX / ((float)AppConfig_Get()->pid_ki_x1000 / 1000.0f);
        if (led_ctrl.integral > integral_limit)
        {
            led_ctrl.integral = integral_limit;
        }
        if (led_ctrl.integral < -integral_limit)
        {
            led_ctrl.integral = -integral_limit;
        }
    }
    else
    {
        led_ctrl.integral = 0.0f;
    }

    p_term = ((float)AppConfig_Get()->pid_kp_x1000 / 1000.0f) * error;
    i_term = ((float)AppConfig_Get()->pid_ki_x1000 / 1000.0f) * led_ctrl.integral;
    d_term = ((float)AppConfig_Get()->pid_kd_x1000 / 1000.0f) * (error - led_ctrl.last_error) / dt;
    output = p_term + i_term + d_term;

    if (output > (float)PWM_MAX)
    {
        output = (float)PWM_MAX;
    }
    if (output < 0.0f)
    {
        output = 0.0f;
    }

    new_pwm = (int32_t)output;

    __HAL_TIM_SET_COMPARE(&PWM_TIMER, PWM_CHANNEL, (uint16_t)new_pwm);
    led_ctrl.pwm_value = (uint16_t)new_pwm;
    led_ctrl.last_error = error;
    led_ctrl.last_tick = now;
}

void LED_SetPWM(int16_t pwm) {
    if (pwm < 0) pwm = 0;
    if (pwm > PWM_MAX) pwm = PWM_MAX;
    __HAL_TIM_SET_COMPARE(&PWM_TIMER, PWM_CHANNEL, pwm);
    led_ctrl.pwm_value = pwm;
}

void Mode_Switch(void) 
{
	LED_PID_Reset();
	
    switch (AppConfig_Get()->led_mode) {
        case MODE_OFF:
            // 关闭LED
            HAL_TIM_PWM_Stop(&PWM_TIMER, PWM_CHANNEL);
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
            break;         
        case MODE_AUTO_PID:
            // 自动PID模式
            HAL_TIM_PWM_Start(&PWM_TIMER, PWM_CHANNEL);
            break;
            
        case MODE_CALIBRATION:
						HAL_TIM_PWM_Start(&PWM_TIMER, PWM_CHANNEL);

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
            __HAL_TIM_SET_COMPARE(&PWM_TIMER, PWM_CHANNEL, ccr);
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

//单片机头文件
#include "stm32f1xx_hal.h"

//硬件驱动
#include "LED.h"



extern TIM_HandleTypeDef htim3;
extern uint16_t light ;
extern Key Keys[4];
uint16_t LDR_threhold[2] ;

 LED_Control led_ctrl = {0};
System_Mode current_mode = MODE_AUTO_PID;
System_Mode last_mode = MODE_AUTO_PID;
extern uint8_t LED_Manu_PWM;
 uint16_t ccr = 0;


void LED_Set( int mode)
{
	last_mode = current_mode;
	if(mode == LED_ON)
	current_mode = MODE_CALIBRATION;
	else if(mode == LED_AUTO)
	current_mode = MODE_AUTO_PID ;
	else if(mode == LED_OFF)
	current_mode = MODE_OFF;
	
	Config_MarkDirty();
}


void LED_Init(void) { 
    /* 启动PWM */   
    /* 默认参数 */
    led_ctrl.enabled = 1;
    led_ctrl.target_lux = 500;
    led_ctrl.k_p = 1.5f;        /* 比例系数，越大响应越快但可能振荡 */
    led_ctrl.dead_zone = 20;    /* ±20lux内不调整，防止振荡 */
		led_ctrl.pwm_value = 300;
    
		LDR_threhold[0] = LDR_THREHOLD_LOW;
		LDR_threhold[1] = LDR_THREHOLD_HIGH;
    /* 初始设置50%亮度 */
    LED_SetPWM(led_ctrl.pwm_value);
}

/* 核心更新函数 */
void LED_Update(int16_t current_lux) {
    /* 如果未使能，直接返回 */
    if (!led_ctrl.enabled) return;
    
    /* 保存当前光照 */
    led_ctrl.current_lux = current_lux;
    
    /* 计算误差 */
    float error = led_ctrl.target_lux - current_lux;
    
    /* 如果误差在死区内，不调整 */
    if (fabs(error) <= led_ctrl.dead_zone) {
        return;
    }
    
    /* 计算PWM调整量 */
    int16_t adjust = (int16_t)(error * led_ctrl.k_p);
    
    /* 限制调整量在±50以内，防止突变 */
    if (adjust > 50) adjust = 50;
    if (adjust < -50) adjust = -50;
    
    /* 计算新的PWM值 */
    int32_t new_pwm = (int32_t)led_ctrl.pwm_value + adjust;
    
    /* 限制PWM范围 */
    if (new_pwm > PWM_MAX) new_pwm = PWM_MAX;
    if (new_pwm < 0) new_pwm = 0;
    
    /* 设置PWM */
    __HAL_TIM_SET_COMPARE(&PWM_TIMER, PWM_CHANNEL, (uint16_t)new_pwm);
		
    led_ctrl.pwm_value = (uint16_t)new_pwm;
}

void LED_SetPWM(int16_t pwm) {
    if (pwm > PWM_MAX) pwm = PWM_MAX;
    __HAL_TIM_SET_COMPARE(&PWM_TIMER, PWM_CHANNEL, pwm);
    led_ctrl.pwm_value = pwm;
}

void Mode_Switch(void) 
{
	
    switch (current_mode) {
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

        switch (current_mode) {    
						case MODE_OFF:

						break;
												
            case MODE_CALIBRATION:
							ccr = 10 * LED_Manu_PWM;
            __HAL_TIM_SET_COMPARE(&PWM_TIMER, PWM_CHANNEL, ccr);
             break;
						
            case MODE_AUTO_PID:

						LED_Update(light);
						break;

        }
}

void LED_Display(void)
{
	 switch (current_mode) 
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

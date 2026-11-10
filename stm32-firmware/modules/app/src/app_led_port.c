/* 当前F103适配：TIM3_CH2、PA7、非反相PWM1。初始化仍由CubeMX负责。 */
#include "mw_led_port.h"
#include "tim.h"
#include "gpio.h"

#define LED_PWM_TIMER htim3
#define LED_PWM_CHANNEL TIM_CHANNEL_2

uint32_t LedPort_NowMs(void) { return HAL_GetTick(); }

void LedPort_Enable(void)
{
    (void)HAL_TIM_PWM_Start(&LED_PWM_TIMER, LED_PWM_CHANNEL);
}

void LedPort_Disable(void)
{
    (void)HAL_TIM_PWM_Stop(&LED_PWM_TIMER, LED_PWM_CHANNEL);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
}

LedPortResult LedPort_SetDuty(uint16_t duty_permille)
{
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&LED_PWM_TIMER);
    uint32_t period;
    uint32_t compare;
    /* TIM3是16位。ARR=65535时，CCR无法容纳ARR+1，不能冒充100%输出。
     * 当前ARR=999；本适配支持ARR=0..65534，其他周期明确拒绝且不改寄存器。
     */
    if (arr >= 65535U) return LED_PORT_INVALID_PERIOD;
    if (duty_permille > LED_PORT_DUTY_SCALE) duty_permille = LED_PORT_DUTY_SCALE;
    period = arr + 1U;
    /* 最大65535*1000，uint32可容纳；向下取整，量化误差小于一个计数。 */
    compare = period * duty_permille / LED_PORT_DUTY_SCALE;
    __HAL_TIM_SET_COMPARE(&LED_PWM_TIMER, LED_PWM_CHANNEL, compare);
    return LED_PORT_OK;
}

/* 当前F103报警提示：PC13指示灯、PB12蜂鸣器，均为低电平有效。
 * GPIO模式和初始关闭电平仍由原CubeMX初始化负责。
 */
#include "mw_alarm_port.h"
#include "gpio.h"

void AlarmPort_SetIndicator(uint8_t active)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,
                      active != 0U ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void AlarmPort_SetBuzzer(uint8_t active)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12,
                      active != 0U ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

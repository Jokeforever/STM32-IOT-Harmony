#include "mw_climate_port.h"
#include "gpio.h"

/* F103 板的风扇和加热输出均为低电平有效。 */
void ClimatePort_SetFan(uint8_t active)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5,
                      active != 0U ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void ClimatePort_SetHeater(uint8_t active)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6,
                      active != 0U ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

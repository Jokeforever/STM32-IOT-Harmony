#include "mw_pump_port.h"
#include "gpio.h"

/* F103 板的水泵驱动为 PB1 高电平有效。 */
void PumpPort_Set(uint8_t active)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1,
                      active != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint32_t PumpPort_NowMs(void)
{
    return HAL_GetTick();
}

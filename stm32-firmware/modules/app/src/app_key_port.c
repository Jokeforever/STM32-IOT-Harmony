/* 当前F103按键适配：PA4、PA5、PB7、PA6，均为低电平按下。 */
#include "mw_key_port.h"
#include "gpio.h"

uint8_t KeyPort_ReadPressedMask(void)
{
    uint8_t pressed = 0U;

    /* 保留原四个引脚的读取顺序；状态机仍由Key_Scan处理。 */
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET)
    {
        pressed |= KEY_PORT_KEY1_MASK;
    }
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET)
    {
        pressed |= KEY_PORT_KEY2_MASK;
    }
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_RESET)
    {
        pressed |= KEY_PORT_KEY3_MASK;
    }
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_RESET)
    {
        pressed |= KEY_PORT_KEY4_MASK;
    }
    return pressed;
}

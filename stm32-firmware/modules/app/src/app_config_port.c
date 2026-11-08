/* 当前板级适配：24C02字节存储 + HAL毫秒时基。更换平台时集中检查此处。 */
#include "mw_config_port.h"
#include "i2c.h"
#include "main.h"
#include <stddef.h>

static uint8_t ConfigPort_RangeValid(uint16_t offset, uint16_t length)
{
    /* 必须先验证再窄化为24C02驱动的8位地址，防止256变成0而覆盖旧槽。 */
    return (length != 0U && offset <= 0xFFU &&
            (uint32_t)offset + (uint32_t)length <= EEPROM_24C02_SIZE) ? 1U : 0U;
}

uint8_t ConfigPort_Read(uint16_t offset, uint8_t *buffer, uint16_t length)
{
    if (buffer == NULL || ConfigPort_RangeValid(offset, length) == 0U) return 0U;
    return EEPROM24C02_Read((uint8_t)offset, buffer, length);
}

uint8_t ConfigPort_Write(uint16_t offset, const uint8_t *buffer, uint16_t length)
{
    if (buffer == NULL || ConfigPort_RangeValid(offset, length) == 0U) return 0U;
    return EEPROM24C02_Write((uint8_t)offset, buffer, length);
}

uint32_t ConfigPort_NowMs(void)
{
    return HAL_GetTick();
}

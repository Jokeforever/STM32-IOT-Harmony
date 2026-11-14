#include "app_vgus.h"
#include "usart.h"
#include <stdio.h>

#define VGUS_DEMO_TEMPERATURE_VP 0x0200U
#define VGUS_DEMO_HUMIDITY_VP    0x0240U
#define VGUS_TX_TIMEOUT_MS      5U

static void Vgus_WriteReading(uint16_t address, const char *label, const char *unit,
                              uint8_t value, uint8_t valid)
{
    /* 帧头、长度、写变量指令、字地址，随后是按字对齐的 ASCII 文本。 */
    uint8_t frame[24] = {
        0xA5U, 0x5AU, 0U, 0x82U, (uint8_t)(address >> 8), (uint8_t)address
    };
    char *const payload = (char *)&frame[6];
    const size_t capacity = sizeof(frame) - 6U - 2U;
    const int length = valid != 0U
                     ? snprintf(payload, capacity, "%s %u%s", label, (unsigned)value, unit)
                     : snprintf(payload, capacity, "%s --", label);
    if (length < 0 || (size_t)length >= capacity) {
        return;
    }

    uint16_t end = (uint16_t)(6U + (unsigned)length);
    if ((end & 1U) != 0U) {
        frame[end++] = ' ';
    }
    frame[end++] = 0xFFU;
    frame[end++] = 0xFFU;
    frame[2] = (uint8_t)(end - 3U);
    (void)HAL_UART_Transmit(&huart1, frame, end, VGUS_TX_TIMEOUT_MS);
}

void Vgus_DisplayTempHumi(const SensorData *sensors)
{
    /* 官方 VTc 480x272 工程的“文本变量”页，左温度、右湿度。 */
    Vgus_WriteReading(VGUS_DEMO_TEMPERATURE_VP, "TEMP", " C",
                      sensors->temperature_c, sensors->valid_mask & SENSOR_VALID_TEMP);
    Vgus_WriteReading(VGUS_DEMO_HUMIDITY_VP, "RH", "%",
                      sensors->humidity_pct, sensors->valid_mask & SENSOR_VALID_HUMI);
}

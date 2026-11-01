#include "app_sensors.h"
/* 温度和 CO2 请求在这里合并，报警模块不再直接写风扇 GPIO。 */
#include "jdq.h"
#include "usart3.h"

extern uint16_t CO2_Threhold;
uint8_t temp_threhold;

void JDQ_Init(void)
{
    temp_threhold = TEMP_THREHOLD;
    JDQ_OFF;
}

void Monitor_Temp(void)
{
    const SensorData *sensors = SensorService_GetData();
    uint8_t temperature_valid = (sensors->valid_mask & SENSOR_VALID_TEMP) != 0U;
    uint8_t co2_valid = (sensors->valid_mask & SENSOR_VALID_CO2) != 0U;
    int16_t upper = (int16_t)temp_threhold + TEMP_HYSTERESIS;
    int16_t lower = (int16_t)temp_threhold - TEMP_HYSTERESIS;
    uint8_t temperature_high = temperature_valid && ((int16_t)sensors->temperature_c > upper);
    uint8_t temperature_low = temperature_valid && ((int16_t)sensors->temperature_c < lower);
    uint8_t co2_high = co2_valid && (sensors->co2_ppm > CO2_Threhold);

    /* 两个有效测量条件任一请求通风即开；均不请求时关。 */
    if (temperature_high || co2_high)
    {
        FAN_ON;
    }
    else
    {
        FAN_OFF;
    }

    /* 保留原有低温继电器用途；高温/正常/温度无效时明确关闭。 */
    if (temperature_low)
    {
        JDQ_ON;
    }
    else
    {
        JDQ_OFF;
    }
}

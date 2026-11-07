#include "app_config.h"
#include "app_sensors.h"
/* 温度和 CO2 请求在这里合并，报警模块不再直接写风扇 GPIO。 */
#include "jdq.h"
#include "usart3.h"


void JDQ_Init(void)
{
    JDQ_OFF;
}

void Monitor_Temp(void)
{
    const SensorData *sensors = SensorService_GetData();
    uint8_t temperature_valid = (sensors->valid_mask & SENSOR_VALID_TEMP) != 0U;
    uint8_t co2_valid = (sensors->valid_mask & SENSOR_VALID_CO2) != 0U;
    int16_t upper = (int16_t)AppConfig_Get()->temp_threshold + TEMP_HYSTERESIS;
    int16_t lower = (int16_t)AppConfig_Get()->temp_threshold - TEMP_HYSTERESIS;
    uint8_t temperature_high = temperature_valid && ((int16_t)sensors->temperature_c > upper);
    uint8_t temperature_low = temperature_valid && ((int16_t)sensors->temperature_c < lower);
    uint8_t co2_high = co2_valid && (sensors->co2_ppm > AppConfig_Get()->co2_threshold);

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

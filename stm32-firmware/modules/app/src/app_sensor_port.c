/* 当前硬件适配：DHT11、JW01、模拟光照及土壤湿度。
 * 保留底层驱动、整数单位与调用方式；在此统一不同的成功返回值。
 */
#include "mw_sensor_port.h"
#include "dht11.h"
#include "usart3.h"
#include "LDR.h"
#include "TS.h"

SensorPortResult SensorPort_ReadTempHumi(uint8_t *temperature_c, uint8_t *humidity_pct)
{
    uint8_t temperature = 0U;
    uint8_t humidity = 0U;

    if ((temperature_c == 0) || (humidity_pct == 0))
    {
        return SENSOR_PORT_ERROR;
    }
    /* DHT11与另外三个驱动相反：0才是成功，1/2等均为失败。 */
    if (DHT11_Read_Data(&temperature, &humidity) != DHT11_OK)
    {
        return SENSOR_PORT_ERROR;
    }
    *humidity_pct = humidity;
    *temperature_c = temperature;
    return SENSOR_PORT_OK;
}

SensorPortResult SensorPort_ReadCO2(uint16_t *co2_ppm)
{
    uint16_t value = 0U;

    if (co2_ppm == 0)
    {
        return SENSOR_PORT_ERROR;
    }
    if (CO2GetData(&value) == 0U)
    {
        return SENSOR_PORT_ERROR;
    }
    *co2_ppm = value;
    return SENSOR_PORT_OK;
}

SensorPortResult SensorPort_ReadLight(uint16_t *light_lux)
{
    uint16_t value = 0U;

    if (light_lux == 0)
    {
        return SENSOR_PORT_ERROR;
    }
    if (LDR_LuxData(&value) == 0U)
    {
        return SENSOR_PORT_ERROR;
    }
    *light_lux = value;
    return SENSOR_PORT_OK;
}

SensorPortResult SensorPort_ReadSoil(uint16_t *soil_moisture_pct)
{
    uint16_t value = 0U;

    if (soil_moisture_pct == 0)
    {
        return SENSOR_PORT_ERROR;
    }
    if (TS_GetData(&value) == 0U)
    {
        return SENSOR_PORT_ERROR;
    }
    *soil_moisture_pct = value;
    return SENSOR_PORT_OK;
}

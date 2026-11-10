/*
 * 统一传感器采集服务。
 * 把原main.c的采集、有效位和滤波历史移至此处。
 * 保持原默认值、驱动返回值语义、调用顺序与滤波行为。
 * 通过SensorPort读取；当前具体驱动适配集中在app_sensor_port.c。
 */
#include "app_sensors.h"
#include "mw_filter.h"
#include "mw_sensor_port.h"

static SensorData s_data = {20U, 40U, 200U, 20U, 350U, 0U};
static uint8_t s_temp_history[FILTER_MEDIAN3_WINDOW];
static uint8_t s_humi_history[FILTER_MEDIAN3_WINDOW];
static uint16_t s_co2_history[FILTER_MEDIAN3_WINDOW];
static uint8_t s_temp_count;
static uint8_t s_humi_count;
static uint8_t s_co2_count;

void SensorService_Init(void)
{
    s_data.temperature_c = 20U;
    s_data.humidity_pct = 40U;
    s_data.light_lux = 200U;
    s_data.soil_moisture_pct = 20U;
    s_data.co2_ppm = 350U;
    s_data.valid_mask = 0U;
    Filter_Median3_ResetU8(s_temp_history, &s_temp_count);
    Filter_Median3_ResetU8(s_humi_history, &s_humi_count);
    Filter_Median3_ResetU16(s_co2_history, &s_co2_count);
}

const SensorData *SensorService_GetData(void)
{
    return &s_data;
}

uint8_t SensorService_ReadLight(void)
{
    return (uint8_t)(SensorPort_ReadLight(&s_data.light_lux) == SENSOR_PORT_OK);
}

void SensorService_Poll(void)
{
    uint16_t raw_ppm = 0U;
    uint8_t raw_temp = 0U;
    uint8_t raw_humi = 0U;

    (void)SensorPort_ReadSoil(&s_data.soil_moisture_pct);

    if (SensorPort_ReadCO2(&raw_ppm) == SENSOR_PORT_OK)
    {
        s_data.co2_ppm = Filter_Median3_U16(s_co2_history, &s_co2_count, raw_ppm);
        s_data.valid_mask |= SENSOR_VALID_CO2;
    }
    else
    {
        s_data.valid_mask &= (uint8_t)~SENSOR_VALID_CO2;
    }

    if (SensorPort_ReadTempHumi(&raw_temp, &raw_humi) == SENSOR_PORT_OK)
    {
        s_data.temperature_c = Filter_Median3_U8(s_temp_history, &s_temp_count, raw_temp);
        s_data.humidity_pct = Filter_Median3_U8(s_humi_history, &s_humi_count, raw_humi);
        s_data.valid_mask |= (SENSOR_VALID_TEMP | SENSOR_VALID_HUMI);
    }
    else
    {
        s_data.valid_mask &= (uint8_t)~(SENSOR_VALID_TEMP | SENSOR_VALID_HUMI);
    }

    (void)SensorService_ReadLight();
}

/*
 * MODULE OVERVIEW
 * 文件：TS.c
 * 作用：土壤湿度 ADC 采集和百分比换算。
 * 关键函数：
 *   TS_GetData()  10 次采样、去极值平均、换算土壤湿度
 * 注意：
 *   土壤传感器是模拟量，拔掉后 ADC 仍可能读到数值，
 *   因此当前 sensor_status 不把土壤湿度作为有效性依据。
 */
#include "TS.h"


#define TS_ADC_WET_VALUE 1241U
#define TS_ADC_DRY_VALUE 4095U
#define TS_TRIM_COUNT 2U

uint8_t TS_GetData(uint16_t* moist)
{
    uint16_t tempData = 0U;

    if (moist == NULL)
    {
        return 0U;
    }

    if (ADC_ReadFiltered(ADC_CHANNEL_1,
                         ADC_SAMPLETIME_55CYCLES_5,
                         TS_READ_TIMES,
                         TS_TRIM_COUNT,
                         &tempData) == 0U)
    {
        return 0U;
    }

    if (tempData <= TS_ADC_WET_VALUE)
    {
        *moist = 100U;
    }
    else if (tempData >= TS_ADC_DRY_VALUE)
    {
        *moist = 0U;
    }
    else
    {
        *moist = (uint16_t)((TS_ADC_DRY_VALUE - tempData) * 100U /
                            (TS_ADC_DRY_VALUE - TS_ADC_WET_VALUE));
    }

    return 1U;
}

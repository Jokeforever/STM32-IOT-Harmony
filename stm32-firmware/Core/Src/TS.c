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

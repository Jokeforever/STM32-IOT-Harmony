#include "TS.h"


#define TS_ADC_WET_VALUE 1241U
#define TS_ADC_DRY_VALUE 4095U

static uint8_t TS_ADC_Read(uint16_t *value)
{
    return ADC_ReadChannel(ADC_CHANNEL_1, ADC_SAMPLETIME_55CYCLES_5, value);
}

uint8_t TS_GetData(uint16_t* moist)
{
    uint32_t tempData = 0U;
    uint16_t sample = 0U;
    uint8_t i;

    if (moist == NULL)
    {
        return 0U;
    }

    for (i = 0U; i < TS_READ_TIMES; i++)
    {
        if (TS_ADC_Read(&sample) == 0U)
        {
            return 0U;
        }

        tempData += sample;
        delay_ms(5U);
    }

    tempData /= TS_READ_TIMES;

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

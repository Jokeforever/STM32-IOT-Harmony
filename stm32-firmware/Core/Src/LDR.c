#include "LDR.h"

	
static uint8_t LDR_ADC_Read(uint16_t *value)
{
    return ADC_ReadChannel(ADC_CHANNEL1, ADC_SAMPLETIME_55CYCLES_5, value);
}

uint8_t LDR_Average_Data(uint16_t *average)
{
    uint32_t tempData = 0U;
    uint16_t sample = 0U;
    uint8_t i;

    if (average == NULL)
    {
        return 0U;
    }

    for (i = 0U; i < LDR_READ_TIMES; i++)
    {
        if (LDR_ADC_Read(&sample) == 0U)
        {
            return 0U;
        }

        tempData += sample;
        HAL_Delay(5U);
    }

    *average = (uint16_t)(tempData / LDR_READ_TIMES);
    return 1U;
}

uint8_t LDR_LuxData(uint16_t *light)
{
    uint16_t adcValue = 0U;
    float voltage;
    float resistance;
    float lux;

    if (light == NULL)
    {
        return 0U;
    }

    if (LDR_Average_Data(&adcValue) == 0U)
    {
        return 0U;
    }

    voltage = ((float)adcValue * 3.3f) / 4095.0f;

    if (voltage < 0.0001f)
    {
        voltage = 0.0001f;
    }
    else if (voltage > 3.2999f)
    {
        voltage = 3.2999f;
    }

    resistance = voltage / (3.3f - voltage) * 10000.0f;
    if (!(resistance > 0.0f))
    {
        return 0U;
    }

    lux = 40000.0f * powf(resistance, -0.6021f);
    if (!(lux >= 0.0f))
    {
        return 0U;
    }

    if (lux > 999.0f)
    {
        lux = 999.0f;
    }

    *light = (uint16_t)(lux + 0.5f);
    return 1U;
}

//void LDR_LuxData(uint16_t *Lux)
//{
//	float voltage = 0;	
//	float R = 0;	
//	voltage = LDR_Average_Data();
//	voltage  = voltage / 4096 * 3.3f;
//	
//	R = voltage / (3.3f - voltage) * 10000;
//		
//	*Lux = 40000 * pow(R, -0.6021);
//	
//	if (*Lux > 999)
//	{
//		*Lux = 999;
//	}
//}


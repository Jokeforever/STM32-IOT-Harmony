#include "TS.h"


uint16_t TS_ADC_Read(void)
{
	//设置指定ADC的规则组通道，采样时间
	return ADC_GetValue(ADC_CHANNEL_1, ADC_SAMPLETIME_55CYCLES_5);
}

uint16_t TS_GetData(uint16_t* moist)
{
	uint32_t  tempData = 0;
	for (uint8_t i = 0; i < TS_READ_TIMES; i++)
	{
		tempData += TS_ADC_Read();
		delay_ms(5);
	}

	tempData /= TS_READ_TIMES;
	if(tempData < 1241)
	{	
		*moist = 100;
			return *moist;
	}
	else if(tempData >= 4096)
	{
		*moist = 0;
		return *moist;
	}
	else
	{
//	return 100 - (float)tempData/40.96;
	*moist = (4096-tempData)*100 / (4096-1241);
	return *moist;
	}
	
}
	




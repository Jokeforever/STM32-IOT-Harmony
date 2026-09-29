#include "LDR.h"

	
uint16_t LDR_ADC_Read(void)
{
	//����ָ��ADC�Ĺ�����ͨ��������ʱ��
	return ADC_GetValue(ADC_CHANNEL1, ADC_SAMPLETIME_55CYCLES_5);
	
}
	
uint16_t LDR_Average_Data(void)
{
	uint32_t  tempData = 0;
	uint8_t i = 0;
	for ( i = 0;i < LDR_READ_TIMES;i++)
	{
		tempData += LDR_ADC_Read();
		HAL_Delay(5);
	}

	tempData /= LDR_READ_TIMES;
	return (uint16_t)tempData;
}

uint16_t LDR_LuxData(uint16_t *light)
{
	float voltage = 0;	
	float R = 0;	
	uint16_t Lux = 0;
	voltage = LDR_Average_Data();
	voltage  = voltage / 4096 * 3.3f;
	
	R = voltage / (3.3f - voltage) * 10000;
		
 	Lux = 40000 * pow(R, -0.6021);
	
	if (Lux > 999)
	{
		Lux = 999;
		
	}
	*light = Lux;
	return 1;
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


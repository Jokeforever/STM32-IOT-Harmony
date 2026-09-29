#ifndef _ADCX_H_
#define _ADCX_H_
#include "stm32f1xx_hal.h"                // Device header

// ADC 编号选择
// 可以是 ADC1/2/3





void ADCx_Init(void);
uint16_t ADC_GetValue(uint8_t ADC_Channel,uint8_t ADC_SampleTime);
void delay_us_systick(uint32_t uSec);
#endif

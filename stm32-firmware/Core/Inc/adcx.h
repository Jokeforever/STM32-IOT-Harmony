#ifndef _ADCX_H_
#define _ADCX_H_
#include "stm32f1xx_hal.h"                // Device header

// ADC 编号选择
// 可以是 ADC1/2/3





void ADCx_Init(void);
uint16_t ADC_GetValue(uint8_t ADC_Channel,uint8_t ADC_SampleTime);
uint8_t ADC_ReadChannel(uint8_t ADC_Channel, uint8_t ADC_SampleTime, uint16_t *value);
#define ADC_FILTER_MAX_SAMPLES 16U
uint8_t ADC_ReadFiltered(uint8_t ADC_Channel, uint8_t ADC_SampleTime, uint8_t sampleCount, uint8_t trimCount, uint16_t *value);
void delay_us_init(void);
void delay_us_systick(uint32_t uSec);
#endif

#include "adcx.h"
#include "stm32f1xx_hal.h" 
extern ADC_HandleTypeDef hadc1;

//uint16_t ADC_GetValue(uint8_t ADC_Channel,uint8_t ADC_SampleTime)
//{
//	//配置ADC通道
//	ADC_RegularChannelConfig(ADCx, ADC_Channel, 1, ADC_SampleTime);
//	
//	ADC_SoftwareStartConvCmd(ADCx, ENABLE); //软件触发ADC转换
//	while(ADC_GetFlagStatus(ADCx, ADC_FLAG_EOC) == RESET); //读取ADC转换完成标志位
//	return ADC_GetConversionValue(ADCx);
//}

uint16_t ADC_GetValue(uint8_t ADC_Channel,uint8_t ADC_SampleTime)
{
	//配置ADC通道
	ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel = ADC_Channel;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SampleTime; 
	
	HAL_ADC_ConfigChannel(&hadc1, &sConfig);
	
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1,50);
	uint32_t tmp = HAL_ADC_GetValue(&hadc1);
	HAL_ADC_Stop(&hadc1);
	return tmp;
}
	

void delay_us_systick(uint32_t uSec)
{
    uint32_t startTick = HAL_GetTick();
    uint32_t tickCounter = uSec / 1000;  // 转换为毫秒
    
    // 处理毫秒部分
    while ((HAL_GetTick() - startTick) < tickCounter)
    {
        // 等待毫秒部分
    }
    
    // 处理剩余微秒部分（<1000us）
    uint32_t remainingUsec = uSec % 1000;
    if (remainingUsec > 0)
    {
        uint32_t remainingTicks = remainingUsec * (SystemCoreClock / 1000000);
        uint32_t elapsed = 0;
        
        SysTick->VAL = 0;  // 重置SysTick计数器
        while (elapsed < remainingTicks)
        {
            elapsed = SysTick->LOAD - SysTick->VAL;
        }
    }
}

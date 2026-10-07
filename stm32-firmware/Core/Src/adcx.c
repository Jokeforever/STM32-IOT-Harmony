#include "adcx.h"
#include "stm32f1xx_hal.h" 
#include "delay.h"
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

uint8_t ADC_ReadChannel(uint8_t ADC_Channel, uint8_t ADC_SampleTime, uint16_t *value)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    if (value == NULL)
    {
        return 0U;
    }

    sConfig.Channel = ADC_Channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SampleTime;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        return 0U;
    }

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        (void)HAL_ADC_Stop(&hadc1);
        return 0U;
    }

    if (HAL_ADC_PollForConversion(&hadc1, 50U) != HAL_OK)
    {
        (void)HAL_ADC_Stop(&hadc1);
        return 0U;
    }

    *value = (uint16_t)HAL_ADC_GetValue(&hadc1);

    if (HAL_ADC_Stop(&hadc1) != HAL_OK)
    {
        return 0U;
    }

    return 1U;
}

uint16_t ADC_GetValue(uint8_t ADC_Channel,uint8_t ADC_SampleTime)
{
    uint16_t value = 0U;

    if (ADC_ReadChannel(ADC_Channel, ADC_SampleTime, &value) == 0U)
    {
        return 0U;
    }

    return value;
}

static uint8_t delay_us_ready = 0U;

void delay_us_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    if ((DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) == 0U)
    {
        delay_us_ready = 1U;
    }
    else
    {
        delay_init();
    }
}

void delay_us_systick(uint32_t uSec)
{
    uint32_t start;
    uint32_t ticks;

    if (delay_us_ready == 0U)
    {
        delay_us(uSec);
        return;
    }

    ticks = uSec * (SystemCoreClock / 1000000U);
    start = DWT->CYCCNT;

    while ((DWT->CYCCNT - start) < ticks)
    {
    }
}

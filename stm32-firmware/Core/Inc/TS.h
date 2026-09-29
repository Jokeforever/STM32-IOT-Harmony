#ifndef __TS_H
#define	__TS_H
#include "main.h"
#include "adcx.h"
#include "delay.h"
#include "math.h"

/*****************辰哥单片机设计******************
											STM32
 * 文件			:	土壤湿度传感器h文件                   
 * 版本			: V1.0
 * 日期			: 2024.8.12
 * MCU			:	STM32F103C8T6
 * 接口			:	见代码						
 * IP账号		:	辰哥单片机设计（同BILIBILI|抖音|快手|小红书|CSDN|公众号|视频号等）
 * 作者			:	辰哥
 * 工作室		: 异方辰电子工作室
 * 讲解视频	:	https://www.bilibili.com/video/BV17z421B79w/?share_source=copy_web
 * 官方网站	:	www.yfcdz.cn

**********************BEGIN***********************/	

#define TS_READ_TIMES	10  //土壤湿度ADC循环读取次数


/***************根据自己需求更改****************/
// TS GPIO宏定义

#define		TS_GPIO_CLK								RCC_APB2Periph_GPIOA
#define 	TS_GPIO_PORT							GPIOA
#define 	TS_GPIO_PIN								GPIO_Pin_1
#define   ADC_CHANNEL               ADC_Channel_1	// ADC 通道宏定义

/*********************END**********************/


void TS_Init(void);
uint16_t TS_GetData(uint16_t* moist);

#endif /* __ADC_H */


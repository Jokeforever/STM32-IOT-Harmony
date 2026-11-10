/* 输出端口分离前的蜂鸣器头，仅主机对照；只清理行尾空白。 */
#ifndef _BEEP_H_
#define _BEEP_H_


#include "main.h"
#include "usart3.h"
#include "LED.h"
#include "bump.h"
#include "fan.h"
#include "jdq.h"



#define BEEP_ON		HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_RESET)

#define BEEP_OFF	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_SET)



void Beep_Init(void);

void Beep_Set(_Bool status);

void IS_Normal(void);

void Handler(void);

#endif

#ifndef __FAN_H
#define	__FAN_H
#include "main.h"




#define	FAN_CLK							RCC_APB2Periph_GPIOB

#define FAN_GPIO_PIN 				GPIO_PIN_5
                              

#define FAN_GPIO_PROT 			GPIOB

#define FAN_ON 		HAL_GPIO_WritePin(FAN_GPIO_PROT,FAN_GPIO_PIN,GPIO_PIN_RESET);
#define FAN_OFF 	HAL_GPIO_WritePin(FAN_GPIO_PROT,FAN_GPIO_PIN,GPIO_PIN_SET);





void FAN_Init(void);

#endif




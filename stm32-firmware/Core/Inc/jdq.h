#ifndef __JDQ_H
#define	__JDQ_H
#include "main.h"
#include "fan.h"




#define	JDQ_CLK							RCC_APB2Periph_GPIOB

#define JDQ_GPIO_PIN 				GPIO_PIN_6

#define JDQ_GPIO_PORT 				GPIOB

#define JDQ_ON 		HAL_GPIO_WritePin(JDQ_GPIO_PORT,JDQ_GPIO_PIN,GPIO_PIN_RESET);
#define JDQ_OFF 	HAL_GPIO_WritePin(JDQ_GPIO_PORT,JDQ_GPIO_PIN,GPIO_PIN_SET);


#define TEMP_THREHOLD  (int)25
#define TEMP_HYSTERESIS 1 


void JDQ_Init(void);
void Monitor_Temp(void);

#endif




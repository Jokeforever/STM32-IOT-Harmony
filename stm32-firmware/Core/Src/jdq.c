#include "jdq.h"

extern uint8_t temp;
uint8_t temp_threhold ;

void JDQ_Init(void)
{
	temp_threhold = TEMP_THREHOLD;
	
	JDQ_OFF;
}

void Monitor_Temp(void)
{
	if(temp > temp_threhold + TEMP_HYSTERESIS)
	{
		FAN_ON;
	}
	else if(temp < temp_threhold - TEMP_HYSTERESIS)
	{
		JDQ_ON;
	}
	else
	{
		JDQ_OFF;
		FAN_OFF;
	}
}


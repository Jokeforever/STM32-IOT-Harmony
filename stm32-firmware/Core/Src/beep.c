#include "beep.h"



void Beep_Init(void)
{

	
	BEEP_OFF;		

}

extern uint16_t light,moist,ppm;
extern volatile Abnormal abnormal[3];
extern volatile DisplayScreen CurrentScreen;
extern uint16_t CO2_Threhold ;
extern uint16_t LDR_threhold[2] ;
extern uint16_t Bump_threhold[2] ;
extern System_Mode current_mode ;
extern Bump_Mode CurrentBump_mode;

void IS_Normal(void)
{
	for(int i = 0; i<3;i++)
	{
		switch(i)
		{
			case 0:
				if(light > LDR_threhold[1])
				{
					abnormal[i] = BRIGHT;

				}
			else if(light<LDR_threhold[0])
			{
				abnormal[i] = DARK;

			}
			else
			abnormal[i] = NONE;
				break;
			case 1:
			if(moist> Bump_threhold[1])
			{
				abnormal[i] = DAMP;

			}
			else if(moist< Bump_threhold[0]   )
			{
				abnormal[i] = DROUGHT;

			}
			else
				abnormal[i] = NONE;
				break;
			case 2:
			if(ppm>CO2_Threhold)
			{
				abnormal[i] = HIGHCO2;

			}
			else
			{
				abnormal[i] = NONE;
			}
				break;	
		}
	}
		Handler();
}

void Handler(void)
{
	for(int i = 0; i<3;i++)
	{
		switch(i)
		{
			case 0:
				if(abnormal[i] != NONE)
				{ 
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
				}
				else
				{
					HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
				}
				break;
			case 1:
				if(abnormal[i] != NONE)
				{
						BEEP_ON;
				}
				else
				{
						BEEP_OFF;
				}
				break;
			case 2:
				if(abnormal[i] == HIGHCO2)
				{
					FAN_ON;
				}
				else
				{
					FAN_OFF;
				}
				break;
		}
	}
}

#include "app_config.h"
#include "app_sensors.h"
#include "beep.h"
#include "Display.h" /* 显式依赖显示枚举，不再经LED.h间接获取。 */



void Beep_Init(void)
{

	
	BEEP_OFF;		

}

extern volatile Abnormal abnormal[3];
extern volatile DisplayScreen CurrentScreen;

void IS_Normal(void)
{
    const SensorData *sensors = SensorService_GetData();
	for(int i = 0; i<3;i++)
	{
		switch(i)
		{
			case 0:
				if(sensors->light_lux > AppConfig_Get()->ldr_high)
				{
					abnormal[i] = BRIGHT;

				}
			else if(sensors->light_lux<AppConfig_Get()->ldr_low)
			{
				abnormal[i] = DARK;

			}
			else
			abnormal[i] = NONE;
				break;
			case 1:
			if(sensors->soil_moisture_pct> AppConfig_Get()->soil_high)
			{
				abnormal[i] = DAMP;

			}
			else if(sensors->soil_moisture_pct< AppConfig_Get()->soil_low   )
			{
				abnormal[i] = DROUGHT;

			}
			else
				abnormal[i] = NONE;
				break;
			case 2:
			if(((sensors->valid_mask & SENSOR_VALID_CO2) != 0U) && (sensors->co2_ppm > AppConfig_Get()->co2_threshold))
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
			/* 风扇由 Monitor_Temp() 统一合并温度和 CO2 请求。 */
		}
	}
}

#include "app_config.h"
#include "app_sensors.h"
#include "beep.h"
#include "mw_alarm.h" /* 报警类型/规则独立于显示层。 */



void Beep_Init(void)
{

	
	BEEP_OFF;		

}

extern volatile Abnormal abnormal[3];

void IS_Normal(void)
{
    const SensorData *sensors = SensorService_GetData();
    const RuntimeConfig *config = AppConfig_Get();
    AlarmThresholds thresholds;
    Abnormal evaluated[ALARM_COUNT];
    uint8_t i;

    thresholds.light_low = config->ldr_low;
    thresholds.light_high = config->ldr_high;
    thresholds.soil_low = config->soil_low;
    thresholds.soil_high = config->soil_high;
    thresholds.co2_threshold = config->co2_threshold;

    if (Alarm_Evaluate(sensors, &thresholds, evaluated) != 0U)
    {
        for (i = 0U; i < ALARM_COUNT; ++i)
        {
            abnormal[i] = evaluated[i];
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

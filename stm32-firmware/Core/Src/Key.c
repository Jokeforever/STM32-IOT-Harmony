#include "app_config.h"
#include "Key.h"
#include "mw_key_port.h"
#include "mw_config.h"

extern volatile DisplayScreen CurrentScreen;
//extern   uint8_t Key1,Key2 ;

extern Key Keys[4];
uint8_t Monitor_para1 = 0; /* 参数页4：0光照下限、1光照上限、2通风上限、6加热下限；页5：3土壤下限、4上限、5 CO2。 */

void Key_Scan(void)
{
    uint8_t pressed = KeyPort_ReadPressedMask();

    Keys[0].Pin_state = (uint8_t)((pressed & KEY_PORT_KEY1_MASK) != 0U);
    Keys[1].Pin_state = (uint8_t)((pressed & KEY_PORT_KEY2_MASK) != 0U);
    Keys[2].Pin_state = (uint8_t)((pressed & KEY_PORT_KEY3_MASK) != 0U);
    Keys[3].Pin_state = (uint8_t)((pressed & KEY_PORT_KEY4_MASK) != 0U);
	
	
	for(uint32_t i = 0; i<4;i++)
	{
		switch(Keys[i].State)
		{
			case 0:
				if(Keys[i].Pin_state)
				{
					Keys[i].Presstime++;
				}
				if(!Keys[i].Pin_state)
				{
					if(Keys[i].Presstime>3)
						Keys[i].State = 1;
					else
						Keys[i].Presstime = 0;
				}
				break;
			case 1:
				if(!Keys[i].Pin_state)
				{
					Keys[i].Releasetime++;
				}
				if(Keys[i].Releasetime>3)
				{
					Keys[i].State=2;
				}
				break;
			case 2:
				if(Keys[i].Presstime>150)
					Keys[i].LongClickFlag = 1;
				Keys[i].ClickFlag = 1;
				Keys[i].Presstime = 0;
				Keys[i].Releasetime = 0;
				break;
			default:
				Keys[i].State =0;
				break;
		}
	}
}

/* 页面字段选择与原编号一致，所有增减都走同一个饱和检查入口。 */
static void Key_AdjustParameter(int direction)
{
    if (CurrentScreen == DISPLAY_SCREEN4)
    {
        switch (Monitor_para1)
        {
            case 0: (void)AppConfig_Adjust(APP_CFG_LIGHT_LOW, direction * LDR_STEP); break;
            case 1: (void)AppConfig_Adjust(APP_CFG_LIGHT_HIGH, direction * LDR_STEP); break;
            case 2:
                (void)AppConfig_Adjust(APP_CFG_TEMP_THRESHOLD, direction * TEMP_STEP);
                break;
            case 6: (void)AppConfig_Adjust(APP_CFG_TEMP_LOW_THRESHOLD, direction * TEMP_STEP); break;
            default: Monitor_para1 = 0; break;
        }
    }
    else if (CurrentScreen == DISPLAY_SCREEN5)
    {
        switch (Monitor_para1)
        {
            case 3: (void)AppConfig_Adjust(APP_CFG_SOIL_LOW, direction * BUMP_STEP); break;
            case 4: (void)AppConfig_Adjust(APP_CFG_SOIL_HIGH, direction * BUMP_STEP); break;
            case 5: (void)AppConfig_Adjust(APP_CFG_CO2_THRESHOLD, direction * CO2_STEP); break;
            default: Monitor_para1 = 3; break;
        }
    }
}

void Key_Func(void)
{
    const RuntimeConfig *config = AppConfig_Get();
    if (Keys[0].ClickFlag)
    {
        CurrentScreen = (DisplayScreen)((CurrentScreen + 1) % 6);
        OLED_Clear();
        switch (CurrentScreen)
        {
            case DISPLAY_SCREEN1: Display1_Init(); break;
            case DISPLAY_SCREEN2: Display2_Init(); break;
            case DISPLAY_SCREEN3: Display3_Init(); break;
            case DISPLAY_SCREEN4:
                if (Monitor_para1 != 0U && Monitor_para1 != 1U && Monitor_para1 != 2U && Monitor_para1 != 6U) Monitor_para1 = 0U;
                Display4_Init(); break;
            case DISPLAY_SCREEN5:
                if (Monitor_para1 < 3U || Monitor_para1 > 5U) Monitor_para1 = 3U;
                Display5_Init(); break;
            default: break;
        }
        Keys[0].ClickFlag = 0; Keys[0].State = 0;
    }
    if (Keys[1].ClickFlag)
    {
        if (CurrentScreen == DISPLAY_SCREEN4)
        {
            switch (Monitor_para1) {
                case 0: Monitor_para1 = 1; break;
                case 1: Monitor_para1 = 2; break;
                case 2: Monitor_para1 = 6; break;
                case 6: Monitor_para1 = 0; break;
                default: Monitor_para1 = 0; break;
            }
        }
        else if (CurrentScreen == DISPLAY_SCREEN5)
        {
            if (Monitor_para1 < 3U || Monitor_para1 > 5U) Monitor_para1 = 3U;
            else Monitor_para1 = (Monitor_para1 == 5U) ? 3U : (uint8_t)(Monitor_para1 + 1U);
        }
        else if (config->led_mode == MODE_CALIBRATION && CurrentScreen == DISPLAY_SCREEN2)
        {
            if (config->led_manual_pwm < 100) (void)AppConfig_Adjust(APP_CFG_MANUAL_PWM, 5);
            else (void)AppConfig_Set(APP_CFG_MANUAL_PWM, 0); /* 保留100后回0的手动调光操作。 */
        }
        Keys[1].ClickFlag = 0; Keys[1].State = 0;
    }
    if (Keys[2].ClickFlag)
    {
        if (CurrentScreen == DISPLAY_SCREEN4 || CurrentScreen == DISPLAY_SCREEN5)
            Key_AdjustParameter(1);
        else
        {
            int mode = Keys[2].LongClickFlag ?
                (config->led_mode != MODE_CALIBRATION ? MODE_CALIBRATION : AppConfig_PreviousLedMode()) :
                (config->led_mode + 1) % 3;
            LED_Set(mode);
            Mode_Switch();
        }
        Keys[2].ClickFlag = 0; Keys[2].LongClickFlag = 0; Keys[2].State = 0;
    }
    if (Keys[3].ClickFlag)
    {
        if (CurrentScreen == DISPLAY_SCREEN4 || CurrentScreen == DISPLAY_SCREEN5)
            Key_AdjustParameter(-1);
        else
        {
            int mode = Keys[3].LongClickFlag ?
                (config->bump_mode != Bump_CALIBRATION ? Bump_CALIBRATION : AppConfig_PreviousBumpMode()) :
                (config->bump_mode + 1) % 3;
            Bump_Set(mode);
        }
        Keys[3].ClickFlag = 0; Keys[3].LongClickFlag = 0; Keys[3].State = 0;
    }
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) {
        Key_Scan();
        Bump_OnTimerTick();
    }
}

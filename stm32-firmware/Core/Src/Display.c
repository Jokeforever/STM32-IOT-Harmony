#include "app_config.h"
#include "app_sensors.h"
#include "Display.h"

//extern   uint8_t Key1,Key2 ;
extern float water_vol ;
extern volatile DisplayScreen CurrentScreen;
extern uint8_t Monitor_para1; /* 本机参数选择，仅用于绘制光标。 */
extern volatile Abnormal abnormal[3];

void Display1_Init(void)
{
	
	
	OLED_ShowCHinese(0,0,1);//温    
	OLED_ShowCHinese(18,0,2);//度     
	OLED_ShowCHinese(96,0,3);//℃  

	                                
	OLED_ShowCHinese(0,3,4);//湿    
	OLED_ShowCHinese(18,3,5);//度   
	OLED_ShowString(96,3,(uint8_t*)"%",16);//%
	
	OLED_ShowString(0,6,(uint8_t*)"CO2",16);
	OLED_ShowString(96,6,(uint8_t*)"ppm",16);                                
	
}

void Display2_Init(void)
{
	OLED_ShowCHinese(0,0,6);//光  
	OLED_ShowCHinese(18,0,7);//强
	OLED_ShowString(80,0,(uint8_t*)"Lux",16);//Lux
	
	OLED_ShowString(0,3,(uint8_t*)"LEDMode:",16);
	

}

void Display3_Init(void)
{

	
	OLED_ShowCHinese(0,0,16);//土  
	OLED_ShowCHinese(18,0,17);//壤
	OLED_ShowCHinese(36,0,4);//湿 
	OLED_ShowCHinese(54,0,5);//度
	OLED_ShowString(112,0,(uint8_t*)"%",16);//%
	
	OLED_ShowCHinese(0,3,14);//水
	OLED_ShowCHinese(18,3,15);//泵
	OLED_ShowCHinese(36,3,0);//:
	
	
	OLED_ShowCHinese(0,6,14);//水
	OLED_ShowCHinese(18,6,20);//量

}

void Display4_Init(void)
{
    /* 16像素字体占两页，0/2/4/6恰好排满64像素，不增加屏幕页。 */
    OLED_ShowString(12,0,(uint8_t*)"LL:",16);
    OLED_ShowString(12,2,(uint8_t*)"LH:",16);
    OLED_ShowString(12,4,(uint8_t*)"TH:",16);
    OLED_ShowString(12,6,(uint8_t*)"TL:",16);
}
void Display5_Init(void)
{
    /* 与参数页4统一：16像素字、每行两页、左侧光标。 */
    OLED_ShowString(12,0,(uint8_t*)"BL:",16);
    OLED_ShowString(12,2,(uint8_t*)"BH:",16);
    OLED_ShowString(12,4,(uint8_t*)"CO:",16);
}

//数据刷新显示1，2
void Refresh1_Data(void)
{
    const SensorData *sensors = SensorService_GetData();
		char buf1[10];
		sprintf(buf1,":%2d  ",sensors->temperature_c);
		OLED_ShowString(36,0,(uint8_t*)buf1,16); 
	
		sprintf(buf1,":%2d  ",sensors->humidity_pct);
		OLED_ShowString(36,3,(uint8_t*)buf1,16);  
	
		sprintf(buf1,":%2d  ",sensors->co2_ppm);
		OLED_ShowString(36,6,(uint8_t*)buf1,16);  
	

}

void Refresh2_Data(void)
{
    const SensorData *sensors = SensorService_GetData();
		char buf2[10];
	
		sprintf(buf2,":%2d ",sensors->light_lux);
		OLED_ShowString(36,0,(uint8_t*)buf2,16);  
	
		LED_Display();
		
	
}

void Refresh3_Data(void)
{
    const SensorData *sensors = SensorService_GetData();
		char buf3[10];
	
		sprintf(buf3,":%2d  ",sensors->soil_moisture_pct);
		OLED_ShowString(72,0,(uint8_t*)buf3,16);  
	
		Bump_Display();
		
		sprintf(buf3,":%.1f  ",water_vol);	
		OLED_ShowString(36,6,(uint8_t*)buf3,16); 
		
	
}
void Refresh4_Data(void)
{
    const RuntimeConfig *config = AppConfig_Get();
    char buf4[10];
    OLED_ShowString(0,0,(uint8_t*)(Monitor_para1 == 0U ? ">" : " "),16);
    sprintf(buf4,"%4u ",(unsigned)config->ldr_low);
    OLED_ShowString(48,0,(uint8_t*)buf4,16);
    OLED_ShowString(0,2,(uint8_t*)(Monitor_para1 == 1U ? ">" : " "),16);
    sprintf(buf4,"%4u ",(unsigned)config->ldr_high);
    OLED_ShowString(48,2,(uint8_t*)buf4,16);
    OLED_ShowString(0,4,(uint8_t*)(Monitor_para1 == 2U ? ">" : " "),16);
    sprintf(buf4,"%3uC ",(unsigned)config->temp_threshold);
    OLED_ShowString(48,4,(uint8_t*)buf4,16);
    OLED_ShowString(0,6,(uint8_t*)(Monitor_para1 == 6U ? ">" : " "),16);
    sprintf(buf4,"%3uC ",(unsigned)config->temp_low_threshold);
    OLED_ShowString(48,6,(uint8_t*)buf4,16);
}
void Refresh5_Data(void)
{
    const RuntimeConfig *config = AppConfig_Get();
    char buf5[10];
    OLED_ShowString(0,0,(uint8_t*)(Monitor_para1 == 3U ? ">" : " "),16);
    sprintf(buf5,"%3u%% ",(unsigned)config->soil_low);
    OLED_ShowString(48,0,(uint8_t*)buf5,16);
    OLED_ShowString(0,2,(uint8_t*)(Monitor_para1 == 4U ? ">" : " "),16);
    sprintf(buf5,"%3u%% ",(unsigned)config->soil_high);
    OLED_ShowString(48,2,(uint8_t*)buf5,16);
    OLED_ShowString(0,4,(uint8_t*)(Monitor_para1 == 5U ? ">" : " "),16);
    sprintf(buf5,"%4u ",(unsigned)config->co2_threshold);
    OLED_ShowString(48,4,(uint8_t*)buf5,16);
}

char bufdat[20];
extern LED_Control led_ctrl;
void Display_Data(void)
{
	        switch(CurrentScreen)
        {
            case DISPLAY_SCREEN1:
                Refresh1_Data();
                break;
                
            case DISPLAY_SCREEN2:
                Refresh2_Data();
                break;
            case DISPLAY_SCREEN3:
                Refresh3_Data();
                break;
						case DISPLAY_SCREEN4:
								Refresh4_Data();
                break;
						case DISPLAY_SCREEN5:
								Refresh5_Data();
								break;
            default:
                // 无显示状态
						OLED_ShowCHinese(50,0,28);// 
						OLED_ShowCHinese(66,0,29);// 
						OLED_ShowCHinese(0,3,21);// 
						OLED_ShowCHinese(16,3,22);// 
						OLED_ShowCHinese(32,3,23);// 
						OLED_ShowCHinese(48,3,24);// 
						OLED_ShowCHinese(64,3,25);// 
						OLED_ShowCHinese(80,3,26);// 
						OLED_ShowCHinese(96,3,27);// 
                break;
        }
		
}


/* 模式绘制集中于显示层；兼容声明仍位于LED.h/bump.h，调用方式不变。 */
void LED_Display(void)
{
	 switch (AppConfig_Get()->led_mode)
				{
						case MODE_OFF:
						OLED_ShowCHinese(64,3,10);//
						OLED_ShowCHinese(80,3,11);//
						break;

            case MODE_CALIBRATION:
						OLED_ShowCHinese(64,3,8);//
						OLED_ShowCHinese(80,3,9);//
             break;

            case MODE_AUTO_PID:
						OLED_ShowCHinese(64,3,12);//
						OLED_ShowCHinese(80,3,13);//
						break;
				}
}

void Bump_Display(void)
{
	switch(AppConfig_Get()->bump_mode)
	{
		case Bump_OFF:
						OLED_ShowCHinese(54,3,10);//
						OLED_ShowCHinese(72,3,11);//
						break;
    case Bump_CALIBRATION:
						OLED_ShowCHinese(54,3,8);//
						OLED_ShowCHinese(72,3,9);//
           break;

    case Bump_AUTO:
						OLED_ShowCHinese(54,3,12);//
						OLED_ShowCHinese(72,3,13);//
						break;
	}
}

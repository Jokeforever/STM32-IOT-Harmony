#include "Display.h"

//extern   uint8_t Key1,Key2 ;
extern uint8_t temp,humi;
extern uint16_t light,ppm,moist;
extern float water_vol ;
extern volatile DisplayScreen CurrentScreen;
extern volatile Abnormal abnormal[3];
extern uint16_t CO2_Threhold ;
extern uint8_t temp_threhold ;
extern uint16_t Bump_threhold[2] ;
extern uint16_t LDR_threhold[2] ;

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
	OLED_ShowString(0,0,(uint8_t*)"LH;",16);
	OLED_ShowString(0,3,(uint8_t*)"LL;",16);
	OLED_ShowString(0,6,(uint8_t*)"TH;",16);
}
void Display5_Init(void)
{
	OLED_ShowString(0,0,(uint8_t*)"BH;",12);
	OLED_ShowString(0,3,(uint8_t*)"BL;",12);
	OLED_ShowString(0,6,(uint8_t*)"CO;",16);

}

//数据刷新显示1，2
void Refresh1_Data(void)
{
		char buf1[10];
		sprintf(buf1,":%2d  ",temp);
		OLED_ShowString(36,0,(uint8_t*)buf1,16); 
	
		sprintf(buf1,":%2d  ",humi);	
		OLED_ShowString(36,3,(uint8_t*)buf1,16);  
	
		sprintf(buf1,":%2d  ",ppm);	
		OLED_ShowString(36,6,(uint8_t*)buf1,16);  
	

}

void Refresh2_Data(void)
{
		char buf2[10];
	
		sprintf(buf2,":%2d ",light);	
		OLED_ShowString(36,0,(uint8_t*)buf2,16);  
	
		LED_Display();
		
	
}

void Refresh3_Data(void)
{
		char buf3[10];
	
		sprintf(buf3,":%2d  ",moist);	
		OLED_ShowString(72,0,(uint8_t*)buf3,16);  
	
		Bump_Display();
		
		sprintf(buf3,":%.1f  ",water_vol);	
		OLED_ShowString(36,6,(uint8_t*)buf3,16); 
		
	
}
void Refresh4_Data(void)
{
	char buf4[10];
	
	sprintf(buf4,"%d  ",LDR_threhold[0]);	
	OLED_ShowString(48,0,(uint8_t*)buf4,16);  
	
	sprintf(buf4,"%d  ",LDR_threhold[1]);	
	OLED_ShowString(48,3,(uint8_t*)buf4,16); 
	
	sprintf(buf4,"%d  ",temp_threhold);	
	OLED_ShowString(48,6,(uint8_t*)buf4,16);  
	

}
void Refresh5_Data(void)
{
	
	char buf5[10];
	
		sprintf(buf5,"%d  ",Bump_threhold[0]);	
	OLED_ShowString(48,0,(uint8_t*)buf5,16);  
	
	sprintf(buf5,"%d  ",Bump_threhold[1]);	
	OLED_ShowString(48,3,(uint8_t*)buf5,16); 

	
	sprintf(buf5,"%d  ",CO2_Threhold);	
	OLED_ShowString(48,6,(uint8_t*)buf5,16); 
	
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

#include "Key.h"

extern volatile DisplayScreen CurrentScreen;
//extern   uint8_t Key1,Key2 ;
extern uint8_t temp,humi;
extern uint16_t light;
extern uint16_t ppm;     

extern uint32_t watertime;
extern Bump_Mode CurrentBump_mode ;
extern Bump_Mode LastBump_mode ;
extern uint8_t waterflag; 
extern volatile uint32_t time_ms;

extern volatile uint32_t hp_ms_total ;       // 本次高压总时长
extern volatile uint32_t hp_ms_count;      // 已运行时长
extern volatile uint8_t hp_running ;        // 是否正在高压模式
extern volatile uint8_t hp_is_on;          // 当前水泵开/关（0=关,1=开）
extern volatile uint32_t hp_state_start ;   // 当前状态开始时刻
extern uint32_t HP_ON_MS ;                 // 高压开 0.5s
extern uint32_t HP_OFF_MS ;              // 高压关 1.5s
 
extern System_Mode current_mode ;
extern System_Mode last_mode ;



extern uint16_t CO2_Threhold ;
extern uint8_t temp_threhold ;
extern uint16_t Bump_threhold[2] ;
extern uint16_t LDR_threhold[2] ;

extern Key Keys[4];
uint8_t Monitor_para1 = 0; //0 = LDRH,1 = LDRL,2 = BUMPH,3 = BUMPL  4 = TEMPH,5=TEMPL,6=CO2
uint8_t LED_Manu_PWM = 50;

void Key_Scan(void)
{
	Keys[0].Pin_state = HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_4) == RESET?1:0;//page
	Keys[1].Pin_state = HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_5) == RESET?1:0;//page2
	Keys[2].Pin_state = HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_7) == RESET?1:0;//LED
	Keys[3].Pin_state = HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_6) == RESET?1:0;//bump
	
	
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

void Key_Func(void)
{
	if(Keys[0].ClickFlag)
	{
		CurrentScreen = (CurrentScreen + 1)%6;
		if(CurrentScreen == DISPLAY_SCREEN1)
		{
			OLED_Clear();
			Display1_Init();
		}
		else 	if(CurrentScreen == DISPLAY_SCREEN2)
		{
			OLED_Clear();
			Display2_Init();
		}
		else if(CurrentScreen == DISPLAY_SCREEN3)
		{
			OLED_Clear();
			Display3_Init();
		}
		else if(CurrentScreen == DISPLAY_SCREEN4)
		{
			OLED_Clear();
			Display4_Init();
		}else if(CurrentScreen == DISPLAY_SCREEN5)
		{
			OLED_Clear();
			Display5_Init();
		}
		else
		{
			OLED_Clear();
		}
		Keys[0].ClickFlag = 0;
		Keys[0].State = 0;
	}
	if(Keys[1].ClickFlag)
	{
		 if(CurrentScreen == DISPLAY_SCREEN4)
		{	
			if(Monitor_para1>3)
				Monitor_para1 = 0;
			Monitor_para1 = (Monitor_para1 + 1)%3;
				
		}
			else if(CurrentScreen == DISPLAY_SCREEN5)
		{	
			if(Monitor_para1<3)
				Monitor_para1 = 3;
			Monitor_para1 = (Monitor_para1 + 1)%6;	
		}
		else
		{
			if(current_mode == MODE_CALIBRATION && CurrentScreen == DISPLAY_SCREEN2)
			{
				if(LED_Manu_PWM < 100)
				LED_Manu_PWM += 5;
				else
				LED_Manu_PWM = 0;
			}
		}
		if((CurrentScreen == DISPLAY_SCREEN2) && (current_mode == MODE_CALIBRATION))
		{
			Config_MarkDirty();
		}
		Keys[1].ClickFlag = 0;
		Keys[1].State = 0;
	}
	if(Keys[2].ClickFlag)
	{
			 if(CurrentScreen == DISPLAY_SCREEN4)
		{
			switch(Monitor_para1)
			{
				case 0:
					if(LDR_threhold[0]<LDR_threhold[1])
					LDR_threhold[0]+=LDR_STEP;
					else
					LDR_threhold[0] = LDR_threhold[1];
					break;
				case 1:
					if(LDR_threhold[1] < 9999)
					LDR_threhold[1]+=LDR_STEP;
					else
					LDR_threhold[1]= 9999;
					break;
				case 2:
					if(temp_threhold<100)
						temp_threhold += TEMP_STEP;
					else
						temp_threhold = 100;
				default:
					Monitor_para1 = 0;
			}
			
		}
		else if(CurrentScreen == DISPLAY_SCREEN5)
		{
			switch(Monitor_para1)
			{
				case 3:
					if(Bump_threhold[0]<Bump_threhold[1])
					Bump_threhold[0]+=BUMP_STEP;
					else
					Bump_threhold[0] = Bump_threhold[1];
					break;
				case 4:
					if(Bump_threhold[1] < 100)
					Bump_threhold[1]+=BUMP_STEP;
					else
					Bump_threhold[1]= 100;
					break;
				case 5:
					if(CO2_Threhold < 4000)
					CO2_Threhold+= CO2_STEP;
					else
					CO2_Threhold = 4000;
					break;
				default:
					Monitor_para1 = 3;

			}
		}
		else
		{
		if(Keys[2].LongClickFlag == 1)
		{
			if (current_mode != MODE_CALIBRATION)
				{
                last_mode = current_mode;
                current_mode = MODE_CALIBRATION;
				} 
				else 
				{
				current_mode = last_mode;
       }
        Mode_Switch();
		}
		else
		{
			last_mode = current_mode;
			current_mode = (current_mode + 1) % 3; 
      Mode_Switch();  
		}
		}
		if((CurrentScreen == DISPLAY_SCREEN4) || (CurrentScreen == DISPLAY_SCREEN5))
		{
			Config_MarkDirty();
		}
		Keys[2].ClickFlag = 0;
		Keys[2].LongClickFlag = 0;
		Keys[2].State = 0;
	}
	if(Keys[3].ClickFlag)
	{
		 if(CurrentScreen == DISPLAY_SCREEN4)
		{
			switch(Monitor_para1)
			{
				case 0:
					if(LDR_threhold[0]>0)
					LDR_threhold[0]-=LDR_STEP;
					else
					LDR_threhold[0] = 0;
					break;
				case 1:
					if(LDR_threhold[1] > LDR_threhold[0])
					LDR_threhold[1]-=LDR_STEP;
					else
					LDR_threhold[1]= LDR_threhold[0];
					break;
				case 2:
					if(temp_threhold>0)
						temp_threhold-=TEMP_STEP;
					else
						temp = 0;
				default:
					Monitor_para1 = 0;
			}
			
		}
		else if(CurrentScreen == DISPLAY_SCREEN5)
		{
			switch(Monitor_para1)
			{
				case 3:
					if(Bump_threhold[0]>0)
					Bump_threhold[0]-=BUMP_STEP;
					else
					Bump_threhold[0] = 0;
					break;
				case 4:
					if(Bump_threhold[1] > Bump_threhold[0])
					Bump_threhold[1]-=BUMP_STEP;
					else
					Bump_threhold[1]= Bump_threhold[0];
					break;
				case 5:
					if(CO2_Threhold >350)
					CO2_Threhold-= CO2_STEP;
					else
					CO2_Threhold = 350;
					break;
				default:
					Monitor_para1 = 3;

			}
		}
		else
		{
		if(Keys[3].LongClickFlag == 1)
		{
			if (CurrentBump_mode != Bump_CALIBRATION)
				{
                LastBump_mode =  CurrentBump_mode;
                CurrentBump_mode = Bump_CALIBRATION;
				} 
				else 
				{
					CurrentBump_mode = LastBump_mode;
				}
		}
		else
		{
				LastBump_mode = CurrentBump_mode;
				CurrentBump_mode = (CurrentBump_mode + 1) % 3; 
		}
	}
		if((CurrentScreen == DISPLAY_SCREEN4) || (CurrentScreen == DISPLAY_SCREEN5))
		{
			Config_MarkDirty();
		}
		Keys[3].ClickFlag = 0;
		Keys[3].LongClickFlag = 0;
		Keys[3].State = 0;
	}
		

}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) 
{
	if(htim->Instance == TIM1)
	{
				Key_Scan();
				if(waterflag && CurrentBump_mode == Bump_AUTO)
			{
				watertime++;
				if(watertime*10>time_ms)  // 灌溉指定时间
				{
					time_ms = 0;
					watertime = 0;
					waterflag = 0;	
					BUMP_OFF;  // 关闭水泵
				}
			}
			
			else	if (hp_running && CurrentBump_mode == Bump_CALIBRATION) {
            hp_ms_count++; // 总时间累计
            
            uint32_t dur = HAL_GetTick() - hp_state_start;
            
            if (hp_is_on) {
                // 正在开：检查是否到 0.5s
                if (dur >= HP_ON_MS) {
                    BUMP_OFF;
                    hp_is_on = 0;
                    hp_state_start = HAL_GetTick();
                }
            } else {
                // 正在关：检查是否到 1.5s
                if (dur >= HP_OFF_MS) {
                    // 总时间没到就继续下一轮
                    if (hp_ms_count < hp_ms_total) {
                        BUMP_ON;
                        hp_is_on = 1;
                        hp_state_start = HAL_GetTick();
                    }
                }
            }
            
            // 高压总时间到
            if (hp_ms_count*10 >= hp_ms_total) {
                hp_running = 0;
                hp_ms_count = 0;
                hp_ms_total = 0;
                hp_is_on = 0;
								BUMP_OFF;
								CurrentBump_mode = Bump_OFF;
            }
			
					}
	}
}

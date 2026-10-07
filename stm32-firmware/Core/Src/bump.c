  #include "bump.h"

extern uint16_t moist ;
extern float water_vol ;

volatile uint32_t watertime ;
volatile uint8_t waterflag = 0;
volatile float time_min  = 0;
volatile uint32_t time_ms = 0;
uint16_t Bump_threhold[2] ;

Bump_Mode CurrentBump_mode = Bump_AUTO;
Bump_Mode LastBump_mode = Bump_AUTO;

void BUMP_Init(void)
{

	BUMP_OFF;
	Bump_threhold[0] = HUMI_THRESHOLD_LOW;
	Bump_threhold[1] = HUMI_THRESHOLD_HIGH;
}

float Calculate_Water_Volume(uint16_t current_humi)
{
    if (current_humi >= Bump_threhold[1])
		{
        return 0.0f;  // 土壤湿润，无需灌溉
    } 
		else if (current_humi < Bump_threhold[0]) 
		{
        // 干旱时按最大差值计算
        float diff = Bump_threhold[1] - Bump_threhold[0];
        return AREA * diff * WATER_DEPTH * WATER_DENSITY / 100.0f;  // 转换为升（假设diff单位为%，需调整）
    } 
		else 
		{
        // 介于两者之间，线性插值
        float diff = Bump_threhold[1] - current_humi;
        return AREA * diff * WATER_DEPTH * WATER_DENSITY / 100.0f;
    }
   
}

volatile uint32_t hp_ms_total = 0;       // 本次高压总时长
volatile uint32_t hp_ms_count = 0;      // 已运行时长
volatile uint8_t hp_running = 0;        // 是否正在高压模式
volatile uint8_t hp_is_on = 0;          // 当前水泵开/关（0=关,1=开）
volatile uint32_t hp_state_start = 0;   // 当前状态开始时刻
uint32_t HP_ON_MS = 500;                 // 高压开 0.5s
uint32_t HP_OFF_MS = 1500;              // 高压关 1.5s

void Irrigation_Control(void) 
	{	
		if(hp_running || waterflag)
			return;
		water_vol = Calculate_Water_Volume(moist);
		
		 if (water_vol > 0.0f && water_vol < MIN_WATER_VOLUME) 
		{
        water_vol = MIN_WATER_VOLUME;
    }
    if (water_vol <= 0)
		{
        BUMP_OFF;  // 无需灌溉，关闭水泵
        return;
    }
    // 计算灌溉时间（假设水泵流量为FLOW_RATE L/min）
     time_min = water_vol / FLOW_RATE;
     time_ms = (uint32_t)(time_min * 60000.0f);  // 转换为毫秒
    
		waterflag = 1;
		BUMP_ON;  // 开启水泵
	
}
	


void Start_HighPressure_Irrigation(void)
{
		
		if (hp_running || waterflag) 
		{
        return;
    }
	 water_vol = Calculate_Water_Volume(moist);
    	 if (water_vol > 0.0f && water_vol < MIN_WATER_VOLUME) 
		{
        water_vol = MIN_WATER_VOLUME;
    }
    if (water_vol <= 0)
		{
        BUMP_OFF;  // 无需灌溉，关闭水泵
        return;
    }
		
    hp_ms_total = (uint32_t)(water_vol / FLOW_RATE * 60000.0f);
    hp_running = 1;
    
    // 第一段：开泵
    BUMP_ON;
    hp_is_on = 1;
    hp_state_start = HAL_GetTick();
}

	
static uint32_t t_last = 0;

void Bump_Control(void)
{
	switch(CurrentBump_mode)
	{
		case Bump_OFF:
						BUMP_OFF;
						watertime = 0;
						waterflag = 0;
						time_ms = 0;
						hp_running = 0;
						hp_ms_count = 0;
						hp_ms_total = 0;
						hp_is_on =0;
						t_last = 0;
						break;			
    case Bump_CALIBRATION:
						watertime = 0;
						waterflag = 0;
						time_ms = 0;
						Start_HighPressure_Irrigation();
           break;
    case Bump_AUTO:	
						hp_running = 0;
						hp_ms_count = 0;
						hp_ms_total = 0;
						hp_is_on =0;
        if (HAL_GetTick() - t_last > 30000) {
            t_last = HAL_GetTick();
					Irrigation_Control();
				}				
						break;
	}
}

void Bump_Display(void)
{
	switch(CurrentBump_mode)
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

void Bump_Set(int mode )
{
	LastBump_mode = CurrentBump_mode;
	if(mode == 0)
	{
		CurrentBump_mode = Bump_OFF;
	}
	else if(mode == 1)
	{
		CurrentBump_mode = Bump_CALIBRATION;
	}
	else if(mode == 2)
	{
		CurrentBump_mode = Bump_AUTO;
	}
	Config_MarkDirty();
}


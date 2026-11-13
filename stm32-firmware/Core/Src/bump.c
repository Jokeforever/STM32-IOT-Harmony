#include "app_config.h"
#include "app_sensors.h"
/*
 * MODULE OVERVIEW
 * 文件：bump.c
 * 作用：水泵关闭、手动校准和自动灌溉控制。
 * 关键函数：
 *   BUMP_Init()        初始化阈值并默认关闭水泵
 *   Bump_Control()     根据模式执行水泵控制
 *   Bump_OnTimerTick()  处理定时器中断中的脉冲切换和到时关泵
 *   Bump_Set()         修改水泵模式
 *   Calculate_Water_Volume() 根据土壤湿度估算水量
 * 注意：
 *   首次上电默认 Bump_OFF，避免自动灌溉误启动。
 *   模式变化后会触发 Config_MarkDirty() 保存到 EEPROM。
 */
  #include "bump.h"
#include "mw_pump_port.h"
  #include "mw_config.h"

extern float water_vol ;

static volatile uint32_t watertime ;
static volatile uint8_t waterflag = 0;
static volatile uint32_t time_ms = 0;


void BUMP_Init(void)
{
    PumpPort_Set(0U);
}

float Calculate_Water_Volume(uint16_t current_humi)
{
    if (current_humi >= AppConfig_Get()->soil_high)
		{
        return 0.0f;  // 土壤湿润，无需灌溉
    } 
		else if (current_humi < AppConfig_Get()->soil_low)
		{
        // 干旱时按最大差值计算
        float diff = AppConfig_Get()->soil_high - AppConfig_Get()->soil_low;
        return AREA * diff * WATER_DEPTH * WATER_DENSITY / 100.0f;  // 转换为升（假设diff单位为%，需调整）
    } 
		else 
		{
        // 介于两者之间，线性插值
        float diff = AppConfig_Get()->soil_high - current_humi;
        return AREA * diff * WATER_DEPTH * WATER_DENSITY / 100.0f;
    }
   
}

static volatile uint32_t hp_ms_total = 0;       // 本次高压总时长
static volatile uint32_t hp_ms_count = 0;      // 已运行时长
static volatile uint8_t hp_running = 0;        // 是否正在高压模式
static volatile uint8_t hp_is_on = 0;          // 当前水泵开/关（0=关,1=开）
static volatile uint32_t hp_state_start = 0;   // 当前状态开始时刻
static const uint32_t HP_ON_MS = 500;                 // 高压开 0.5s
static const uint32_t HP_OFF_MS = 1500;              // 高压关 1.5s

void Irrigation_Control(void) 
	{
    const SensorData *sensors = SensorService_GetData();
		if(hp_running || waterflag)
			return;
		water_vol = Calculate_Water_Volume(sensors->soil_moisture_pct);
		
		 if (water_vol > 0.0f && water_vol < MIN_WATER_VOLUME) 
		{
        water_vol = MIN_WATER_VOLUME;
    }
    if (water_vol <= 0)
		{
        PumpPort_Set(0U);  // 无需灌溉，关闭水泵
        return;
    }
    // 计算灌溉时间（假设水泵流量为FLOW_RATE L/min）
     const float time_min = water_vol / FLOW_RATE;
     time_ms = (uint32_t)(time_min * 60000.0f);  // 转换为毫秒
    
		waterflag = 1;
		PumpPort_Set(1U);  // 开启水泵
	
}
	


void Start_HighPressure_Irrigation(void)
{
    const SensorData *sensors = SensorService_GetData();
		
		if (hp_running || waterflag) 
		{
        return;
    }
	 water_vol = Calculate_Water_Volume(sensors->soil_moisture_pct);
    	 if (water_vol > 0.0f && water_vol < MIN_WATER_VOLUME) 
		{
        water_vol = MIN_WATER_VOLUME;
    }
    if (water_vol <= 0)
		{
        PumpPort_Set(0U);  // 无需灌溉，关闭水泵
        return;
    }
		
    hp_ms_total = (uint32_t)(water_vol / FLOW_RATE * 60000.0f);
    hp_running = 1;
    
    // 第一段：开泵
    PumpPort_Set(1U);
    hp_is_on = 1;
    hp_state_start = PumpPort_NowMs();
}

	
static uint32_t t_last = 0;

void Bump_Control(void)
{
	switch(AppConfig_Get()->bump_mode)
	{
		case Bump_OFF:
						PumpPort_Set(0U);
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
        if (PumpPort_NowMs() - t_last > 30000) {
            t_last = PumpPort_NowMs();
					Irrigation_Control();
				}				
						break;
	}
}

void Bump_OnTimerTick(void)
{
    if (waterflag && AppConfig_Get()->bump_mode == Bump_AUTO) {
        watertime++;
        if (watertime * 10U > time_ms) {
            time_ms = 0;
            watertime = 0;
            waterflag = 0;
            PumpPort_Set(0U);
        }
    } else if (hp_running && AppConfig_Get()->bump_mode == Bump_CALIBRATION) {
        hp_ms_count++;
        const uint32_t elapsed = PumpPort_NowMs() - hp_state_start;

        if (hp_is_on) {
            if (elapsed >= HP_ON_MS) {
                PumpPort_Set(0U);
                hp_is_on = 0;
                hp_state_start = PumpPort_NowMs();
            }
        } else if (elapsed >= HP_OFF_MS && hp_ms_count < hp_ms_total) {
            PumpPort_Set(1U);
            hp_is_on = 1;
            hp_state_start = PumpPort_NowMs();
        }

        if (hp_ms_count * 10U >= hp_ms_total) {
            hp_running = 0;
            hp_ms_count = 0;
            hp_ms_total = 0;
            hp_is_on = 0;
            PumpPort_Set(0U);
            AppConfig_CompletePumpFromISR();
        }
    }
}

/* 兼容绘制入口Bump_Display的实现已集中到Display.c。 */

void Bump_Set(int mode )
{
    (void)AppConfig_Set(APP_CFG_BUMP_MODE, mode);
}


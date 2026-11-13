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
 *   Irrigation_Estimate() 估算水量和计时目标，见 mw_irrigation.c
 * 注意：
 *   首次上电默认 Bump_OFF，避免自动灌溉误启动。
 *   模式变化后会触发 Config_MarkDirty() 保存到 EEPROM。
 */
  #include "bump.h"
#include "mw_pump_port.h"
#include "mw_irrigation.h"
  #include "mw_config.h"

extern float water_vol ;

static volatile uint32_t watertime ;
static volatile uint8_t waterflag = 0;
static volatile uint32_t time_ms = 0;


void BUMP_Init(void)
{
    PumpPort_Set(0U);
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
    if (hp_running || waterflag) {
        return;
    }

    const RuntimeConfig *config = AppConfig_Get();
    const IrrigationEstimate estimate = Irrigation_Estimate(
        sensors->soil_moisture_pct, config->soil_low, config->soil_high);
    water_vol = estimate.volume_l;
    if (water_vol <= 0.0f) {
        PumpPort_Set(0U);
        return;
    }

    time_ms = estimate.duration_ms;
    waterflag = 1;
    PumpPort_Set(1U);
}

void Start_HighPressure_Irrigation(void)
{
    const SensorData *sensors = SensorService_GetData();
    if (hp_running || waterflag) {
        return;
    }

    const RuntimeConfig *config = AppConfig_Get();
    const IrrigationEstimate estimate = Irrigation_Estimate(
        sensors->soil_moisture_pct, config->soil_low, config->soil_high);
    water_vol = estimate.volume_l;
    if (water_vol <= 0.0f) {
        PumpPort_Set(0U);
        return;
    }

    hp_ms_total = estimate.duration_ms;
    hp_running = 1;
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


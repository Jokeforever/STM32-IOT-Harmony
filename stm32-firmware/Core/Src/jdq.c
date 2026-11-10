#include "app_config.h"
#include "app_sensors.h"
/* 温度和 CO2 请求在这里合并，报警模块不再直接写风扇 GPIO。 */
#include "jdq.h"
#include "mw_climate.h"

static ClimateState climate_state;


void JDQ_Init(void)
{
    Climate_Reset(&climate_state);
    JDQ_OFF;
}

void Monitor_Temp(void)
{
    const RuntimeConfig *config = AppConfig_Get();
    ClimateLimits limits;
    ClimateDemand demand;
    limits.temp_high_c = config->temp_threshold;
    limits.temp_low_c = config->temp_low_threshold;
    limits.co2_threshold = config->co2_threshold;
    (void)Climate_Evaluate(SensorService_GetData(), &limits, &climate_state, &demand);
    /* 两个请求独立：低温和CO2超限时允许同时通风/加热。 */
    if (demand.fan_on) { FAN_ON; } else { FAN_OFF; }
    if (demand.heater_on) { JDQ_ON; } else { JDQ_OFF; }
}

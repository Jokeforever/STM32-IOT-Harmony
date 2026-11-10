#include "mw_alarm.h"

uint8_t Alarm_Evaluate(const SensorData *data, const AlarmThresholds *thresholds,
                       Abnormal out[ALARM_COUNT])
{
    if ((data == 0) || (thresholds == 0) || (out == 0))
    {
        return 0U;
    }

    if (data->light_lux > thresholds->light_high)
    {
        out[ALARM_LIGHT_INDEX] = BRIGHT;
    }
    else if (data->light_lux < thresholds->light_low)
    {
        out[ALARM_LIGHT_INDEX] = DARK;
    }
    else
    {
        out[ALARM_LIGHT_INDEX] = NONE;
    }

    if (data->soil_moisture_pct > thresholds->soil_high)
    {
        out[ALARM_SOIL_INDEX] = DAMP;
    }
    else if (data->soil_moisture_pct < thresholds->soil_low)
    {
        out[ALARM_SOIL_INDEX] = DROUGHT;
    }
    else
    {
        out[ALARM_SOIL_INDEX] = NONE;
    }

    if (((data->valid_mask & SENSOR_VALID_CO2) != 0U) &&
        (data->co2_ppm > thresholds->co2_threshold))
    {
        out[ALARM_CO2_INDEX] = HIGHCO2;
    }
    else
    {
        out[ALARM_CO2_INDEX] = NONE;
    }

    return 1U;
}

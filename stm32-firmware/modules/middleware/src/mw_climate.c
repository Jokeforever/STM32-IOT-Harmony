#include "mw_climate.h"
#include <stddef.h>

void Climate_Reset(ClimateState *state)
{
    if (state != NULL) {
        state->cooling = 0U; state->heating = 0U; state->configured = 0U;
        state->last_high_c = 0U; state->last_low_c = 0U;
    }
}

uint8_t Climate_Evaluate(const SensorData *data, const ClimateLimits *limits,
                         ClimateState *state, ClimateDemand *out)
{
    if (out != NULL) { out->fan_on = 0U; out->heater_on = 0U; }
    if (data == NULL || limits == NULL || state == NULL || out == NULL ||
        limits->temp_high_c > 100U || limits->temp_low_c >= limits->temp_high_c) {
        Climate_Reset(state);
        return 0U;
    }
    if (!state->configured || state->last_high_c != limits->temp_high_c ||
        state->last_low_c != limits->temp_low_c) {
        Climate_Reset(state);
        state->configured = 1U;
        state->last_high_c = limits->temp_high_c;
        state->last_low_c = limits->temp_low_c;
    }
    if ((data->valid_mask & SENSOR_VALID_TEMP) == 0U) {
        state->cooling = 0U; state->heating = 0U;
    } else {
        if (data->temperature_c > limits->temp_high_c) state->cooling = 1U;
        else if ((int16_t)data->temperature_c <= (int16_t)limits->temp_high_c - 1) state->cooling = 0U;
        if (data->temperature_c < limits->temp_low_c) state->heating = 1U;
        else if ((int16_t)data->temperature_c >= (int16_t)limits->temp_low_c + 1) state->heating = 0U;
    }
    out->fan_on = (uint8_t)(state->cooling ||
        ((data->valid_mask & SENSOR_VALID_CO2) != 0U && data->co2_ppm > limits->co2_threshold));
    out->heater_on = state->heating;
    return 1U;
}

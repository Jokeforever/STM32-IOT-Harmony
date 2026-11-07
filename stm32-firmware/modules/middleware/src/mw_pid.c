/* 从LED_Update提取的位置式PID；保留当前公式、分支次序及历史更新规则。 */
#include "mw_pid.h"
#include <stddef.h>

void PID_Reset(PID_State *state, uint32_t now_ms)
{
    if (state == NULL) return;
    state->integral = 0.0f;
    state->last_error = 0.0f;
    state->last_tick = now_ms;
    state->initialized = 0U;
}

PID_Result PID_Update(PID_State *state, const PID_Config *config,
                      int16_t measured, uint32_t now_ms, uint16_t *output)
{
    float error;
    float dt;
    float kp, ki, kd;
    float p_term, i_term, d_term;
    float value;
    float integral_limit;
    uint32_t elapsed_ms;

    if (state == NULL || config == NULL || output == NULL) return PID_INVALID;
    if (config->kp_x1000 < 0 || config->kp_x1000 > 10000 ||
        config->ki_x1000 < 0 || config->ki_x1000 > 10000 ||
        config->kd_x1000 < 0 || config->kd_x1000 > 10000 ||
        config->output_max == 0U || config->nominal_period_ms == 0U ||
        config->max_gap_ms < config->nominal_period_ms) return PID_INVALID;

    error = (float)config->target - (float)measured;
    if ((error < 0.0f ? -error : error) <= (float)config->dead_zone)
    {
        state->integral = 0.0f;
        state->last_error = error;
        state->last_tick = now_ms;
        return PID_HOLD;
    }

    if (state->initialized == 0U)
    {
        state->last_tick = now_ms;
        state->last_error = error;
        state->integral = 0.0f;
        state->initialized = 1U;
        return PID_HOLD;
    }

    elapsed_ms = now_ms - state->last_tick;
    if (elapsed_ms == 0U) return PID_HOLD;
    if (elapsed_ms > config->max_gap_ms)
    {
        state->integral = 0.0f;
        state->last_error = error;
        elapsed_ms = config->nominal_period_ms;
    }
    dt = (float)elapsed_ms / 1000.0f;
    kp = (float)config->kp_x1000 / 1000.0f;
    ki = (float)config->ki_x1000 / 1000.0f;
    kd = (float)config->kd_x1000 / 1000.0f;

    state->integral += error * dt;
    if (ki > 0.0f)
    {
        integral_limit = (float)config->output_max / ki;
        if (state->integral > integral_limit) state->integral = integral_limit;
        if (state->integral < -integral_limit) state->integral = -integral_limit;
    }
    else state->integral = 0.0f;

    p_term = kp * error;
    i_term = ki * state->integral;
    d_term = kd * (error - state->last_error) / dt;
    value = p_term + i_term + d_term;
    if (value > (float)config->output_max) value = (float)config->output_max;
    if (value < 0.0f) value = 0.0f;
    *output = (uint16_t)(int32_t)value;
    state->last_error = error;
    state->last_tick = now_ms;
    return PID_OUTPUT_READY;
}

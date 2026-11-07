/* 冻结的2026-11-07拆分前实现，仅作差分对照；不加入目标固件构建。 */
void LED_PID_Reset(void)
{
    led_ctrl.integral = 0.0f;
    led_ctrl.last_error = 0.0f;
    led_ctrl.last_tick = HAL_GetTick();
    led_ctrl.pid_initialized = 0U;
}

void LED_Update(int16_t current_lux)
{
    float error;
    float dt;
    float p_term;
    float i_term;
    float d_term;
    float output;
    float integral_limit;
    int32_t new_pwm;
    uint32_t now;
    uint32_t elapsed_ms;

    if (!led_ctrl.enabled)
    {
        return;
    }

    led_ctrl.current_lux = current_lux;
    error = (float)AppConfig_Get()->pid_target_lux - (float)current_lux;

    if (fabsf(error) <= (float)AppConfig_Get()->pid_dead_zone)
    {
        led_ctrl.integral = 0.0f;
        led_ctrl.last_error = error;
        led_ctrl.last_tick = HAL_GetTick();
        return;
    }

    now = HAL_GetTick();
    if (led_ctrl.pid_initialized == 0U)
    {
        led_ctrl.last_tick = now;
        led_ctrl.last_error = error;
        led_ctrl.integral = 0.0f;
        led_ctrl.pid_initialized = 1U;
        return;
    }

    elapsed_ms = now - led_ctrl.last_tick;
    if (elapsed_ms == 0U)
    {
        return;
    }
    if (elapsed_ms > LED_PID_MAX_GAP_MS)
    {
        /* 长时间网络等待后重建历史，避免积分突增和微分冲击。 */
        led_ctrl.integral = 0.0f;
        led_ctrl.last_error = error;
        elapsed_ms = LED_CONTROL_INTERVAL_MS;
    }
    dt = (float)elapsed_ms / 1000.0f;

    led_ctrl.integral += error * dt;

    if (((float)AppConfig_Get()->pid_ki_x1000 / 1000.0f) > 0.0f)
    {
        integral_limit = (float)PWM_MAX / ((float)AppConfig_Get()->pid_ki_x1000 / 1000.0f);
        if (led_ctrl.integral > integral_limit)
        {
            led_ctrl.integral = integral_limit;
        }
        if (led_ctrl.integral < -integral_limit)
        {
            led_ctrl.integral = -integral_limit;
        }
    }
    else
    {
        led_ctrl.integral = 0.0f;
    }

    p_term = ((float)AppConfig_Get()->pid_kp_x1000 / 1000.0f) * error;
    i_term = ((float)AppConfig_Get()->pid_ki_x1000 / 1000.0f) * led_ctrl.integral;
    d_term = ((float)AppConfig_Get()->pid_kd_x1000 / 1000.0f) * (error - led_ctrl.last_error) / dt;
    output = p_term + i_term + d_term;

    if (output > (float)PWM_MAX)
    {
        output = (float)PWM_MAX;
    }
    if (output < 0.0f)
    {
        output = 0.0f;
    }

    new_pwm = (int32_t)output;

    __HAL_TIM_SET_COMPARE(&PWM_TIMER, PWM_CHANNEL, (uint16_t)new_pwm);
    led_ctrl.pwm_value = (uint16_t)new_pwm;
    led_ctrl.last_error = error;
    led_ctrl.last_tick = now;
}

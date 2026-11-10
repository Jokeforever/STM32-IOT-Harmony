/* 统一运行参数、范围检查和保存通知；EEPROM布局由mw_config继续负责。 */
#include "app_config.h"
#include "mw_config.h"
#include "LED.h"
#include "main.h" /* PRIMASK：仅保护主循环提交水泵模式的几个指令。 */
#include <stddef.h>

#define RUNTIME_DEFAULTS {2U,0U,50U,30U,2000U,100U,1000U,30U,75U,200,20U,1500,0,0,20U}
static const RuntimeConfig s_defaults = RUNTIME_DEFAULTS;
static RuntimeConfig s_config = RUNTIME_DEFAULTS;
static uint8_t s_previous_led = 2U;
static uint8_t s_previous_bump = 0U;

static uint8_t ReadValue(const RuntimeConfig *c, AppConfigKey key, int32_t *value)
{
    switch (key)
    {
        case APP_CFG_LED_MODE: *value = c->led_mode; return 1U;
        case APP_CFG_BUMP_MODE: *value = c->bump_mode; return 1U;
        case APP_CFG_MANUAL_PWM: *value = c->led_manual_pwm; return 1U;
        case APP_CFG_TEMP_THRESHOLD: *value = c->temp_threshold; return 1U;
        case APP_CFG_TEMP_LOW_THRESHOLD: *value = c->temp_low_threshold; return 1U;
        case APP_CFG_CO2_THRESHOLD: *value = c->co2_threshold; return 1U;
        case APP_CFG_LIGHT_LOW: *value = c->ldr_low; return 1U;
        case APP_CFG_LIGHT_HIGH: *value = c->ldr_high; return 1U;
        case APP_CFG_SOIL_LOW: *value = c->soil_low; return 1U;
        case APP_CFG_SOIL_HIGH: *value = c->soil_high; return 1U;
        case APP_CFG_PID_TARGET: *value = c->pid_target_lux; return 1U;
        case APP_CFG_PID_DEAD_ZONE: *value = c->pid_dead_zone; return 1U;
        case APP_CFG_PID_KP: *value = c->pid_kp_x1000; return 1U;
        case APP_CFG_PID_KI: *value = c->pid_ki_x1000; return 1U;
        case APP_CFG_PID_KD: *value = c->pid_kd_x1000; return 1U;
        default: return 0U;
    }
}

static uint8_t Bounds(const RuntimeConfig *c, AppConfigKey key, int32_t *low, int32_t *high)
{
    (void)c;
    switch (key)
    {
        case APP_CFG_LED_MODE: *low = 0; *high = 2; return 1U;
        case APP_CFG_BUMP_MODE: *low = 0; *high = 2; return 1U;
        case APP_CFG_MANUAL_PWM: *low = 0; *high = 100; return 1U;
        case APP_CFG_TEMP_THRESHOLD: *low = (int32_t)c->temp_low_threshold + 1; *high = 100; return 1U;
        case APP_CFG_TEMP_LOW_THRESHOLD: *low = 0; *high = (int32_t)c->temp_threshold - 1; return 1U;
        case APP_CFG_CO2_THRESHOLD: *low = 350; *high = 4000; return 1U;
        case APP_CFG_LIGHT_LOW: *low = 0; *high = 9999; return 1U;
        case APP_CFG_LIGHT_HIGH: *low = 0; *high = 9999; return 1U;
        case APP_CFG_SOIL_LOW: *low = 0; *high = 100; return 1U;
        case APP_CFG_SOIL_HIGH: *low = 0; *high = 100; return 1U;
        case APP_CFG_PID_TARGET: *low = 0; *high = 999; return 1U;
        case APP_CFG_PID_DEAD_ZONE: *low = 0; *high = 200; return 1U;
        case APP_CFG_PID_KP: *low = 0; *high = 10000; return 1U;
        case APP_CFG_PID_KI: *low = 0; *high = 10000; return 1U;
        case APP_CFG_PID_KD: *low = 0; *high = 10000; return 1U;
        default: return 0U;
    }
}

void AppConfig_GetDefaults(RuntimeConfig *out) { if (out != NULL) *out = s_defaults; }
const RuntimeConfig *AppConfig_Get(void) { return &s_config; }
void AppConfig_Init(void)
{
    s_config = s_defaults;
    s_previous_led = s_config.led_mode;
    s_previous_bump = s_config.bump_mode;
}

uint8_t AppConfig_Restore(const RuntimeConfig *config)
{
    int32_t value, low, high;
    int key;
    if (config == NULL) return 0U;
    for (key = 0; key < APP_CFG_COUNT; key++)
    {
        if (!ReadValue(config, (AppConfigKey)key, &value) ||
            !Bounds(config, (AppConfigKey)key, &low, &high) || value < low || value > high) return 0U;
    }
    if (config->ldr_low > config->ldr_high || config->soil_low > config->soil_high) return 0U;
    s_config = *config;
    s_previous_led = s_config.led_mode;
    s_previous_bump = s_config.bump_mode;
    LED_PID_Reset();
    return 1U;
}

uint8_t AppConfig_GetValue(AppConfigKey key, int32_t *value)
{
    return value != NULL ? ReadValue(&s_config, key, value) : 0U;
}

AppConfigResult AppConfig_Set(AppConfigKey key, int32_t value)
{
    int32_t old_value, low, high;
    if (!ReadValue(&s_config, key, &old_value) || !Bounds(&s_config, key, &low, &high) || value < low || value > high)
        return APP_CONFIG_REJECTED;
    if (key == APP_CFG_LIGHT_LOW && value > s_config.ldr_high) value = s_config.ldr_high;
    if (key == APP_CFG_LIGHT_HIGH && value < s_config.ldr_low) value = s_config.ldr_low;
    if (key == APP_CFG_SOIL_LOW && value > s_config.soil_high) value = s_config.soil_high;
    if (key == APP_CFG_SOIL_HIGH && value < s_config.soil_low) value = s_config.soil_low;
    if (key == APP_CFG_BUMP_MODE)
    {
        /* 同步“读取当前模式/比较/提交”，防止脉冲完成中断落在比较和赋值之间。
         * 临界区不调用HAL计时、EEPROM或Config_MarkDirty，并恢复原中断屏蔽状态。
         */
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        old_value = s_config.bump_mode;
        s_previous_bump = (uint8_t)old_value;
        s_config.bump_mode = (uint8_t)value;
        __set_PRIMASK(primask);
        if (old_value == value) return APP_CONFIG_UNCHANGED;
        Config_MarkDirty();
        return APP_CONFIG_CHANGED;
    }
    /* 接受相同模式也保持原有“上次模式”语义，但不重复标脏。 */
    if (key == APP_CFG_LED_MODE) s_previous_led = (uint8_t)old_value;
    if (old_value == value) return APP_CONFIG_UNCHANGED;
    switch (key)
    {
        case APP_CFG_LED_MODE: s_config.led_mode = (uint8_t)value; break;
        case APP_CFG_MANUAL_PWM: s_config.led_manual_pwm = (uint8_t)value; break;
        case APP_CFG_TEMP_THRESHOLD: s_config.temp_threshold = (uint8_t)value; break;
        case APP_CFG_TEMP_LOW_THRESHOLD: s_config.temp_low_threshold = (uint8_t)value; break;
        case APP_CFG_CO2_THRESHOLD: s_config.co2_threshold = (uint16_t)value; break;
        case APP_CFG_LIGHT_LOW: s_config.ldr_low = (uint16_t)value; break;
        case APP_CFG_LIGHT_HIGH: s_config.ldr_high = (uint16_t)value; break;
        case APP_CFG_SOIL_LOW: s_config.soil_low = (uint16_t)value; break;
        case APP_CFG_SOIL_HIGH: s_config.soil_high = (uint16_t)value; break;
        case APP_CFG_PID_TARGET: s_config.pid_target_lux = (int16_t)value; break;
        case APP_CFG_PID_DEAD_ZONE: s_config.pid_dead_zone = (uint16_t)value; break;
        case APP_CFG_PID_KP: s_config.pid_kp_x1000 = (int32_t)value; break;
        case APP_CFG_PID_KI: s_config.pid_ki_x1000 = (int32_t)value; break;
        case APP_CFG_PID_KD: s_config.pid_kd_x1000 = (int32_t)value; break;
        default: return APP_CONFIG_REJECTED;
    }
    if (key == APP_CFG_LED_MODE || (key >= APP_CFG_PID_TARGET && key <= APP_CFG_PID_KD)) LED_PID_Reset();
    Config_MarkDirty();
    return APP_CONFIG_CHANGED;
}

AppConfigResult AppConfig_Adjust(AppConfigKey key, int32_t delta)
{
    int32_t value, low, high;
    if (!AppConfig_GetValue(key, &value) || !Bounds(&s_config, key, &low, &high)) return APP_CONFIG_REJECTED;
    /* 先比较剩余空间，再相加，INT32_MIN/MAX增量也不会溢出。 */
    if (delta > high - value) value = high;
    else if (delta < low - value) value = low;
    else value += delta;
    return AppConfig_Set(key, value);
}

uint8_t AppConfig_PreviousLedMode(void) { return s_previous_led; }
uint8_t AppConfig_PreviousBumpMode(void) { return s_previous_bump; }
void AppConfig_CompletePumpFromISR(void)
{
    if (s_config.bump_mode == 1U) s_config.bump_mode = 0U;
}

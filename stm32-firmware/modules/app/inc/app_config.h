/* 运行配置契约：单位与范围见字段；独立于EEPROM二进制布局。 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t led_mode;           /* 0关闭、1手动常亮、2自动 */
    volatile uint8_t bump_mode; /* 0关闭、1脉冲、2自动；仅该字段可被完成中断更新 */
    uint8_t led_manual_pwm;     /* 0..100 % */
    uint8_t temp_threshold;     /* 0..100 摄氏度 */
    uint16_t co2_threshold;     /* 350..4000 ppm */
    uint16_t ldr_low;           /* 0..9999 lux，不能超过ldr_high */
    uint16_t ldr_high;          /* 0..9999 lux，不能低于ldr_low */
    uint16_t soil_low;          /* 0..100 %，不能超过soil_high */
    uint16_t soil_high;         /* 0..100 %，不能低于soil_low */
    int16_t pid_target_lux;     /* 0..999 lux */
    uint16_t pid_dead_zone;     /* 0..200 lux */
    int32_t pid_kp_x1000;       /* 0..10000：系数乘1000保存 */
    int32_t pid_ki_x1000;
    int32_t pid_kd_x1000;
} RuntimeConfig;

typedef enum
{
    APP_CFG_LED_MODE,
    APP_CFG_BUMP_MODE,
    APP_CFG_MANUAL_PWM,
    APP_CFG_TEMP_THRESHOLD,
    APP_CFG_CO2_THRESHOLD,
    APP_CFG_LIGHT_LOW,
    APP_CFG_LIGHT_HIGH,
    APP_CFG_SOIL_LOW,
    APP_CFG_SOIL_HIGH,
    APP_CFG_PID_TARGET,
    APP_CFG_PID_DEAD_ZONE,
    APP_CFG_PID_KP,
    APP_CFG_PID_KI,
    APP_CFG_PID_KD,
    APP_CFG_COUNT
} AppConfigKey;

typedef enum
{
    APP_CONFIG_REJECTED = 0,
    APP_CONFIG_UNCHANGED = 1,
    APP_CONFIG_CHANGED = 2
} AppConfigResult;

void AppConfig_Init(void); /* 启动阶段，须在启动定时器中断前调用 */
const RuntimeConfig *AppConfig_Get(void); /* 只读实时视图，非历史快照 */
void AppConfig_GetDefaults(RuntimeConfig *out);
uint8_t AppConfig_Restore(const RuntimeConfig *config); /* 启动阶段加载，范围非法则拒绝，不标脏 */
uint8_t AppConfig_GetValue(AppConfigKey key, int32_t *value);
AppConfigResult AppConfig_Set(AppConfigKey key, int32_t value);
AppConfigResult AppConfig_Adjust(AppConfigKey key, int32_t delta); /* 按键饱和增减，无溢出 */
uint8_t AppConfig_PreviousLedMode(void);
uint8_t AppConfig_PreviousBumpMode(void);
void AppConfig_CompletePumpFromISR(void); /* 唯一中断写入口：仅把脉冲模式变为关闭，不写EEPROM */

/* Set/Adjust只在主循环调用。标量越界拒绝；光照/土壤上下限在合法总范围内钳制至另一端。
 * 只有实际变化才标记延时保存；PID/LED模式变化清历史。GPIO模式生效仍由现有LED/泵函数负责。
 */
#ifdef __cplusplus
}
#endif
#endif

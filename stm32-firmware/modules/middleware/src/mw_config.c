/*
 * MODULE OVERVIEW
 * 文件：mw_config.c
 * 作用：设备参数掉电保存实现（从 main.c 抽离）
 * 层次：middleware 层
 *
 * 抽离说明：
 *   本文件原为 main.c 第 441-821 行的配置管理代码，共 12 个函数。
 *   逻辑与原来完全一致，未做任何行为改动。
 *
 * 依赖：
 *   - EEPROM24C02_Read / EEPROM24C02_Write   （drivers 层）
 *   - 各模块的全局参数变量                    （app 层）
 *   - HAL_GetTick()                          （bsp 层）
 */

#include "mw_config.h"
#include "i2c.h"          /* EEPROM24C02_Read / Write */
#include "LED.h"          /* System_Mode, led_ctrl, LDR_threhold, LED_PID_Reset */
#include "bump.h"         /* Bump_Mode, Bump_threhold, HUMI_THRESHOLD_* */
#include "jdq.h"          /* TEMP_THREHOLD */
#include "usart3.h"       /* CO2_THREHOLD */

#include <string.h>
#include <stddef.h>

/* ============================================================
 * 旧版本配置结构（v1），仅用于迁移识别
 * ============================================================ */
typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint16_t sequence;
    uint8_t  led_mode;
    uint8_t  bump_mode;
    uint8_t  led_manual_pwm;
    uint8_t  temp_threshold;
    uint16_t co2_threshold;
    uint16_t ldr_low;
    uint16_t ldr_high;
    uint16_t soil_low;
    uint16_t soil_high;
    uint16_t soil_wet_adc;
    uint16_t soil_dry_adc;
    uint32_t crc32;
} DeviceConfigV1;

/* 双槽读取缓冲，两个结构体共用同一块 64 字节空间 */
typedef union
{
    uint8_t       raw[64];
    DeviceConfig  v2;
    DeviceConfigV1 v1;
} ConfigSlot;

/* ============================================================
 * 模块内部状态
 * ============================================================ */
static DeviceConfig g_device_config;
static uint16_t     g_config_sequence = 0U;
static uint8_t      g_config_active_slot = 0U;
static volatile uint8_t  g_config_dirty = 0U;
static uint32_t     g_config_dirty_tick = 0U;

/* ============================================================
 * 外部参数变量（由各模块定义）
 * ============================================================ */
extern System_Mode current_mode;
extern System_Mode last_mode;
extern Bump_Mode   CurrentBump_mode;
extern Bump_Mode   LastBump_mode;
extern uint8_t     temp_threhold;
extern uint16_t    Bump_threhold[2];
extern uint16_t    LDR_threhold[2];
extern uint16_t    CO2_Threhold;
extern uint8_t     LED_Manu_PWM;

/* ============================================================
 * CRC32（标准多项式 0xEDB88320）
 * ============================================================ */
static uint32_t Config_CRC32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;

    for (uint32_t i = 0U; i < len; i++)
    {
        crc ^= (uint32_t)data[i];
        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1) ^ 0xEDB88320U;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFFU;
}

/* ============================================================
 * 有效性校验
 * ============================================================ */
static uint8_t Config_IsValid(const DeviceConfig *config)
{
    if (config == NULL)
    {
        return 0U;
    }
    if (config->magic != CONFIG_MAGIC)
    {
        return 0U;
    }
    if (config->version != CONFIG_VERSION)
    {
        return 0U;
    }
    if (config->size != (uint16_t)sizeof(DeviceConfig))
    {
        return 0U;
    }

    return (Config_CRC32((const uint8_t *)config,
                         (uint32_t)(sizeof(DeviceConfig) - sizeof(uint32_t))) == config->crc32) ? 1U : 0U;
}

static uint8_t Config_IsValidV1(const DeviceConfigV1 *config)
{
    if (config == NULL)
    {
        return 0U;
    }
    if (config->magic != CONFIG_MAGIC)
    {
        return 0U;
    }
    if (config->version != CONFIG_VERSION_V1)
    {
        return 0U;
    }
    if (config->size != (uint16_t)sizeof(DeviceConfigV1))
    {
        return 0U;
    }

    return (Config_CRC32((const uint8_t *)config,
                         (uint32_t)(sizeof(DeviceConfigV1) - sizeof(uint32_t))) == config->crc32) ? 1U : 0U;
}

/* ============================================================
 * v1 -> v2 迁移
 * ============================================================ */
static void Config_UpgradeV1(DeviceConfig *out, const DeviceConfigV1 *in)
{
    memset(out, 0, sizeof(*out));
    out->magic = CONFIG_MAGIC;
    out->version = CONFIG_VERSION;
    out->size = (uint16_t)sizeof(DeviceConfig);
    out->sequence = in->sequence;
    out->led_mode = in->led_mode;
    out->bump_mode = in->bump_mode;
    out->led_manual_pwm = in->led_manual_pwm;
    out->temp_threshold = in->temp_threshold;
    out->co2_threshold = in->co2_threshold;
    out->ldr_low = in->ldr_low;
    out->ldr_high = in->ldr_high;
    out->soil_low = in->soil_low;
    out->soil_high = in->soil_high;
    out->soil_wet_adc = in->soil_wet_adc;
    out->soil_dry_adc = in->soil_dry_adc;
    out->pid_target_lux = 500;
    out->pid_dead_zone = 20U;
    out->pid_kp_x1000 = 1500;
    out->pid_ki_x1000 = 0;
    out->pid_kd_x1000 = 0;
}

/* ============================================================
 * 槽读取
 * ============================================================ */
static uint8_t Config_ReadSlot(uint8_t slot_addr, DeviceConfig *out, uint8_t *is_v1)
{
    ConfigSlot slot;

    if (EEPROM24C02_Read(slot_addr, slot.raw, (uint16_t)sizeof(slot.raw)) == 0U)
    {
        return 0U;
    }

    if (Config_IsValid(&slot.v2) != 0U)
    {
        *out = slot.v2;
        *is_v1 = 0U;
        return 1U;
    }

    if (Config_IsValidV1(&slot.v1) != 0U)
    {
        Config_UpgradeV1(out, &slot.v1);
        *is_v1 = 1U;
        return 1U;
    }

    return 0U;
}

/* ============================================================
 * 默认配置
 * ============================================================ */
static void Config_SetDefault(void)
{
    memset(&g_device_config, 0, sizeof(g_device_config));
    g_device_config.magic = CONFIG_MAGIC;
    g_device_config.version = CONFIG_VERSION;
    g_device_config.size = (uint16_t)sizeof(DeviceConfig);
    g_device_config.sequence = 0U;
    g_device_config.led_mode = (uint8_t)MODE_AUTO_PID;
    g_device_config.bump_mode = (uint8_t)Bump_OFF;
    g_device_config.led_manual_pwm = 50U;
    g_device_config.temp_threshold = (uint8_t)TEMP_THREHOLD;
    g_device_config.co2_threshold = CO2_THREHOLD;
    g_device_config.ldr_low = LDR_THREHOLD_LOW;
    g_device_config.ldr_high = LDR_THREHOLD_HIGH;
    g_device_config.soil_low = HUMI_THRESHOLD_LOW;
    g_device_config.soil_high = HUMI_THRESHOLD_HIGH;
    g_device_config.soil_wet_adc = 1241U;
    g_device_config.soil_dry_adc = 4095U;
    g_device_config.pid_target_lux = 500;
    g_device_config.pid_dead_zone = 20U;
    g_device_config.pid_kp_x1000 = 1500;
    g_device_config.pid_ki_x1000 = 0;
    g_device_config.pid_kd_x1000 = 0;
    g_device_config.crc32 = Config_CRC32((const uint8_t *)&g_device_config,
                                         (uint32_t)(sizeof(DeviceConfig) - sizeof(uint32_t)));
}

/* ============================================================
 * 范围修正，防止 EEPROM 里的脏数据导致异常行为
 * ============================================================ */
static void Config_Sanitize(DeviceConfig *config)
{
    if (config->led_mode > (uint8_t)MODE_AUTO_PID)
    {
        config->led_mode = (uint8_t)MODE_AUTO_PID;
    }
    if (config->bump_mode > (uint8_t)Bump_AUTO)
    {
        config->bump_mode = (uint8_t)Bump_OFF;
    }
    if (config->led_manual_pwm > 100U)
    {
        config->led_manual_pwm = 100U;
    }
    if (config->temp_threshold > 100U)
    {
        config->temp_threshold = 100U;
    }
    if ((config->co2_threshold < 350U) || (config->co2_threshold > 4000U))
    {
        config->co2_threshold = CO2_THREHOLD;
    }
    if ((config->ldr_low > config->ldr_high) || (config->ldr_high > 9999U))
    {
        config->ldr_low = LDR_THREHOLD_LOW;
        config->ldr_high = LDR_THREHOLD_HIGH;
    }
    if ((config->soil_low > config->soil_high) || (config->soil_high > 100U))
    {
        config->soil_low = HUMI_THRESHOLD_LOW;
        config->soil_high = HUMI_THRESHOLD_HIGH;
    }
    if ((config->soil_wet_adc < 1U) || (config->soil_wet_adc >= config->soil_dry_adc))
    {
        config->soil_wet_adc = 1241U;
        config->soil_dry_adc = 4095U;
    }
    if ((config->pid_target_lux < 0) || (config->pid_target_lux > 999))
    {
        config->pid_target_lux = 500;
    }
    if (config->pid_dead_zone > 200U)
    {
        config->pid_dead_zone = 20U;
    }
    if ((config->pid_kp_x1000 < 0) || (config->pid_kp_x1000 > 10000))
    {
        config->pid_kp_x1000 = 1500;
    }
    if ((config->pid_ki_x1000 < 0) || (config->pid_ki_x1000 > 10000))
    {
        config->pid_ki_x1000 = 0;
    }
    if ((config->pid_kd_x1000 < 0) || (config->pid_kd_x1000 > 10000))
    {
        config->pid_kd_x1000 = 0;
    }
}

/* ============================================================
 * 把配置应用到各模块的全局参数
 * ============================================================ */
static void Config_Apply(void)
{
    current_mode = (System_Mode)g_device_config.led_mode;
    last_mode = current_mode;
    CurrentBump_mode = (Bump_Mode)g_device_config.bump_mode;
    LastBump_mode = CurrentBump_mode;
    LED_Manu_PWM = g_device_config.led_manual_pwm;
    temp_threhold = g_device_config.temp_threshold;
    CO2_Threhold = g_device_config.co2_threshold;
    LDR_threhold[0] = g_device_config.ldr_low;
    LDR_threhold[1] = g_device_config.ldr_high;
    Bump_threhold[0] = g_device_config.soil_low;
    Bump_threhold[1] = g_device_config.soil_high;

    led_ctrl.target_lux = g_device_config.pid_target_lux;
    led_ctrl.dead_zone = g_device_config.pid_dead_zone;
    led_ctrl.k_p = (float)g_device_config.pid_kp_x1000 / 1000.0f;
    led_ctrl.k_i = (float)g_device_config.pid_ki_x1000 / 1000.0f;
    led_ctrl.k_d = (float)g_device_config.pid_kd_x1000 / 1000.0f;
    LED_PID_Reset();
}

/* ============================================================
 * 从当前全局参数构造配置结构
 * ============================================================ */
static void Config_FromCurrent(DeviceConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->magic = CONFIG_MAGIC;
    config->version = CONFIG_VERSION;
    config->size = (uint16_t)sizeof(DeviceConfig);
    config->sequence = g_config_sequence + 1U;
    if (config->sequence == 0U)
    {
        config->sequence = 1U;
    }
    config->led_mode = (uint8_t)current_mode;
    config->bump_mode = (uint8_t)CurrentBump_mode;
    config->led_manual_pwm = LED_Manu_PWM;
    config->temp_threshold = temp_threhold;
    config->co2_threshold = CO2_Threhold;
    config->ldr_low = LDR_threhold[0];
    config->ldr_high = LDR_threhold[1];
    config->soil_low = Bump_threhold[0];
    config->soil_high = Bump_threhold[1];
    config->soil_wet_adc = 1241U;
    config->soil_dry_adc = 4095U;
    config->pid_target_lux = led_ctrl.target_lux;
    config->pid_dead_zone = led_ctrl.dead_zone;
    config->pid_kp_x1000 = (int32_t)(led_ctrl.k_p * 1000.0f + 0.5f);
    config->pid_ki_x1000 = (int32_t)(led_ctrl.k_i * 1000.0f + 0.5f);
    config->pid_kd_x1000 = (int32_t)(led_ctrl.k_d * 1000.0f + 0.5f);
    config->crc32 = Config_CRC32((const uint8_t *)config,
                                  (uint32_t)(sizeof(DeviceConfig) - sizeof(uint32_t)));
}

/* ============================================================
 * 对外接口实现
 * ============================================================ */
void Config_Load(void)
{
    DeviceConfig slot_a;
    DeviceConfig slot_b;
    uint8_t a_ok;
    uint8_t b_ok;
    uint8_t a_v1 = 0U;
    uint8_t b_v1 = 0U;
    uint8_t chosen_v1 = 0U;

    a_ok = Config_ReadSlot(CONFIG_SLOT_A, &slot_a, &a_v1);
    b_ok = Config_ReadSlot(CONFIG_SLOT_B, &slot_b, &b_v1);

    if ((a_ok != 0U) && (b_ok != 0U))
    {
        if (slot_b.sequence > slot_a.sequence)
        {
            g_device_config = slot_b;
            g_config_active_slot = 1U;
            chosen_v1 = b_v1;
        }
        else
        {
            g_device_config = slot_a;
            g_config_active_slot = 0U;
            chosen_v1 = a_v1;
        }
    }
    else if (a_ok != 0U)
    {
        g_device_config = slot_a;
        g_config_active_slot = 0U;
        chosen_v1 = a_v1;
    }
    else if (b_ok != 0U)
    {
        g_device_config = slot_b;
        g_config_active_slot = 1U;
        chosen_v1 = b_v1;
    }
    else
    {
        Config_SetDefault();
        g_config_active_slot = 0U;
        g_config_dirty = 1U;
        g_config_dirty_tick = HAL_GetTick();
    }

    g_config_sequence = g_device_config.sequence;
    Config_Sanitize(&g_device_config);
    Config_Apply();

    if ((a_ok == 0U) && (b_ok == 0U))
    {
        /* 默认配置已标记为待写入，无需额外处理 */
    }
    else if (chosen_v1 != 0U)
    {
        /* 从 v1 迁移过来的配置需要重新写入 v2 格式 */
        g_config_dirty = 1U;
        g_config_dirty_tick = HAL_GetTick();
    }
}

uint8_t Config_Save(void)
{
    DeviceConfig config;
    DeviceConfig verify;
    uint8_t slot = (g_config_active_slot == 0U) ? CONFIG_SLOT_B : CONFIG_SLOT_A;

    Config_FromCurrent(&config);

    if (EEPROM24C02_Write(slot, (const uint8_t *)&config, (uint16_t)sizeof(config)) == 0U)
    {
        return 0U;
    }
    if (EEPROM24C02_Read(slot, (uint8_t *)&verify, (uint16_t)sizeof(verify)) == 0U)
    {
        return 0U;
    }
    if (Config_IsValid(&verify) == 0U)
    {
        return 0U;
    }

    g_device_config = verify;
    g_config_sequence = verify.sequence;
    g_config_active_slot = (slot == CONFIG_SLOT_A) ? 0U : 1U;
    return 1U;
}

void Config_MarkDirty(void)
{
    g_config_dirty = 1U;
    g_config_dirty_tick = HAL_GetTick();
}

uint8_t Config_IsDirty(void)
{
    return g_config_dirty;
}

void Config_Process(void)
{
    if (g_config_dirty == 0U)
    {
        return;
    }
    if ((HAL_GetTick() - g_config_dirty_tick) < 1000U)
    {
        return;
    }

    if (Config_Save() != 0U)
    {
        g_config_dirty = 0U;
    }
    else
    {
        g_config_dirty_tick = HAL_GetTick();
    }
}

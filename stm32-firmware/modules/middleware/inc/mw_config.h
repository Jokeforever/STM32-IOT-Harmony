/*
 * MODULE OVERVIEW
 * 文件：mw_config.h
 * 作用：设备参数掉电保存模块（从 main.c / main.h 抽离）
 * 层次：middleware 层 —— 与硬件无关，只依赖 EEPROM 读写接口
 *
 * 设计说明：
 *   - 24C02 双槽存储，Slot A = 0x00，Slot B = 0x40
 *   - 每个槽带 magic / version / size / sequence / CRC32
 *   - 写入空闲槽后读回校验，通过才切换活动槽
 *   - 支持 v1 -> v2 配置迁移
 *   - 参数变化后延时写入，避免频繁擦写 EEPROM
 *
 * 迁移提示（换 MCU 时）：
 *   本模块只依赖 EEPROM24C02_Read/Write 两个函数。
 *   换平台只需替换驱动实现，本文件无需修改。
 */

#ifndef __MW_CONFIG_H
#define __MW_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ============================================================
 * 配置常量
 * ============================================================ */
#define CONFIG_MAGIC       0x43464731U
#define CONFIG_VERSION     2U
#define CONFIG_VERSION_V1  1U
#define CONFIG_SLOT_A      0x00U
#define CONFIG_SLOT_B      0x40U

/* ============================================================
 * 当前配置结构（v2）
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
    int16_t  pid_target_lux;
    uint16_t pid_dead_zone;
    int32_t  pid_kp_x1000;
    int32_t  pid_ki_x1000;
    int32_t  pid_kd_x1000;
    uint32_t crc32;
} DeviceConfig;

/* ============================================================
 * 对外接口
 * ============================================================ */

/**
 * @brief  从 EEPROM 加载配置，自动选择有效槽并做版本迁移
 * @note   两个槽都无效时加载默认配置并标记待写入
 */
void Config_Load(void);

/**
 * @brief  立即把当前参数写入 EEPROM
 * @retval 1 = 成功，0 = 失败
 */
uint8_t Config_Save(void);

/**
 * @brief  标记参数已变化，延时后由 Config_Process 写入
 * @note   避免连续按键导致频繁写 EEPROM
 */
void Config_MarkDirty(void);

/**
 * @brief  在主循环中周期调用，处理延时写入
 */
void Config_Process(void);

/**
 * @brief  查询是否有待写入的参数变化
 * @retval 1 = 有未保存变化，0 = 已同步
 */
uint8_t Config_IsDirty(void);

#ifdef __cplusplus
}
#endif

#endif /* __MW_CONFIG_H */

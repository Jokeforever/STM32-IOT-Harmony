/*
 * MODULE OVERVIEW
 * 文件：mw_filter.h
 * 作用：通用数字滤波算法（从 main.c 抽离）
 * 层次：middleware 层 —— 纯算法，与硬件完全无关
 *
 * 提供：
 *   Filter_Median3_U8    8 位三点中值滤波
 *   Filter_Median3_U16   16 位三点中值滤波
 *   Filter_Median3_Reset 重置滤波器历史
 *
 * 使用方式：
 *   每个传感器维护一份 history 数组和 count 计数，
 *   每次读到有效数据后调用对应函数，返回滤波后的值。
 *
 * 设计要点：
 *   - 前 3 次调用直接返回原值（历史还没填满）
 *   - 之后每次滑动窗口，取三点中值
 *   - 无效数据不应传入，否则会污染历史
 *
 * 迁移提示（换 MCU 时）：
 *   本文件是纯算法，不含任何硬件依赖，可直接复用。
 */

#ifndef __MW_FILTER_H
#define __MW_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ============================================================
 * 滤波窗口长度
 * ============================================================ */
#define FILTER_MEDIAN3_WINDOW   3U

/**
 * @brief  8 位三点中值滤波
 * @param  history  长度为 3 的历史缓冲，由调用者维护
 * @param  count    已填充数量，由调用者维护，初始为 0
 * @param  value    本次采样值
 * @return 滤波后的值
 * @note   前 3 次调用返回原值；此后返回三点中值
 */
uint8_t Filter_Median3_U8(uint8_t history[FILTER_MEDIAN3_WINDOW],
                          uint8_t *count,
                          uint8_t value);

/**
 * @brief  16 位三点中值滤波
 * @param  history  长度为 3 的历史缓冲
 * @param  count    已填充数量
 * @param  value    本次采样值
 * @return 滤波后的值
 */
uint16_t Filter_Median3_U16(uint16_t history[FILTER_MEDIAN3_WINDOW],
                            uint8_t *count,
                            uint16_t value);

/**
 * @brief  重置 8 位滤波器历史
 * @param  history  历史缓冲
 * @param  count    计数
 * @note   传感器恢复有效时调用，避免旧数据影响
 */
void Filter_Median3_ResetU8(uint8_t history[FILTER_MEDIAN3_WINDOW],
                            uint8_t *count);

/**
 * @brief  重置 16 位滤波器历史
 */
void Filter_Median3_ResetU16(uint16_t history[FILTER_MEDIAN3_WINDOW],
                             uint8_t *count);

#ifdef __cplusplus
}
#endif

#endif /* __MW_FILTER_H */

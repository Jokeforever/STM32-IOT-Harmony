/*
 * MODULE OVERVIEW
 * 文件：mw_filter.c
 * 作用：通用数字滤波算法实现（从 main.c 抽离）
 * 层次：middleware 层
 *
 * 抽离说明：
 *   本文件原为 main.c 第 137-199 行的两个静态函数，
 *   逻辑与原来完全一致，只是去掉了 static 并加了声明。
 *
 * 为什么用三点中值而不是平均值：
 *   平均值会被单次尖峰拉偏，中值能直接剔除异常值。
 *   对于 DHT11、CO2 这类偶发错误帧的传感器，中值更合适。
 */

#include "mw_filter.h"

/* ============================================================
 * 8 位三点中值滤波
 * ============================================================ */
uint8_t Filter_Median3_U8(uint8_t history[FILTER_MEDIAN3_WINDOW],
                          uint8_t *count,
                          uint8_t value)
{
    uint8_t a;
    uint8_t b;
    uint8_t c;

    /* 历史未填满，直接返回原值 */
    if (*count < FILTER_MEDIAN3_WINDOW)
    {
        history[*count] = value;
        (*count)++;
        return value;
    }

    /* 滑动窗口 */
    history[0] = history[1];
    history[1] = history[2];
    history[2] = value;

    a = history[0];
    b = history[1];
    c = history[2];

    /* 取中值 */
    if (((a >= b) && (a <= c)) || ((a <= b) && (a >= c)))
    {
        return a;
    }
    if (((b >= a) && (b <= c)) || ((b <= a) && (b >= c)))
    {
        return b;
    }
    return c;
}

/* ============================================================
 * 16 位三点中值滤波
 * ============================================================ */
uint16_t Filter_Median3_U16(uint16_t history[FILTER_MEDIAN3_WINDOW],
                            uint8_t *count,
                            uint16_t value)
{
    uint16_t a;
    uint16_t b;
    uint16_t c;

    if (*count < FILTER_MEDIAN3_WINDOW)
    {
        history[*count] = value;
        (*count)++;
        return value;
    }

    history[0] = history[1];
    history[1] = history[2];
    history[2] = value;

    a = history[0];
    b = history[1];
    c = history[2];

    if (((a >= b) && (a <= c)) || ((a <= b) && (a >= c)))
    {
        return a;
    }
    if (((b >= a) && (b <= c)) || ((b <= a) && (b >= c)))
    {
        return b;
    }
    return c;
}

/* ============================================================
 * 重置
 * ============================================================ */
void Filter_Median3_ResetU8(uint8_t history[FILTER_MEDIAN3_WINDOW],
                            uint8_t *count)
{
    uint8_t i;

    for (i = 0U; i < FILTER_MEDIAN3_WINDOW; i++)
    {
        history[i] = 0U;
    }
    *count = 0U;
}

void Filter_Median3_ResetU16(uint16_t history[FILTER_MEDIAN3_WINDOW],
                             uint8_t *count)
{
    uint8_t i;

    for (i = 0U; i < FILTER_MEDIAN3_WINDOW; i++)
    {
        history[i] = 0U;
    }
    *count = 0U;
}

#ifndef MW_IRRIGATION_H
#define MW_IRRIGATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float volume_l;
    uint32_t duration_ms;
} IrrigationEstimate;

/* 阈值由配置层保证 0 <= low_pct <= high_pct <= 100。
 * 返回估算水量及计时目标，达到湿润阈值时两者为 0。
 */
IrrigationEstimate Irrigation_Estimate(uint16_t moisture_pct,
                                      uint16_t low_pct, uint16_t high_pct);

#ifdef __cplusplus
}
#endif

#endif

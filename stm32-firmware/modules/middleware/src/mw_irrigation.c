#include "mw_irrigation.h"

#define PLANTING_AREA_M2     0.08f
#define WATER_DEPTH_MM       10.0f
#define WATER_L_PER_M2_MM     1.0f
#define PUMP_FLOW_L_PER_MIN   1.2f
#define MIN_WATER_VOLUME_L    0.1f

IrrigationEstimate Irrigation_Estimate(uint16_t moisture_pct,
                                      uint16_t low_pct, uint16_t high_pct)
{
    IrrigationEstimate estimate = {0.0f, 0U};
    if (moisture_pct >= high_pct) {
        return estimate;
    }

    const uint16_t reference_pct = moisture_pct < low_pct ? low_pct : moisture_pct;
    const float diff_pct = high_pct - reference_pct;
    estimate.volume_l = PLANTING_AREA_M2 * diff_pct * WATER_DEPTH_MM
                      * WATER_L_PER_M2_MM / 100.0f;
    if (estimate.volume_l > 0.0f && estimate.volume_l < MIN_WATER_VOLUME_L) {
        estimate.volume_l = MIN_WATER_VOLUME_L;
    }

    const float minutes = estimate.volume_l / PUMP_FLOW_L_PER_MIN;
    estimate.duration_ms = (uint32_t)(minutes * 60000.0f);
    return estimate;
}

/* 温度/CO2控制契约：不依赖HAL、GPIO、存储或云端。 */
#ifndef MW_CLIMATE_H
#define MW_CLIMATE_H
#include "sensor_data.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint8_t temp_high_c; /* 1..100，必须大于下限 */
    uint8_t temp_low_c;  /* 0..99 */
    uint16_t co2_threshold;
} ClimateLimits;
typedef struct {
    uint8_t cooling;
    uint8_t heating;
    uint8_t configured;
    uint8_t last_high_c;
    uint8_t last_low_c;
} ClimateState;
typedef struct { uint8_t fan_on; uint8_t heater_on; } ClimateDemand;
void Climate_Reset(ClimateState *state);
/* 超上限开，降至上限-1关；低于下限开，升至下限+1关。
 * 温度失效清除两项温度请求；CO2独立按有效位与严格超限请求通风。
 * 上下限变化清除温度历史，并立即按新限值重判。同值设置不重置。
 * 返回1成功；空指针/非法上下限返回0，有输出/状态指针时清零为关闭。
 * 调用者持有state，每台设备独立；不使用GPIO状态作为回差历史。
 */
uint8_t Climate_Evaluate(const SensorData *data, const ClimateLimits *limits,
                         ClimateState *state, ClimateDemand *out);
#ifdef __cplusplus
}
#endif
#endif

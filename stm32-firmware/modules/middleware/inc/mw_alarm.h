/* 报警规则契约：只接收数据与阈值，不依赖HAL、显示或配置服务。 */
#ifndef MW_ALARM_H
#define MW_ALARM_H

#include "sensor_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 保留原Display.h中的类型名及0..5编号，兼容现有状态使用者。 */
typedef enum
{
    NONE = 0,
    BRIGHT,
    DARK,
    DAMP,
    DROUGHT,
    HIGHCO2
} Abnormal;

#define ALARM_LIGHT_INDEX 0U
#define ALARM_SOIL_INDEX 1U
#define ALARM_CO2_INDEX 2U
#define ALARM_COUNT 3U

typedef struct
{
    uint16_t light_low;       /* lux */
    uint16_t light_high;      /* lux */
    uint16_t soil_low;        /* 土壤湿度，整数百分比 */
    uint16_t soil_high;       /* 土壤湿度，整数百分比 */
    uint16_t co2_threshold;   /* ppm */
} AlarmThresholds;

/* 成功返回1并写入三项状态；任一空指针返回0，输出保持原样。
 * out须有至少ALARM_COUNT个元素。无状态、无分配、无硬件输出。
 * 保留原严格>/<和先上限后下限判断；范围/上下限关系由配置入口负责。
 * 仅CO2使用有效位；光照和土壤沿用已有读数，不增加拔插判断。
 */
uint8_t Alarm_Evaluate(const SensorData *data, const AlarmThresholds *thresholds,
                       Abnormal out[ALARM_COUNT]);

#ifdef __cplusplus
}
#endif
#endif

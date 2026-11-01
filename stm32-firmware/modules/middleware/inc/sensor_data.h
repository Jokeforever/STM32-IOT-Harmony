/* 传感器数据契约：只包含标准 C 类型，不依赖 HAL、GPIO 或具体传感器。 */
#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <stdint.h>

/* 与现有 OneNET sensor_status 位定义一致。未定义的位保持为 0。 */
#define SENSOR_VALID_TEMP (1U << 0)
#define SENSOR_VALID_HUMI (1U << 1)
#define SENSOR_VALID_CO2  (1U << 3)

typedef struct
{
    uint8_t temperature_c;       /* 摄氏度；当前 DHT11 仅提供整数温度 */
    uint8_t humidity_pct;        /* 空气相对湿度，百分比 */
    uint16_t light_lux;          /* 光照，lux；沿用现有换算与限幅 */
    uint16_t soil_moisture_pct;  /* 土壤湿度，百分比 */
    uint16_t co2_ppm;            /* JW01 等效 CO2，ppm */
    uint8_t valid_mask;          /* 最近一次采集的温度/湿度/CO2有效位 */
} SensorData;

/* 失败时保留最近数值，消费者必须结合 valid_mask 判断可检测的传感器。
 * 光照/土壤不做拔插检测；估算补水量是业务计算结果，不放在本结构中。
 * 当前契约保留整数单位；未来引入小数精度时应明确修改数据契约。
 */
#endif

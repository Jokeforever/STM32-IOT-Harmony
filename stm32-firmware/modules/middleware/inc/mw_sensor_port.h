/* 传感器读取契约：标准C类型，不依赖HAL或具体传感器。 */
#ifndef MW_SENSOR_PORT_H
#define MW_SENSOR_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SENSOR_PORT_ERROR = 0,
    SENSOR_PORT_OK = 1
} SensorPortResult;

/* 全部接口统一：OK=1，ERROR=0。空指针返回ERROR，不调用底层驱动。
 * 仅成功时更新输出；失败保留调用者原值。无动态分配，无额外采集或滤波。
 * 当前适配为同步调用；调用时序/频率由主循环和采集服务负责。
 * 成功仅表示本次驱动读取成功，不证明模拟传感器已连接或完成标定。
 */
SensorPortResult SensorPort_ReadTempHumi(uint8_t *temperature_c, uint8_t *humidity_pct);
SensorPortResult SensorPort_ReadCO2(uint16_t *co2_ppm);
SensorPortResult SensorPort_ReadLight(uint16_t *light_lux);
SensorPortResult SensorPort_ReadSoil(uint16_t *soil_moisture_pct);

#ifdef __cplusplus
}
#endif
#endif

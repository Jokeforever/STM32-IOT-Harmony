/* 采集服务：主循环负责调用，显示/控制/上报只读取统一数据。 */
#ifndef APP_SENSORS_H
#define APP_SENSORS_H

#include "sensor_data.h"

#ifdef __cplusplus
extern "C" {
#endif

void SensorService_Init(void);
void SensorService_Poll(void);       /* 原5秒上报周期内采集，调用顺序不变 */
uint8_t SensorService_ReadLight(void); /* 原1秒LED周期的额外光照采集 */

/* 返回只读实时视图，不是历史快照。
 * 仅主循环上下文读写本服务；中断只更新驱动接收缓冲。
 * 后续若引入并发，需先增加快照/同步机制。
 */
const SensorData *SensorService_GetData(void);

#ifdef __cplusplus
}
#endif
#endif

#ifndef APP_VGUS_H
#define APP_VGUS_H

#include "sensor_data.h"

/* USART1 初始化后，每个采集周期发送一次环境读数与现有补水估算。 */
void Vgus_DisplayEnvironment(const SensorData *sensors, float water_volume_l);

/* 主循环执行控制逻辑后调用。 */
void Vgus_UpdateActuators(uint32_t now_ms);

#endif

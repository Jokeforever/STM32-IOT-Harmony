#ifndef APP_VGUS_H
#define APP_VGUS_H

#include "sensor_data.h"

/* USART1 初始化后，每个采集周期发送一次环境读数与现有补水估算。 */
void Vgus_DisplayEnvironment(const SensorData *sensors, float water_volume_l);

/* 主循环执行控制逻辑后调用。 */
void Vgus_UpdateActuators(uint32_t now_ms);

/* 主循环开始后接收触摸；两个回调仅用于 USART1 中断。 */
void Vgus_StartTouch(void);
void Vgus_TouchRxComplete(void);
void Vgus_TouchRxError(void);
void Vgus_ProcessTouch(void);

#endif

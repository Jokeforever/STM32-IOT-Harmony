#ifndef APP_VGUS_H
#define APP_VGUS_H

#include "sensor_data.h"

/* USART1 初始化后，在主循环中发送一次温湿度快照。 */
void Vgus_DisplayTempHumi(const SensorData *sensors);

#endif

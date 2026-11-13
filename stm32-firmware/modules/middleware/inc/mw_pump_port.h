#ifndef MW_PUMP_PORT_H
#define MW_PUMP_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GPIO 初始化后可从主循环或定时器中断调用。
 * active 为 0 时关闭，非 0 时开启。每次请求写一次输出。
 */
void PumpPort_Set(uint8_t active);

#ifdef __cplusplus
}
#endif

#endif

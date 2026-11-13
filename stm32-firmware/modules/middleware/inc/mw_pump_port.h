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

/* 主循环和定时器中断可读取的 32 位毫秒计数，溢出时回到 0。 */
uint32_t PumpPort_NowMs(void);

#ifdef __cplusplus
}
#endif

#endif

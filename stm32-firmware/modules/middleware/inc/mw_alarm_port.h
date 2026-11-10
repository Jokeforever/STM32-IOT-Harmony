/* 报警提示输出契约：标准类型，不暴露引脚或有效电平。 */
#ifndef MW_ALARM_PORT_H
#define MW_ALARM_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* active=0请求关闭，非0请求开启（通常使用0/1）。
 * 一次调用只写对应输出一次，不缓存、不等待、不配置GPIO。
 * 返回void，不代表已经检测到实体灯亮或蜂鸣器发声。
 */
void AlarmPort_SetIndicator(uint8_t active);
void AlarmPort_SetBuzzer(uint8_t active);

#ifdef __cplusplus
}
#endif
#endif

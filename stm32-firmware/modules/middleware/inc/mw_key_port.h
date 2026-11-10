/* 按键输入契约：只依赖标准C类型，bit0..3对应逻辑按键1..4。 */
#ifndef MW_KEY_PORT_H
#define MW_KEY_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KEY_PORT_KEY1_MASK (1U << 0)
#define KEY_PORT_KEY2_MASK (1U << 1)
#define KEY_PORT_KEY3_MASK (1U << 2)
#define KEY_PORT_KEY4_MASK (1U << 3)
#define KEY_PORT_ALL_MASK 0x0FU

/* 返回1表示按下，0表示松开；高4位固定为0。
 * 当前适配按键1..4依次读取四个GPIO，不是同时采样。
 * 仅转换引脚与有效电平；无消抖、计时、事件消费或错误检测。
 * 由原TIM1扫描调用；不等待、不分配内存、不修改外设配置。
 */
uint8_t KeyPort_ReadPressedMask(void);

#ifdef __cplusplus
}
#endif
#endif

/* 配置存储/时基契约：无HAL、I2C或具体存储芯片依赖。 */
#ifndef MW_CONFIG_PORT_H
#define MW_CONFIG_PORT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* offset是配置区内的字节偏移，不是I2C设备地址。当前适配为24C02的0..255。
 * 完成整个读/写才返回非0，失败返回0。禁止空缓冲、零长度和超出适配区的访问。
 * Write同步完成，返回前由底层负责页拆分和写完成等待；不能仅表示已加入异步队列。
 * 未来换存储介质需保留字节语义和完成语义，Flash擦除等由新适配负责。
 */
uint8_t ConfigPort_Read(uint16_t offset, uint8_t *buffer, uint16_t length);
uint8_t ConfigPort_Write(uint16_t offset, const uint8_t *buffer, uint16_t length);

/* 单调递增的uint32毫秒计数，允许自然回绕；不得改成秒或RTC日历时间。 */
uint32_t ConfigPort_NowMs(void);

/* 当前接口在主循环同步调用，不提供并发锁，也不增加中断内保存。 */
#ifdef __cplusplus
}
#endif
#endif

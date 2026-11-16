#ifndef MW_CLIMATE_PORT_H
#define MW_CLIMATE_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GPIO 初始化完成后调用。active 为 0 时关闭，非 0 时开启。
 * 每次调用写一次对应输出。接口表达控制请求，执行器状态需另行测量。
 */
void ClimatePort_SetFan(uint8_t active);
void ClimatePort_SetHeater(uint8_t active);

/* 返回主控输出引脚的逻辑开关状态。 */
uint8_t ClimatePort_GetFanOutput(void);
uint8_t ClimatePort_GetHeaterOutput(void);

#ifdef __cplusplus
}
#endif

#endif

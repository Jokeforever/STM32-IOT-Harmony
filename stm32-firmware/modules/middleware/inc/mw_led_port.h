/* LED运行期端口契约：不依赖HAL、定时器、按键或OLED。 */
#ifndef MW_LED_PORT_H
#define MW_LED_PORT_H
#include <stdint.h>
#define LED_PORT_DUTY_SCALE 1000U
typedef enum {
    LED_PORT_OK = 0,
    LED_PORT_INVALID_PERIOD = 1
} LedPortResult;

uint32_t LedPort_NowMs(void); /* uint32毫秒时基，按无符号差处理回绕 */
void LedPort_Enable(void);   /* 沿用现有PWM启动动作，不表示实体亮度已测量成功 */
void LedPort_Disable(void);  /* 沿用先停止PWM、再拉低引脚的动作 */
/* 输入0..1000，过大钳制至1000；0=合法CCR已写入，1=当前周期不受支持且不写入。 */
LedPortResult LedPort_SetDuty(uint16_t duty_permille);
#endif

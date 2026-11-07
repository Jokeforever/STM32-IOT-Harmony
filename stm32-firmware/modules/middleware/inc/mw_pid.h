/* 单向输出的位置式PID：只依赖标准整数类型，不访问HAL、配置、传感器或PWM。 */
#ifndef MW_PID_H
#define MW_PID_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int16_t target;
    uint16_t dead_zone;
    int32_t kp_x1000;           /* 0..10000，实际系数0..10；沿用当前配置精度 */
    int32_t ki_x1000;
    int32_t kd_x1000;
    uint16_t output_max;       /* 输出0..output_max，必须大于0 */
    uint32_t nominal_period_ms;/* 长间隔恢复使用的名义周期，必须大于0 */
    uint32_t max_gap_ms;       /* 超过此间隔重建历史，须不小于名义周期 */
} PID_Config;

typedef struct
{
    float integral;
    float last_error;
    uint32_t last_tick;
    uint8_t initialized;
} PID_State;

typedef enum
{
    PID_INVALID = -1,
    PID_HOLD = 0,
    PID_OUTPUT_READY = 1
} PID_Result;

void PID_Reset(PID_State *state, uint32_t now_ms);
PID_Result PID_Update(PID_State *state, const PID_Config *config,
                      int16_t measured, uint32_t now_ms, uint16_t *output);

/* 首次有效样本、死区、零时间间隔返回HOLD，不写output。死区清积分并更新时间，保留初始化状态。
 * 正常输出仅由调用方在OUTPUT_READY时写到硬件；INVALID不修改状态或output。
 * now_ms由调用方提供，uint32差值支持一次计数回绕，不支持跨越完整2^32毫秒的停顿。
 * 单个实例由单一上下文访问；修改控制参数后由调用方Reset。支持用不同State创建多个实例。
 * 当前是正向、非负输出的控制器，未增加双向输出、负增益、微分滤波或新的抗饱和策略。
 */
#ifdef __cplusplus
}
#endif
#endif

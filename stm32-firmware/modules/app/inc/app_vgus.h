#ifndef APP_VGUS_H
#define APP_VGUS_H

#include "sensor_data.h"
#include "mw_alarm.h"

typedef enum {
    VGUS_LINK_INITIALIZING,
    VGUS_LINK_CONNECTING_WIFI,
    VGUS_LINK_CONNECTING_CLOUD,
    VGUS_LINK_ONLINE,
    VGUS_LINK_LOCAL_ONLY
} VgusLinkState;

/* 初始化阶段和主循环均可调用，屏幕独立重启后也会收到最新提示。 */
void Vgus_UpdateLinkStatus(VgusLinkState state, uint32_t now_ms);

typedef enum {
    VGUS_RESET_UNKNOWN, VGUS_RESET_POWER, VGUS_RESET_PIN, VGUS_RESET_SOFTWARE,
    VGUS_RESET_WATCHDOG, VGUS_RESET_LOW_POWER
} VgusResetCause;

typedef enum {
    VGUS_DIAG_AT, VGUS_DIAG_IP, VGUS_DIAG_TCP, VGUS_DIAG_MQTT, VGUS_DIAG_STAGE_COUNT
} VgusDiagnosticStage;

void Vgus_SetResetCause(VgusResetCause cause);
void Vgus_RecordNetworkResult(VgusDiagnosticStage stage, uint8_t succeeded);

/* USART1 初始化后，每个采集周期发送一次环境读数与现有补水估算。 */
void Vgus_DisplayEnvironment(const SensorData *sensors, float water_volume_l, uint32_t sampled_ms);

/* 主循环执行控制逻辑后调用。 */
void Vgus_UpdateActuators(uint32_t now_ms);
void Vgus_UpdateAlarms(uint32_t now_ms, const SensorData *sensors,
                       const volatile Abnormal alarm_states[ALARM_COUNT]);

/* 主循环开始后接收触摸；两个回调仅用于 USART1 中断。 */
void Vgus_StartTouch(void);
void Vgus_TouchRxComplete(void);
void Vgus_TouchRxError(void);
void Vgus_ProcessTouch(void);

#endif

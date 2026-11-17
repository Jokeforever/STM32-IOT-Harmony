#include "app_vgus.h"
#include "app_config.h"
#include "app_commands.h"
#include "LED.h"
#include "bump.h"
#include "mw_pump_port.h"
#include "mw_climate_port.h"
#include "mw_climate.h"
#include "jdq.h"
#include "usart.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define VGUS_TEMPERATURE_VP   0x0200U
#define VGUS_HUMIDITY_VP      0x0240U
#define VGUS_LIGHT_VP         0x0280U
#define VGUS_CO2_VP           0x0288U
#define VGUS_SOIL_MOISTURE_VP 0x0290U
#define VGUS_WATER_VOLUME_VP  0x0298U
#define VGUS_TEXT_SLOT_BYTES  16U
#define VGUS_TX_TIMEOUT_MS    5U

/* 整个字槽都重写，内容缩短时不会留下前一帧的字符。 */
static void Vgus_FinishTextSlot(char *payload, size_t length, size_t capacity)
{
    memset(payload + length, ' ', capacity - length);
    const size_t text_end = (length + 1U) & ~(size_t)1U;
    ((uint8_t *)payload)[text_end] = 0xFFU;
    ((uint8_t *)payload)[text_end + 1U] = 0xFFU;
}

static void Vgus_WriteReading(uint16_t address, uint8_t valid, const char *format, ...)
{
    uint8_t frame[6U + VGUS_TEXT_SLOT_BYTES] = {
        0xA5U, 0x5AU, sizeof(frame) - 3U, 0x82U, (uint8_t)(address >> 8), (uint8_t)address
    };
    char *const payload = (char *)&frame[6];
    /* 最长显示 14 字节，末尾留一个字用于文本结束符。 */
    const size_t capacity = VGUS_TEXT_SLOT_BYTES - 1U;
    int length = 2;
    if (valid == 0U) {
        memcpy(payload, "--", 2U);
    } else {
        va_list args;
        va_start(args, format);
        length = vsnprintf(payload, capacity, format, args);
        va_end(args);
        if (length < 0 || (size_t)length >= capacity) {
            memcpy(payload, "--", 2U);
            length = 2;
        }
    }

    Vgus_FinishTextSlot(payload, (size_t)length, VGUS_TEXT_SLOT_BYTES);
    (void)HAL_UART_Transmit(&huart1, frame, sizeof(frame), VGUS_TX_TIMEOUT_MS);
}

void Vgus_DisplayEnvironment(const SensorData *sensors, float water_volume_l)
{
    Vgus_WriteReading(VGUS_TEMPERATURE_VP, sensors->valid_mask & SENSOR_VALID_TEMP,
                      "%u", (unsigned)sensors->temperature_c);
    Vgus_WriteReading(VGUS_HUMIDITY_VP, sensors->valid_mask & SENSOR_VALID_HUMI,
                      "%u", (unsigned)sensors->humidity_pct);
    Vgus_WriteReading(VGUS_LIGHT_VP, 1U, "%u", (unsigned)sensors->light_lux);
    Vgus_WriteReading(VGUS_CO2_VP, sensors->valid_mask & SENSOR_VALID_CO2,
                      "%u", (unsigned)sensors->co2_ppm);
    Vgus_WriteReading(VGUS_SOIL_MOISTURE_VP, 1U, "%u", (unsigned)sensors->soil_moisture_pct);
    Vgus_WriteReading(VGUS_WATER_VOLUME_VP, 1U, "%.1f", (double)water_volume_l);
}

/* 六个连续的 8 字文本槽，地址从 0x0300 起，每格偏移 8。 */
#define VGUS_ACTUATORS_VP        0x0300U
#define VGUS_ACTUATOR_SLOTS      6U
#define VGUS_MODE_MARKER_COUNT   6U
#define VGUS_MODE_MARKER_BYTES   4U
#define VGUS_MODE_MARKERS_BYTES  (VGUS_MODE_MARKER_COUNT * VGUS_MODE_MARKER_BYTES)
#define VGUS_STATUS_DATA_BYTES   (VGUS_MODE_MARKERS_BYTES + VGUS_ACTUATOR_SLOTS * VGUS_TEXT_SLOT_BYTES)
/* 标记占 0x02F4–0x02FF，紧接原状态区，按键地址 0x0330/0x0331 独立。 */
#define VGUS_STATUS_FRAME_VP     (VGUS_ACTUATORS_VP - VGUS_MODE_MARKERS_BYTES / 2U)
#define VGUS_STATUS_POLL_MS      200U
#define VGUS_STATUS_RESEND_MS    1000U
#define VGUS_STATUS_TIMEOUT_MS   15U

static const char *const led_modes[] = {
    [MODE_OFF] = "\xB9\xD8\xB1\xD5",
    [MODE_CALIBRATION] = "\xB1\xEA\xD7\xBC",
    [MODE_AUTO_PID] = "\xD7\xD4\xB6\xAF"
}; /* 关闭、标准、自动，GB2312 */
static const char *const pump_modes[] = {
    [Bump_OFF] = "\xB9\xD8\xB1\xD5",
    [Bump_CALIBRATION] = "\xC2\xF6\xB3\xE5\xB9\xE0\xB8\xC8",
    [Bump_AUTO] = "\xD7\xD4\xB6\xAF"
}; /* 关闭、脉冲灌溉、自动，GB2312 */
static const char *const output_states[] = {
    "\xB9\xD8\xB1\xD5", "\xBF\xAA\xC6\xF4"
}; /* 关闭、开启，GB2312 */

static const char *Vgus_ModeText(uint8_t mode, const char *const *modes, size_t count)
{
    return mode < count && modes[mode] != NULL ? modes[mode] : "--";
}

void Vgus_UpdateActuators(uint32_t now_ms)
{
    static uint32_t last_poll_ms;
    static uint32_t last_send_ms;
    static uint8_t sent;
    static uint8_t previous[VGUS_STATUS_DATA_BYTES];

    if (now_ms - last_poll_ms < VGUS_STATUS_POLL_MS) {
        return;
    }
    last_poll_ms = now_ms;

    uint8_t frame[6U + VGUS_STATUS_DATA_BYTES] = {
        0xA5U, 0x5AU, sizeof(frame) - 3U, 0x82U,
        (uint8_t)(VGUS_STATUS_FRAME_VP >> 8), (uint8_t)VGUS_STATUS_FRAME_VP
    };
    uint8_t *const payload = &frame[6];
    char *const status = (char *)&payload[VGUS_MODE_MARKERS_BYTES];
    const RuntimeConfig *config = AppConfig_Get();
    /* 和屏上两行按钮同序。只根据主控当前模式显示实心圆。 */
    const uint8_t selected[VGUS_MODE_MARKER_COUNT] = {
        config->led_mode == MODE_OFF,
        config->led_mode == MODE_CALIBRATION,
        config->led_mode == MODE_AUTO_PID,
        config->bump_mode == Bump_OFF,
        config->bump_mode == Bump_CALIBRATION,
        config->bump_mode == Bump_AUTO
    };
    for (size_t index = 0; index < VGUS_MODE_MARKER_COUNT; ++index) {
        uint8_t *const marker = &payload[index * VGUS_MODE_MARKER_BYTES];
        marker[0] = selected[index] ? 0xA1U : ' ';
        marker[1] = selected[index] ? 0xF1U : ' ';
        marker[2] = 0xFFU;
        marker[3] = 0xFFU;
    }
    char duty[8];
    const unsigned permille = LED_GetDutyPermille();
    (void)snprintf(duty, sizeof(duty), "%u.%u%%", permille / 10U, permille % 10U);
    const char *const rows[VGUS_ACTUATOR_SLOTS] = {
        Vgus_ModeText(config->led_mode, led_modes, sizeof(led_modes) / sizeof(led_modes[0])),
        duty,
        Vgus_ModeText(config->bump_mode, pump_modes, sizeof(pump_modes) / sizeof(pump_modes[0])),
        output_states[PumpPort_GetOutput() != 0U],
        output_states[ClimatePort_GetFanOutput() != 0U],
        output_states[ClimatePort_GetHeaterOutput() != 0U]
    };
    for (size_t index = 0; index < VGUS_ACTUATOR_SLOTS; ++index) {
        const size_t length = strlen(rows[index]);
        char *const slot = status + index * VGUS_TEXT_SLOT_BYTES;
        memcpy(slot, rows[index], length);
        Vgus_FinishTextSlot(slot, length, VGUS_TEXT_SLOT_BYTES);
    }

    /* 变化时发送，同一快照定期重发，支持串口屏独立复位。 */
    if (sent && now_ms - last_send_ms < VGUS_STATUS_RESEND_MS &&
        memcmp(previous, payload, sizeof(previous)) == 0) {
        return;
    }
    if (HAL_UART_Transmit(&huart1, frame, sizeof(frame), VGUS_STATUS_TIMEOUT_MS) == HAL_OK) {
        memcpy(previous, payload, sizeof(previous));
        last_send_ms = now_ms;
        sent = 1U;
    }
}

void Vgus_UpdateLinkStatus(VgusLinkState state, uint32_t now_ms)
{
    static const char *const labels[] = {
        [VGUS_LINK_INITIALIZING] = "\xD6\xF7\xBF\xD8\xB3\xF5\xCA\xBC\xBB\xAF", /* 主控初始化 */
        [VGUS_LINK_CONNECTING_WIFI] = "\xC1\xAC\xBD\xD3\x20\x57\x69\x2D\x46\x69", /* 连接 Wi-Fi */
        [VGUS_LINK_CONNECTING_CLOUD] = "\xC1\xAC\xBD\xD3\xD4\xC6\xB6\xCB", /* 连接云端 */
        [VGUS_LINK_ONLINE] = "\xD4\xC6\xB6\xCB\xD4\xDA\xCF\xDF", /* 云端在线 */
        [VGUS_LINK_LOCAL_ONLY] = "\xC0\xEB\xCF\xDF\x20\xB1\xBE\xB5\xD8\xBF\xC9\xBF\xD8", /* 离线 本地可控 */
    };
    static uint8_t sent;
    static VgusLinkState previous_state;
    static uint32_t last_send_ms;
    if ((unsigned)state >= sizeof(labels) / sizeof(labels[0])) {
        return;
    }
    if (sent && state == previous_state && now_ms - last_send_ms < VGUS_STATUS_RESEND_MS) {
        return;
    }

    uint8_t frame[6U + VGUS_TEXT_SLOT_BYTES] = {
        0xA5U, 0x5AU, sizeof(frame) - 3U, 0x82U, 0x03U, 0xD0U
    };
    char *const text = (char *)&frame[6];
    const size_t length = strlen(labels[state]);
    memcpy(text, labels[state], length);
    Vgus_FinishTextSlot(text, length, VGUS_TEXT_SLOT_BYTES);
    if (HAL_UART_Transmit(&huart1, frame, sizeof(frame), VGUS_TX_TIMEOUT_MS) == HAL_OK) {
        previous_state = state;
        last_send_ms = now_ms;
        sent = 1U;
    }
}

#define VGUS_ALARMS_VP       0x03E0U
#define VGUS_ALARM_ROWS      4U
#define VGUS_ALARM_FIELDS    3U
#define VGUS_ALARM_SLOTS     (1U + VGUS_ALARM_ROWS * VGUS_ALARM_FIELDS + 2U)
#define VGUS_ALARM_TX_MS     28U
#define VGUS_HISTORY_KEYS_VP 0x0460U
#define VGUS_HISTORY_VP      0x0470U
#define VGUS_HISTORY_COUNT   6U
#define VGUS_HISTORY_DETAIL_BYTES 24U
#define VGUS_HISTORY_ROW_BYTES (VGUS_TEXT_SLOT_BYTES + VGUS_HISTORY_DETAIL_BYTES)
#define VGUS_HISTORY_TX_MS   28U

enum { VGUS_TEMP_COOLING = HIGHCO2 + 1, VGUS_TEMP_HEATING };

typedef struct {
    uint32_t elapsed_s;
    uint16_t reading;
    uint8_t row;
    uint8_t state;
} VgusAlarmEvent;

static VgusAlarmEvent alarm_history[VGUS_HISTORY_COUNT];
static uint8_t history_next;
static uint8_t history_count;
static uint8_t history_dirty = 1U;
static volatile uint8_t history_open_requested;

/* 静态文案用 GB2312，屏幕第 14 页和首页告警数共用这份快照。 */
static const char alarm_normal[] = "\xD5\xFD\xB3\xA3";
static const char alarm_bright[] = "\xC6\xAB\xC1\xC1";
static const char alarm_dark[] = "\xC6\xAB\xB0\xB5";
static const char alarm_damp[] = "\xC6\xAB\xCA\xAA";
static const char alarm_dry[] = "\xC6\xAB\xB8\xC9";
static const char alarm_above[] = "\xB3\xAC\xCF\xDE";
static const char alarm_no_data[] = "\xCE\xDE\xD3\xD0\xD0\xA7\xD6\xB5";
static const char alarm_cooling[] = "\xCD\xA8\xB7\xE7\xD6\xD0";
static const char alarm_heating[] = "\xBC\xD3\xC8\xC8\xD6\xD0";
static const char alarm_idle[] = "\xB4\xFD\xBB\xFA";
static const char alarm_fan_temp[] = "\xB8\xDF\xCE\xC2\xCD\xA8\xB7\xE7";
static const char alarm_fan_co2[] = "\x43\x4F\x32\x20\xB3\xAC\xCF\xDE";
static const char alarm_fan_both[] = "\xB8\xDF\xCE\xC2\xBA\xCD\x43\x4F\x32";
static const char alarm_heater_low[] = "\xB5\xCD\xCE\xC2\xBC\xD3\xC8\xC8";

static void Vgus_PutAlarmLabel(char *slot, const char *label)
{
    const size_t length = strlen(label);
    memcpy(slot, label, length);
    Vgus_FinishTextSlot(slot, length, VGUS_TEXT_SLOT_BYTES);
}

static void Vgus_PutAlarmValue(char *slot, uint16_t first, uint16_t last, uint8_t is_range)
{
    const int length = is_range
        ? snprintf(slot, VGUS_TEXT_SLOT_BYTES, "%u-%u", (unsigned)first, (unsigned)last)
        : snprintf(slot, VGUS_TEXT_SLOT_BYTES, "%u", (unsigned)first);
    if (length < 0 || length > 14) {
        Vgus_PutAlarmLabel(slot, "--");
        return;
    }
    Vgus_FinishTextSlot(slot, (size_t)length, VGUS_TEXT_SLOT_BYTES);
}

static void Vgus_RecordAlarmChanges(uint32_t now_ms, const uint16_t readings[VGUS_ALARM_ROWS],
                                   const uint8_t states[VGUS_ALARM_ROWS],
                                   const uint8_t valid[VGUS_ALARM_ROWS])
{
    static uint8_t last_valid_states[VGUS_ALARM_ROWS];
    static uint32_t last_ms;
    static uint64_t elapsed_ms;

    /* 按无符号差值累加，HAL 的 32 位毫秒计数回绕后仍能连续计时。 */
    elapsed_ms += (uint32_t)(now_ms - last_ms);
    last_ms = now_ms;
    for (uint8_t row = 0U; row < VGUS_ALARM_ROWS; ++row) {
        if (!valid[row] || states[row] == last_valid_states[row]) {
            continue;
        }
        last_valid_states[row] = states[row];
        alarm_history[history_next] = (VgusAlarmEvent){
            (uint32_t)(elapsed_ms / 1000U), readings[row], row, states[row]
        };
        history_next = (history_next + 1U) % VGUS_HISTORY_COUNT;
        if (history_count < VGUS_HISTORY_COUNT) {
            ++history_count;
        }
        history_dirty = 1U;
    }
}

static void Vgus_SendAlarmHistory(uint32_t now_ms)
{
    static uint8_t retrying;
    static uint32_t last_attempt_ms;
    if (!history_dirty || (retrying && now_ms - last_attempt_ms < VGUS_STATUS_POLL_MS)) {
        return;
    }
    static const char *const recovery_labels[VGUS_ALARM_ROWS] = {
        "\xB9\xE2\xD5\xD5\xBB\xD6\xB8\xB4" /* 光照恢复 */, "\xCD\xC1\xC8\xC0\xBB\xD6\xB8\xB4" /* 土壤恢复 */,
        "\x43\x4F\x32\xBB\xD6\xB8\xB4" /* CO2恢复 */, "\xCE\xC2\xB6\xC8\xBB\xD6\xB8\xB4" /* 温度恢复 */
    };
    static const char *const event_labels[] = {
        [BRIGHT] = "\xB9\xE2\xD5\xD5\xC6\xAB\xC1\xC1" /* 光照偏亮 */,
        [DARK] = "\xB9\xE2\xD5\xD5\xC6\xAB\xB0\xB5" /* 光照偏暗 */,
        [DAMP] = "\xCD\xC1\xC8\xC0\xC6\xAB\xCA\xAA" /* 土壤偏湿 */,
        [DROUGHT] = "\xCD\xC1\xC8\xC0\xC6\xAB\xB8\xC9" /* 土壤偏干 */,
        [HIGHCO2] = "\x43\x4F\x32\xB3\xAC\xCF\xDE" /* CO2超限 */,
        [VGUS_TEMP_COOLING] = "\xB8\xDF\xCE\xC2\xCD\xA8\xB7\xE7" /* 高温通风 */,
        [VGUS_TEMP_HEATING] = "\xB5\xCD\xCE\xC2\xBC\xD3\xC8\xC8" /* 低温加热 */
    };
    static const char *const units[VGUS_ALARM_ROWS] = {" lx", "%", " ppm", "\xA1\xE6" /* ℃ */};

    uint8_t frame[6U + VGUS_HISTORY_COUNT * VGUS_HISTORY_ROW_BYTES] = {
        0xA5U, 0x5AU, sizeof(frame) - 3U, 0x82U, VGUS_HISTORY_VP >> 8, (uint8_t)VGUS_HISTORY_VP
    };
    for (uint8_t row = 0U; row < VGUS_HISTORY_COUNT; ++row) {
        char *const time = (char *)&frame[6U + row * VGUS_HISTORY_ROW_BYTES];
        char *const detail = time + VGUS_TEXT_SLOT_BYTES;
        size_t time_length = 0U;
        size_t detail_length = 0U;
        if (row < history_count) {
            const uint8_t index = (history_next + VGUS_HISTORY_COUNT - 1U - row) % VGUS_HISTORY_COUNT;
            const VgusAlarmEvent *const event = &alarm_history[index];
            const uint32_t sec = event->elapsed_s;
            time_length = (size_t)snprintf(time, VGUS_TEXT_SLOT_BYTES, "%02u:%02u:%02u",
                                         (unsigned)(sec / 3600U), (unsigned)(sec / 60U % 60U),
                                         (unsigned)(sec % 60U));
            const char *const label = event->state == NONE
                ? recovery_labels[event->row] : event_labels[event->state];
            detail_length = (size_t)snprintf(detail, VGUS_HISTORY_DETAIL_BYTES, "%s %u%s",
                                           label, (unsigned)event->reading, units[event->row]);
        } else if (row == 0U) {
            static const char empty[] = "\xD4\xDD\xCE\xDE\xB8\xE6\xBE\xAF\xBC\xC7\xC2\xBC" /* 暂无告警记录 */;
            detail_length = sizeof(empty) - 1U;
            memcpy(detail, empty, detail_length);
        }
        Vgus_FinishTextSlot(time, time_length <= VGUS_TEXT_SLOT_BYTES - 2U ? time_length : 0U, VGUS_TEXT_SLOT_BYTES);
        Vgus_FinishTextSlot(detail, detail_length <= VGUS_HISTORY_DETAIL_BYTES - 2U ? detail_length : 0U,
                            VGUS_HISTORY_DETAIL_BYTES);
    }
    last_attempt_ms = now_ms;
    retrying = HAL_UART_Transmit(&huart1, frame, sizeof(frame), VGUS_HISTORY_TX_MS) != HAL_OK;
    history_dirty = retrying;
}

void Vgus_UpdateAlarms(uint32_t now_ms, const SensorData *sensors,
                       const volatile Abnormal alarm_states[ALARM_COUNT])
{
    static uint32_t last_poll_ms;
    static uint32_t last_send_ms;
    static uint8_t sent;
    static uint8_t polled;
    static uint8_t previous[VGUS_ALARM_SLOTS * VGUS_TEXT_SLOT_BYTES];
    const uint8_t fan_reasons = Climate_GetFanReasons();
    const uint8_t heat_on = ClimatePort_GetHeaterOutput();
    const uint8_t temp_cooling = (fan_reasons & CLIMATE_FAN_TEMP) != 0U;
    const uint8_t valid[VGUS_ALARM_ROWS] = {
        1U, 1U, (sensors->valid_mask & SENSOR_VALID_CO2) != 0U,
        (sensors->valid_mask & SENSOR_VALID_TEMP) != 0U
    };
    const uint16_t readings[VGUS_ALARM_ROWS] = {
        sensors->light_lux, sensors->soil_moisture_pct, sensors->co2_ppm, sensors->temperature_c
    };
    const uint8_t states[VGUS_ALARM_ROWS] = {
        (uint8_t)alarm_states[ALARM_LIGHT_INDEX], (uint8_t)alarm_states[ALARM_SOIL_INDEX],
        alarm_states[ALARM_CO2_INDEX] == HIGHCO2 ? HIGHCO2 : NONE,
        temp_cooling ? VGUS_TEMP_COOLING : heat_on ? VGUS_TEMP_HEATING : NONE
    };

    Vgus_RecordAlarmChanges(now_ms, readings, states, valid);
    if (polled && now_ms - last_poll_ms < VGUS_STATUS_POLL_MS) {
        return;
    }
    polled = 1U;
    last_poll_ms = now_ms;

    const RuntimeConfig *config = AppConfig_Get();
    const struct {
        uint16_t lower;
        uint16_t upper;
        uint8_t range;
    } limits[VGUS_ALARM_ROWS] = {
        {config->ldr_low, config->ldr_high, 1U},
        {config->soil_low, config->soil_high, 1U},
        {config->co2_threshold, 0U, 0U},
        {config->temp_low_threshold, config->temp_threshold, 1U}
    };
    static const char *const statuses[] = {
        [NONE] = alarm_normal, [BRIGHT] = alarm_bright, [DARK] = alarm_dark,
        [DAMP] = alarm_damp, [DROUGHT] = alarm_dry, [HIGHCO2] = alarm_above,
        [VGUS_TEMP_COOLING] = alarm_cooling, [VGUS_TEMP_HEATING] = alarm_heating
    };
    static const char *const fan_labels[] = {
        [0] = alarm_idle,
        [CLIMATE_FAN_TEMP] = alarm_fan_temp,
        [CLIMATE_FAN_CO2] = alarm_fan_co2,
        [CLIMATE_FAN_TEMP | CLIMATE_FAN_CO2] = alarm_fan_both
    };

    uint8_t frame[6U + VGUS_ALARM_SLOTS * VGUS_TEXT_SLOT_BYTES] = {
        0xA5U, 0x5AU, sizeof(frame) - 3U, 0x82U, VGUS_ALARMS_VP >> 8, (uint8_t)VGUS_ALARMS_VP
    };
    char *const payload = (char *)&frame[6];
    uint8_t alarm_count = 0U;
    for (size_t index = 0; index < VGUS_ALARM_ROWS; ++index) {
        char *const slot = payload + (1U + index * VGUS_ALARM_FIELDS) * VGUS_TEXT_SLOT_BYTES;
        if (valid[index]) {
            Vgus_PutAlarmValue(slot, readings[index], 0U, 0U);
            alarm_count += states[index] != NONE;
        } else {
            Vgus_PutAlarmLabel(slot, "--");
        }
        Vgus_PutAlarmValue(slot + VGUS_TEXT_SLOT_BYTES, limits[index].lower, limits[index].upper, limits[index].range);
        Vgus_PutAlarmLabel(slot + 2U * VGUS_TEXT_SLOT_BYTES, valid[index] ? statuses[states[index]] : alarm_no_data);
    }
    Vgus_PutAlarmValue(payload, alarm_count, 0U, 0U);
    Vgus_PutAlarmLabel(payload + (VGUS_ALARM_SLOTS - 2U) * VGUS_TEXT_SLOT_BYTES,
                       fan_labels[fan_reasons & (CLIMATE_FAN_TEMP | CLIMATE_FAN_CO2)]);
    Vgus_PutAlarmLabel(payload + (VGUS_ALARM_SLOTS - 1U) * VGUS_TEXT_SLOT_BYTES,
                       heat_on ? alarm_heater_low : alarm_idle);
    if (sent && now_ms - last_send_ms < VGUS_STATUS_RESEND_MS &&
        memcmp(previous, payload, sizeof(previous)) == 0) {
        return;
    }
    if (HAL_UART_Transmit(&huart1, frame, sizeof(frame), VGUS_ALARM_TX_MS) == HAL_OK) {
        memcpy(previous, payload, sizeof(previous));
        last_send_ms = now_ms;
        sent = 1U;
    }
}

/* 0x83 上报一个按键字，或响应一次连续读取 2–3 个待应用值。 */
#define VGUS_TOUCH_TIMEOUT_MS       25U
#define VGUS_SETTINGS_TIMEOUT_MS    500U
#define VGUS_SETTINGS_MAX_VALUES    3U
#define VGUS_LED_KEYS_VP            0x0330U
#define VGUS_PUMP_KEYS_VP           0x0331U

typedef enum {
    VGUS_PAGE_CLOSED, VGUS_PAGE_SOIL, VGUS_PAGE_CLIMATE, VGUS_PAGE_LIGHT
} VgusSettingsPage;
enum { VGUS_SETTINGS_OPEN = 1, VGUS_SETTINGS_APPLY, VGUS_SETTINGS_CLOSE };

typedef struct {
    uint16_t draft_vp;
    uint16_t key_vp;
    uint16_t status_vp;
    uint8_t count;
    AppConfigKey config[VGUS_SETTINGS_MAX_VALUES];
    const char *conflict_notice;
} VgusSettingsGroup;

static const VgusSettingsGroup settings_groups[] = {
    [VGUS_PAGE_SOIL] = {
        0x0340U, 0x0342U, 0x0350U, 2U, {APP_CFG_SOIL_LOW, APP_CFG_SOIL_HIGH},
        "\xB8\xC9\xBA\xB5\xB8\xDF\xD3\xDA\xCA\xAA\xC8\xF3" /* 干旱高于湿润 */
    },
    [VGUS_PAGE_CLIMATE] = {
        0x0370U, 0x0373U, 0x0380U, 3U,
        {APP_CFG_TEMP_LOW_THRESHOLD, APP_CFG_TEMP_THRESHOLD, APP_CFG_CO2_THRESHOLD},
        "\xCF\xC2\xCF\xDE\xD0\xEB\xD0\xA1\xD3\xDA\xC9\xCF\xCF\xDE" /* 下限须小于上限 */
    },
    [VGUS_PAGE_LIGHT] = {
        0x03A0U, 0x03A3U, 0x03B0U, 3U,
        {APP_CFG_LIGHT_LOW, APP_CFG_LIGHT_HIGH, APP_CFG_MANUAL_PWM},
        "\xCF\xC2\xCF\xDE\xB4\xF3\xD3\xDA\xC9\xCF\xCF\xDE" /* 下限大于上限 */
    }
};

static const struct {
    uint16_t address;
    CommandKey command;
    uint8_t modes[3];
} touch_controls[] = {
    {VGUS_LED_KEYS_VP, COMMAND_LED, {MODE_OFF, MODE_CALIBRATION, MODE_AUTO_PID}},
    {VGUS_PUMP_KEYS_VP, COMMAND_PUMP, {Bump_OFF, Bump_CALIBRATION, Bump_AUTO}}
};
#define VGUS_TOUCH_CONTROL_COUNT (sizeof(touch_controls) / sizeof(touch_controls[0]))
#define VGUS_SETTINGS_GROUP_COUNT (sizeof(settings_groups) / sizeof(settings_groups[0]))

static uint8_t touch_byte;
static uint8_t touch_pos;
static uint8_t touch_frame[7U + 2U * VGUS_SETTINGS_MAX_VALUES];
static uint32_t last_touch_byte_ms;

static volatile uint8_t pending_touch_keys[VGUS_TOUCH_CONTROL_COUNT];
static volatile uint8_t settings_navigation_pending;
static volatile VgusSettingsPage requested_page;
static volatile VgusSettingsPage apply_page;
static volatile VgusSettingsPage reply_waiting_for;
static volatile uint8_t settings_reply_ready;
static volatile uint32_t settings_reply_started_ms;
static volatile uint16_t received_values[VGUS_SETTINGS_MAX_VALUES];

static VgusSettingsPage current_settings_page;
static uint8_t settings_seed_due;
static const char *settings_notice;
static const char *settings_notice_sent;
static uint16_t settings_values_sent[VGUS_SETTINGS_MAX_VALUES];

void Vgus_StartTouch(void)
{
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    (void)HAL_UART_Receive_IT(&huart1, &touch_byte, 1U);
}

static void Vgus_QueueTouchFrame(uint32_t now_ms)
{
    if (touch_frame[3] != 0x83U || touch_frame[2] != 4U + 2U * touch_frame[6]) {
        return;
    }
    const uint16_t address = ((uint16_t)touch_frame[4] << 8) | touch_frame[5];
    const uint16_t value = ((uint16_t)touch_frame[7] << 8) | touch_frame[8];
    if (touch_frame[6] == 1U) {
        if (address == VGUS_HISTORY_KEYS_VP && value == 1U) {
            history_open_requested = 1U;
            return;
        }
        for (size_t index = 0; index < VGUS_TOUCH_CONTROL_COUNT; ++index) {
            if (address == touch_controls[index].address &&
                value > 0U && value <= sizeof(touch_controls[index].modes)) {
                pending_touch_keys[index] = (uint8_t)value;
                return;
            }
        }
        for (size_t index = 1; index < VGUS_SETTINGS_GROUP_COUNT; ++index) {
            if (address != settings_groups[index].key_vp) continue;
            if (value == VGUS_SETTINGS_OPEN || value == VGUS_SETTINGS_CLOSE) {
                requested_page = value == VGUS_SETTINGS_OPEN ? (VgusSettingsPage)index : VGUS_PAGE_CLOSED;
                settings_navigation_pending = 1U;
                apply_page = VGUS_PAGE_CLOSED;
            } else if (value == VGUS_SETTINGS_APPLY) {
                apply_page = (VgusSettingsPage)index;
            }
            return;
        }
    } else if (reply_waiting_for != VGUS_PAGE_CLOSED && !settings_reply_ready &&
               now_ms - settings_reply_started_ms < VGUS_SETTINGS_TIMEOUT_MS) {
        const VgusSettingsGroup *group = &settings_groups[reply_waiting_for];
        if (address != group->draft_vp || touch_frame[6] != group->count) return;
        for (size_t index = 0; index < group->count; ++index) {
            const size_t offset = 7U + 2U * index;
            received_values[index] = ((uint16_t)touch_frame[offset] << 8) | touch_frame[offset + 1U];
        }
        settings_reply_ready = 1U;
    }
}

void Vgus_TouchRxComplete(void)
{
    const uint32_t now_ms = HAL_GetTick();
    const uint8_t byte = touch_byte;
    if (now_ms - last_touch_byte_ms >= VGUS_TOUCH_TIMEOUT_MS) {
        touch_pos = 0U;
    }
    last_touch_byte_ms = now_ms;

    if (touch_pos >= 3U) {
        touch_frame[touch_pos++] = byte;
        if (touch_pos == touch_frame[2] + 3U) {
            Vgus_QueueTouchFrame(now_ms);
            touch_pos = 0U;
        }
    } else if (byte == 0xA5U) {
        touch_frame[0] = byte;
        touch_pos = 1U;
    } else if ((touch_pos == 1U && byte == 0x5AU) ||
               (touch_pos == 2U && (byte == 6U || byte == 8U || byte == 10U))) {
        touch_frame[touch_pos++] = byte;
    } else {
        touch_pos = 0U;
    }

    (void)HAL_UART_Receive_IT(&huart1, &touch_byte, 1U);
}

void Vgus_TouchRxError(void)
{
    touch_pos = 0U;
    Vgus_StartTouch();
}

static CommandStatus Vgus_ApplySettings(VgusSettingsPage page, const uint16_t *values)
{
    if (page == VGUS_PAGE_SOIL) {
        const CommandEntry entries[] = {{COMMAND_SOIL_LOW, values[0]}, {COMMAND_SOIL_HIGH, values[1]}};
        return AppCommands_Execute(entries, 2U).status;
    }
    if (page == VGUS_PAGE_CLIMATE) {
        const CommandEntry entries[] = {
            {COMMAND_TEMP_LOW, values[0]}, {COMMAND_TEMP, values[1]}, {COMMAND_CO2, values[2]}
        };
        return AppCommands_Execute(entries, 3U).status;
    }

    /* 光照告警上下限和标准亮度是本地配置。先验证三项，再按可应用的次序写入。 */
    if (values[0] > 9999U || values[1] > 9999U || values[2] > 100U) return COMMAND_OUT_OF_RANGE;
    if (values[0] > values[1]) return COMMAND_CONFLICT;
    if (values[0] > AppConfig_Get()->ldr_high) {
        (void)AppConfig_Set(APP_CFG_LIGHT_HIGH, values[1]);
        (void)AppConfig_Set(APP_CFG_LIGHT_LOW, values[0]);
    } else {
        (void)AppConfig_Set(APP_CFG_LIGHT_LOW, values[0]);
        (void)AppConfig_Set(APP_CFG_LIGHT_HIGH, values[1]);
    }
    (void)AppConfig_Set(APP_CFG_MANUAL_PWM, values[2]);
    return COMMAND_OK;
}

static void Vgus_SendSettingsPage(const VgusSettingsGroup *group)
{
    uint16_t current[VGUS_SETTINGS_MAX_VALUES] = {0};
    for (size_t index = 0; index < group->count; ++index) {
        int32_t value;
        if (!AppConfig_GetValue(group->config[index], &value)) return;
        current[index] = (uint16_t)value;
    }

    if (settings_seed_due) {
        uint8_t draft[6U + 2U * VGUS_SETTINGS_MAX_VALUES] = {
            0xA5U, 0x5AU, 3U + 2U * group->count, 0x82U,
            (uint8_t)(group->draft_vp >> 8), (uint8_t)group->draft_vp
        };
        for (size_t index = 0; index < group->count; ++index) {
            draft[6U + 2U * index] = (uint8_t)(current[index] >> 8);
            draft[7U + 2U * index] = (uint8_t)current[index];
        }
        if (HAL_UART_Transmit(&huart1, draft, 6U + 2U * group->count, VGUS_TX_TIMEOUT_MS) != HAL_OK) return;
        settings_seed_due = 0U;
    }
    if (settings_notice == settings_notice_sent &&
        memcmp(current, settings_values_sent, sizeof(current)) == 0) return;

    const uint16_t size = 6U + (1U + group->count) * VGUS_TEXT_SLOT_BYTES;
    uint8_t frame[6U + (1U + VGUS_SETTINGS_MAX_VALUES) * VGUS_TEXT_SLOT_BYTES] = {
        0xA5U, 0x5AU, (uint8_t)(size - 3U), 0x82U,
        (uint8_t)(group->status_vp >> 8), (uint8_t)group->status_vp
    };
    char *const notice = (char *)&frame[6];
    const size_t notice_len = strlen(settings_notice);
    memcpy(notice, settings_notice, notice_len);
    Vgus_FinishTextSlot(notice, notice_len, VGUS_TEXT_SLOT_BYTES);
    const char *format = current_settings_page == VGUS_PAGE_SOIL ? "%u%%" : "%u";
    for (size_t index = 0; index < group->count; ++index) {
        char *slot = notice + (1U + index) * VGUS_TEXT_SLOT_BYTES;
        const int length = snprintf(slot, VGUS_TEXT_SLOT_BYTES, format, (unsigned)current[index]);
        if (length < 0 || length > 14) return;
        Vgus_FinishTextSlot(slot, (size_t)length, VGUS_TEXT_SLOT_BYTES);
    }
    if (HAL_UART_Transmit(&huart1, frame, size, VGUS_STATUS_TIMEOUT_MS) == HAL_OK) {
        settings_notice_sent = settings_notice;
        memcpy(settings_values_sent, current, sizeof(current));
    }
}

static void Vgus_ProcessSettings(void)
{
    uint16_t values[VGUS_SETTINGS_MAX_VALUES];
    const uint32_t now_ms = HAL_GetTick();
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    const uint8_t navigation = settings_navigation_pending;
    const VgusSettingsPage next_page = requested_page;
    const VgusSettingsPage apply = apply_page;
    const uint8_t reply = settings_reply_ready;
    for (size_t index = 0; index < VGUS_SETTINGS_MAX_VALUES; ++index) {
        values[index] = received_values[index];
    }
    const uint8_t timed_out = reply_waiting_for != VGUS_PAGE_CLOSED &&
        now_ms - settings_reply_started_ms >= VGUS_SETTINGS_TIMEOUT_MS;
    settings_navigation_pending = 0U;
    apply_page = VGUS_PAGE_CLOSED;
    settings_reply_ready = 0U;
    if (navigation || reply || timed_out) reply_waiting_for = VGUS_PAGE_CLOSED;
    __set_PRIMASK(primask);

    if (navigation) {
        current_settings_page = next_page;
        settings_seed_due = next_page != VGUS_PAGE_CLOSED;
        settings_notice = "\xD2\xD1\xB6\xC1\xC8\xA1"; /* 已读取 */
        settings_notice_sent = NULL;
    }
    if (current_settings_page == VGUS_PAGE_CLOSED) return;

    const VgusSettingsGroup *group = &settings_groups[current_settings_page];
    if (!navigation && reply) {
        switch (Vgus_ApplySettings(current_settings_page, values)) {
            case COMMAND_OK: settings_notice = "\xD2\xD1\xC9\xFA\xD0\xA7"; break; /* 已生效 */
            case COMMAND_CONFLICT: settings_notice = group->conflict_notice; break;
            case COMMAND_OUT_OF_RANGE: settings_notice = "\xB3\xAC\xB3\xF6\xB7\xB6\xCE\xA7"; break; /* 超出范围 */
            default: settings_notice = "\xD3\xA6\xD3\xC3\xCA\xA7\xB0\xDC"; break; /* 应用失败 */
        }
    } else if (!navigation && timed_out) {
        settings_notice = "\xB6\xC1\xC8\xA1\xB3\xAC\xCA\xB1"; /* 读取超时 */
    } else if (!navigation && apply == current_settings_page && !settings_seed_due &&
               reply_waiting_for == VGUS_PAGE_CLOSED) {
        uint8_t read[] = {
            0xA5U, 0x5AU, 4U, 0x83U, (uint8_t)(group->draft_vp >> 8),
            (uint8_t)group->draft_vp, group->count
        };
        __disable_irq();
        settings_reply_started_ms = now_ms;
        reply_waiting_for = current_settings_page;
        __set_PRIMASK(primask);
        if (HAL_UART_Transmit(&huart1, read, sizeof(read), VGUS_TX_TIMEOUT_MS) == HAL_OK) {
            settings_notice = "\xD3\xA6\xD3\xC3\xD6\xD0"; /* 应用中 */
        } else {
            __disable_irq();
            reply_waiting_for = VGUS_PAGE_CLOSED;
            settings_reply_ready = 0U;
            __set_PRIMASK(primask);
            settings_notice = "\xCD\xA8\xD0\xC5\xCA\xA7\xB0\xDC"; /* 通信失败 */
        }
    }
    Vgus_SendSettingsPage(group);
}

void Vgus_ProcessTouch(void)
{
    uint8_t keys[VGUS_TOUCH_CONTROL_COUNT];
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    for (size_t index = 0; index < VGUS_TOUCH_CONTROL_COUNT; ++index) {
        keys[index] = pending_touch_keys[index];
        pending_touch_keys[index] = 0U;
    }
    const uint8_t history_open = history_open_requested;
    history_open_requested = 0U;
    __set_PRIMASK(primask);

    CommandEntry commands[VGUS_TOUCH_CONTROL_COUNT];
    uint8_t count = 0U;
    for (size_t index = 0; index < VGUS_TOUCH_CONTROL_COUNT; ++index) {
        if (keys[index] != 0U) {
            commands[count].key = touch_controls[index].command;
            commands[count].value = touch_controls[index].modes[keys[index] - 1U];
            ++count;
        }
    }
    if (count != 0U) {
        (void)AppCommands_Execute(commands, count);
    }
    Vgus_ProcessSettings();
    if (history_open) {
        history_dirty = 1U;
    }
    Vgus_SendAlarmHistory(HAL_GetTick());
}

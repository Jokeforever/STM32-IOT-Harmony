#include "app_vgus.h"
#include "app_config.h"
#include "app_commands.h"
#include "LED.h"
#include "bump.h"
#include "mw_pump_port.h"
#include "mw_climate_port.h"
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
static void Vgus_FinishTextSlot(char *payload, size_t length)
{
    memset(payload + length, ' ', VGUS_TEXT_SLOT_BYTES - length);
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

    Vgus_FinishTextSlot(payload, (size_t)length);
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
        Vgus_FinishTextSlot(slot, length);
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

/* 收帧只接受 1 字触摸键值或 2 字的灌溉参数读取应答。 */
#define VGUS_TOUCH_TIMEOUT_MS       25U
#define VGUS_SOIL_REPLY_TIMEOUT_MS  500U
#define VGUS_LED_KEYS_VP            0x0330U
#define VGUS_PUMP_KEYS_VP           0x0331U
#define VGUS_SOIL_DRAFT_VP          0x0340U
#define VGUS_SOIL_KEYS_VP           0x0342U
#define VGUS_SOIL_STATUS_VP         0x0350U

enum {
    VGUS_SOIL_OPEN = 1,
    VGUS_SOIL_APPLY = 2,
    VGUS_SOIL_CLOSE = 3
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

static uint8_t touch_byte;
static uint8_t touch_pos;
static uint8_t touch_frame[11];
static uint32_t last_touch_byte_ms;

/* 中断只交付完整帧。主循环统一应用模式及阈值。 */
static volatile uint8_t pending_touch_keys[VGUS_TOUCH_CONTROL_COUNT];
static volatile uint8_t pending_soil_navigation;
static volatile uint8_t pending_soil_apply;
static volatile uint8_t soil_reply_waiting;
static volatile uint8_t soil_reply_ready;
static volatile uint32_t soil_reply_started_ms;
static volatile uint16_t requested_soil_low;
static volatile uint16_t requested_soil_high;

static uint8_t soil_page_open;
static uint8_t soil_seed_due;
static const char *soil_notice;
static const char *soil_notice_sent;
static uint16_t soil_low_sent;
static uint16_t soil_high_sent;

void Vgus_StartTouch(void)
{
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    (void)HAL_UART_Receive_IT(&huart1, &touch_byte, 1U);
}

static void Vgus_QueueTouchFrame(uint32_t now_ms)
{
    if (touch_frame[3] != 0x83U) {
        return;
    }
    const uint16_t address = ((uint16_t)touch_frame[4] << 8) | touch_frame[5];
    const uint16_t value = ((uint16_t)touch_frame[7] << 8) | touch_frame[8];
    if (touch_frame[2] == 6U && touch_frame[6] == 1U) {
        for (size_t index = 0; index < VGUS_TOUCH_CONTROL_COUNT; ++index) {
            if (address == touch_controls[index].address &&
                value > 0U && value <= sizeof(touch_controls[index].modes)) {
                pending_touch_keys[index] = (uint8_t)value;
                return;
            }
        }
        if (address == VGUS_SOIL_KEYS_VP) {
            if (value == VGUS_SOIL_OPEN || value == VGUS_SOIL_CLOSE) {
                pending_soil_navigation = (uint8_t)value;
                pending_soil_apply = 0U;
            } else if (value == VGUS_SOIL_APPLY) {
                pending_soil_apply = 1U;
            }
        }
    } else if (touch_frame[2] == 8U && touch_frame[6] == 2U &&
               address == VGUS_SOIL_DRAFT_VP && soil_reply_waiting && !soil_reply_ready &&
               now_ms - soil_reply_started_ms < VGUS_SOIL_REPLY_TIMEOUT_MS) {
        requested_soil_low = value;
        requested_soil_high = ((uint16_t)touch_frame[9] << 8) | touch_frame[10];
        soil_reply_ready = 1U;
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

    if (byte == 0xA5U) {
        /* 当前支持的控件值不会含 0xA5。新帧头可从截断帧立即恢复。 */
        touch_frame[0] = byte;
        touch_pos = 1U;
    } else if ((touch_pos == 1U && byte == 0x5AU) ||
               (touch_pos == 2U && (byte == 6U || byte == 8U)) || touch_pos >= 3U) {
        touch_frame[touch_pos++] = byte;
        if (touch_pos >= 3U && touch_pos == touch_frame[2] + 3U) {
            Vgus_QueueTouchFrame(now_ms);
            touch_pos = 0U;
        }
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

static void Vgus_SendSoilPage(void)
{
    const RuntimeConfig *config = AppConfig_Get();
    if (soil_seed_due) {
        uint8_t draft[] = {
            0xA5U, 0x5AU, 7U, 0x82U, VGUS_SOIL_DRAFT_VP >> 8, (uint8_t)VGUS_SOIL_DRAFT_VP,
            0U, (uint8_t)config->soil_low, 0U, (uint8_t)config->soil_high
        };
        if (HAL_UART_Transmit(&huart1, draft, sizeof(draft), VGUS_TX_TIMEOUT_MS) != HAL_OK) {
            return;
        }
        soil_seed_due = 0U;
    }
    if (soil_notice == soil_notice_sent && config->soil_low == soil_low_sent &&
        config->soil_high == soil_high_sent) {
        return;
    }

    uint8_t frame[6U + 3U * VGUS_TEXT_SLOT_BYTES] = {
        0xA5U, 0x5AU, sizeof(frame) - 3U, 0x82U,
        VGUS_SOIL_STATUS_VP >> 8, (uint8_t)VGUS_SOIL_STATUS_VP
    };
    char *const notice = (char *)&frame[6];
    char *const low = notice + VGUS_TEXT_SLOT_BYTES;
    char *const high = low + VGUS_TEXT_SLOT_BYTES;
    const size_t notice_len = strlen(soil_notice);
    memcpy(notice, soil_notice, notice_len);
    Vgus_FinishTextSlot(notice, notice_len);
    const int low_len = snprintf(low, VGUS_TEXT_SLOT_BYTES, "%u%%", (unsigned)config->soil_low);
    const int high_len = snprintf(high, VGUS_TEXT_SLOT_BYTES, "%u%%", (unsigned)config->soil_high);
    if (low_len < 0 || high_len < 0 || low_len > 14 || high_len > 14) {
        return;
    }
    Vgus_FinishTextSlot(low, (size_t)low_len);
    Vgus_FinishTextSlot(high, (size_t)high_len);
    if (HAL_UART_Transmit(&huart1, frame, sizeof(frame), VGUS_STATUS_TIMEOUT_MS) == HAL_OK) {
        soil_notice_sent = soil_notice;
        soil_low_sent = config->soil_low;
        soil_high_sent = config->soil_high;
    }
}

static void Vgus_ProcessSoilSettings(void)
{
    const uint32_t now_ms = HAL_GetTick();
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    const uint8_t navigation = pending_soil_navigation;
    const uint8_t apply = pending_soil_apply;
    const uint8_t reply = soil_reply_ready;
    const uint16_t low = requested_soil_low;
    const uint16_t high = requested_soil_high;
    const uint8_t timed_out = soil_reply_waiting &&
        now_ms - soil_reply_started_ms >= VGUS_SOIL_REPLY_TIMEOUT_MS;
    pending_soil_navigation = 0U;
    pending_soil_apply = 0U;
    soil_reply_ready = 0U;
    if (navigation || reply || timed_out) {
        soil_reply_waiting = 0U;
    }
    __set_PRIMASK(primask);

    if (navigation) {
        soil_page_open = navigation == VGUS_SOIL_OPEN;
        soil_seed_due = soil_page_open;
        soil_notice = "\xD2\xD1\xB6\xC1\xC8\xA1"; /* 已读取 */
        soil_notice_sent = NULL;
    }
    if (!soil_page_open) {
        return;
    }

    if (!navigation && reply) {
        const CommandEntry commands[] = {{COMMAND_SOIL_LOW, low}, {COMMAND_SOIL_HIGH, high}};
        const CommandStatus status = AppCommands_Execute(commands, 2U).status;
        switch (status) {
            case COMMAND_OK: soil_notice = "\xD2\xD1\xC9\xFA\xD0\xA7"; break; /* 已生效 */
            case COMMAND_CONFLICT: soil_notice = "\xB8\xC9\xBA\xB5\xB8\xDF\xD3\xDA\xCA\xAA\xC8\xF3"; break; /* 干旱高于湿润 */
            case COMMAND_OUT_OF_RANGE: soil_notice = "\xB3\xAC\xB3\xF6\xB7\xB6\xCE\xA7"; break; /* 超出范围 */
            default: soil_notice = "\xD3\xA6\xD3\xC3\xCA\xA7\xB0\xDC"; break; /* 应用失败 */
        }
    } else if (!navigation && timed_out) {
        soil_notice = "\xB6\xC1\xC8\xA1\xB3\xAC\xCA\xB1"; /* 读取超时 */
    } else if (!navigation && apply && !soil_seed_due && !soil_reply_waiting) {
        uint8_t read[] = {
            0xA5U, 0x5AU, 4U, 0x83U, VGUS_SOIL_DRAFT_VP >> 8, (uint8_t)VGUS_SOIL_DRAFT_VP, 2U
        };
        __disable_irq();
        soil_reply_started_ms = now_ms;
        soil_reply_waiting = 1U;
        __set_PRIMASK(primask);
        if (HAL_UART_Transmit(&huart1, read, sizeof(read), VGUS_TX_TIMEOUT_MS) == HAL_OK) {
            soil_notice = "\xD3\xA6\xD3\xC3\xD6\xD0"; /* 应用中 */
        } else {
            __disable_irq();
            soil_reply_waiting = 0U;
            soil_reply_ready = 0U;
            __set_PRIMASK(primask);
            soil_notice = "\xCD\xA8\xD0\xC5\xCA\xA7\xB0\xDC"; /* 通信失败 */
        }
    }
    Vgus_SendSoilPage();
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
    Vgus_ProcessSoilSettings();
}

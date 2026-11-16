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

/* A5 5A 06 83 03 <地址低字节> 01 00 <键值>。 */
#define VGUS_TOUCH_TIMEOUT_MS    25U
#define VGUS_TOUCH_ADDRESS_INDEX 5U
#define VGUS_TOUCH_LED_LOW       0x30U

static const uint8_t touch_prefix[] = {
    0xA5U, 0x5AU, 0x06U, 0x83U, 0x03U, VGUS_TOUCH_LED_LOW, 0x01U, 0x00U
};
/* 连续地址 0x0330 和 0x0331 分别对应照明和水泵。 */
static const struct {
    CommandKey command;
    uint8_t modes[3];
} touch_controls[] = {
    {COMMAND_LED, {MODE_OFF, MODE_CALIBRATION, MODE_AUTO_PID}},
    {COMMAND_PUMP, {Bump_OFF, Bump_CALIBRATION, Bump_AUTO}}
};
#define VGUS_TOUCH_CONTROL_COUNT (sizeof(touch_controls) / sizeof(touch_controls[0]))

static uint8_t touch_byte;
static uint8_t touch_pos;
static uint8_t touch_control;
static uint32_t last_touch_byte_ms;
/* 各执行器独立保留最后一个有效目标。 */
static volatile uint8_t pending_touch_keys[VGUS_TOUCH_CONTROL_COUNT];

void Vgus_StartTouch(void)
{
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    (void)HAL_UART_Receive_IT(&huart1, &touch_byte, 1U);
}

void Vgus_TouchRxComplete(void)
{
    const uint32_t now_ms = HAL_GetTick();
    if (now_ms - last_touch_byte_ms >= VGUS_TOUCH_TIMEOUT_MS) {
        touch_pos = 0U;
    }
    last_touch_byte_ms = now_ms;

    const uint8_t control = (uint8_t)(touch_byte - VGUS_TOUCH_LED_LOW);
    if (touch_pos == VGUS_TOUCH_ADDRESS_INDEX && control < VGUS_TOUCH_CONTROL_COUNT) {
        touch_control = control;
        ++touch_pos;
    } else if (touch_pos < sizeof(touch_prefix) && touch_byte == touch_prefix[touch_pos]) {
        ++touch_pos;
    } else {
        if (touch_pos == sizeof(touch_prefix) &&
            touch_byte > 0U && touch_byte <= sizeof(touch_controls[0].modes)) {
            pending_touch_keys[touch_control] = touch_byte;
        }
        /* 遇到新的帧头，直接用当前字节开始同步。 */
        touch_pos = (touch_byte == touch_prefix[0]) ? 1U : 0U;
    }

    (void)HAL_UART_Receive_IT(&huart1, &touch_byte, 1U);
}

void Vgus_TouchRxError(void)
{
    touch_pos = 0U;
    Vgus_StartTouch();
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
}

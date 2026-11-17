#include "app_network_startup.h"
#include "app_vgus.h"
#include "esp8266.h"
#include "onenet.h"

#define BOOT_RETRY_AT_MS      500U
#define BOOT_RETRY_JOIN_MS   1000U
#define BOOT_WIFI_SETTLE_MS   800U
#define BOOT_CLOUD_SETTLE_MS  500U
#define BOOT_OFFLINE_WAIT_MS 10000U

typedef enum {
    BOOT_CHECK_AT,
    BOOT_SET_MODE,
    BOOT_SET_DHCP,
    BOOT_JOIN,
    BOOT_CLOSE_TCP,
    BOOT_OPEN_TCP,
    BOOT_LOGIN,
    BOOT_SUBSCRIBE,
    BOOT_OFFLINE,
    BOOT_ONLINE
} StartupStep;

typedef struct {
    Esp8266StartupCommand command;
    uint8_t attempts;
    uint16_t retry_ms;
} StartupAtSettings;

static const StartupAtSettings at_steps[] = {
    [BOOT_CHECK_AT] = {ESP8266_START_AT, 6U, BOOT_RETRY_AT_MS},
    [BOOT_SET_MODE] = {ESP8266_START_MODE, 6U, BOOT_RETRY_AT_MS},
    [BOOT_SET_DHCP] = {ESP8266_START_DHCP, 6U, BOOT_RETRY_AT_MS},
    [BOOT_JOIN] = {ESP8266_START_JOIN, 4U, BOOT_RETRY_JOIN_MS},
    [BOOT_CLOSE_TCP] = {ESP8266_START_CLOSE, 1U, 0U},
    [BOOT_OPEN_TCP] = {ESP8266_START_TCP, 6U, BOOT_RETRY_AT_MS}
};

static StartupStep step;
static uint8_t started;
static uint8_t attempt;
static uint8_t login_attempts;
static uint32_t pause_started_ms;
static uint32_t pause_ms;

static void Startup_Move(StartupStep next, uint32_t now_ms, uint32_t delay_ms)
{
    step = next;
    attempt = 0U;
    pause_started_ms = now_ms;
    pause_ms = delay_ms;
}

static void Startup_Offline(uint32_t now_ms)
{
    ESP8266_StartupEnd(0U);
    Startup_Move(BOOT_OFFLINE, now_ms, BOOT_OFFLINE_WAIT_MS);
}

static AppNetworkStartupState Startup_Status(void)
{
    if (step <= BOOT_JOIN) return APP_STARTUP_WIFI;
    if (step == BOOT_OFFLINE) return APP_STARTUP_OFFLINE;
    if (step == BOOT_ONLINE) return APP_STARTUP_ONLINE;
    return APP_STARTUP_CLOUD;
}

static void Startup_FinishAt(uint32_t now_ms, uint8_t ok)
{
    if (step == BOOT_CHECK_AT) Vgus_RecordNetworkResult(VGUS_DIAG_AT, ok);
    if (step == BOOT_JOIN) Vgus_RecordNetworkResult(VGUS_DIAG_IP, ok);
    if (step == BOOT_OPEN_TCP) Vgus_RecordNetworkResult(VGUS_DIAG_TCP, ok);

    const StartupAtSettings *const settings = &at_steps[step];
    if (!ok && attempt < settings->attempts) {
        pause_started_ms = now_ms;
        pause_ms = settings->retry_ms;
        return;
    }

    switch (step) {
        case BOOT_CHECK_AT:
            if (ok) Startup_Move(BOOT_SET_MODE, now_ms, 0U);
            else Startup_Offline(now_ms);
            break;
        case BOOT_SET_MODE:
            Startup_Move(BOOT_SET_DHCP, now_ms, 0U);
            break;
        case BOOT_SET_DHCP:
            Startup_Move(BOOT_JOIN, now_ms, 0U);
            break;
        case BOOT_JOIN:
            if (ok) Startup_Move(BOOT_CLOSE_TCP, now_ms, BOOT_WIFI_SETTLE_MS);
            else Startup_Offline(now_ms);
            break;
        case BOOT_CLOSE_TCP:
            /* 无旧连接时模块可返回 ERROR，也继续打开新的连接。 */
            Startup_Move(BOOT_OPEN_TCP, now_ms, 0U);
            break;
        case BOOT_OPEN_TCP:
            if (ok) Startup_Move(BOOT_LOGIN, now_ms, BOOT_CLOUD_SETTLE_MS);
            else Startup_Offline(now_ms);
            break;
        default:
            break;
    }
}

static void Startup_FinishMqtt(uint32_t now_ms, uint8_t ok)
{
    if (step == BOOT_SUBSCRIBE) {
        if (ok) {
            ESP8266_StartupEnd(1U);
            Startup_Move(BOOT_ONLINE, now_ms, 0U);
        } else {
            Startup_Offline(now_ms);
        }
        return;
    }

    Vgus_RecordNetworkResult(VGUS_DIAG_MQTT, ok);
    if (ok) {
        Startup_Move(BOOT_SUBSCRIBE, now_ms, 0U);
    } else if (login_attempts < 6U) {
        Startup_Move(BOOT_CLOSE_TCP, now_ms, BOOT_RETRY_AT_MS);
    } else {
        Startup_Offline(now_ms);
    }
}

AppNetworkStartupState AppNetworkStartup_Poll(uint32_t now_ms)
{
    if (step == BOOT_ONLINE) {
        if (!OneNet_IsConnected()) {
            login_attempts = 0U;
            Startup_Move(BOOT_CLOSE_TCP, now_ms, BOOT_RETRY_AT_MS);
        }
        return Startup_Status();
    }
    if (now_ms - pause_started_ms < pause_ms) return Startup_Status();
    if (step == BOOT_OFFLINE) {
        login_attempts = 0U;
        Startup_Move(BOOT_CHECK_AT, now_ms, 0U);
        return Startup_Status();
    }

    const uint8_t mqtt = step == BOOT_LOGIN || step == BOOT_SUBSCRIBE;
    Esp8266StartupResult result;
    if (!started) {
        ++attempt;
        if (step == BOOT_LOGIN) ++login_attempts;
        started = mqtt
            ? OneNet_StartupBegin(step == BOOT_SUBSCRIBE, now_ms)
            : ESP8266_StartupBeginCommand(at_steps[step].command, now_ms);
        result = started ? ESP8266_START_WAIT : ESP8266_START_FAIL;
    } else {
        result = mqtt ? OneNet_StartupPoll(now_ms) : ESP8266_StartupPollCommand(now_ms);
    }
    if (result == ESP8266_START_WAIT) return Startup_Status();

    started = 0U;
    if (mqtt) Startup_FinishMqtt(now_ms, result == ESP8266_START_OK);
    else Startup_FinishAt(now_ms, result == ESP8266_START_OK);
    return Startup_Status();
}

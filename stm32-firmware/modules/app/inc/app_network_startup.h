#ifndef APP_NETWORK_STARTUP_H
#define APP_NETWORK_STARTUP_H

#include <stdint.h>

typedef enum {
    APP_STARTUP_WIFI,
    APP_STARTUP_CLOUD,
    APP_STARTUP_OFFLINE,
    APP_STARTUP_ONLINE
} AppNetworkStartupState;

/* 首次入网成功前独占 ESP8266 应答。每次调用只推进一步。 */
AppNetworkStartupState AppNetworkStartup_Poll(uint32_t now_ms);

#endif

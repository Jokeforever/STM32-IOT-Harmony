/* 业务适配：命令规则与当前RuntimeConfig/LED适配分开。 */
#include "app_commands.h"
#include "app_config.h"
#include "LED.h"

static uint8_t report_pending;
static const AppConfigKey config_keys[COMMAND_KEY_COUNT] = {
    APP_CFG_LED_MODE, APP_CFG_BUMP_MODE, APP_CFG_CO2_THRESHOLD,
    APP_CFG_SOIL_LOW, APP_CFG_SOIL_HIGH, APP_CFG_TEMP_THRESHOLD, APP_CFG_TEMP_LOW_THRESHOLD
};

static uint8_t Apply(CommandKey key, int32_t value, uint8_t *changed)
{
    AppConfigResult result = AppConfig_Set(config_keys[key], value);
    if (result == APP_CONFIG_REJECTED) return 0U;
    if (result == APP_CONFIG_CHANGED) {
        *changed = 1U;
        if (key == COMMAND_LED) Mode_Switch();
    }
    return 1U;
}

AppCommandResult AppCommands_Execute(const CommandEntry *entries, uint8_t count)
{
    CommandState current, desired;
    AppCommandResult result = {COMMAND_INTERNAL, 0U};
    uint8_t requested = 0U;
    unsigned i;
    for (i = 0; i < COMMAND_KEY_COUNT; ++i)
        if (!AppConfig_GetValue(config_keys[i], &current.values[i])) return result;
    result.status = Command_Plan(entries, count, &current, &desired);
    if (result.status != COMMAND_OK) return result;
    for (i = 0; i < count; ++i) requested |= (uint8_t)(1U << (unsigned)entries[i].key);
    /* 先扩高端再提高低端，或先降低低端再降低高端，避免合法批次被中间钳制。 */
    if (desired.values[COMMAND_SOIL_LOW] > current.values[COMMAND_SOIL_HIGH]) {
        if ((requested & (1U << COMMAND_SOIL_HIGH)) && !Apply(COMMAND_SOIL_HIGH, desired.values[COMMAND_SOIL_HIGH], &result.changed)) goto failed;
        if ((requested & (1U << COMMAND_SOIL_LOW)) && !Apply(COMMAND_SOIL_LOW, desired.values[COMMAND_SOIL_LOW], &result.changed)) goto failed;
    } else {
        if ((requested & (1U << COMMAND_SOIL_LOW)) && !Apply(COMMAND_SOIL_LOW, desired.values[COMMAND_SOIL_LOW], &result.changed)) goto failed;
        if ((requested & (1U << COMMAND_SOIL_HIGH)) && !Apply(COMMAND_SOIL_HIGH, desired.values[COMMAND_SOIL_HIGH], &result.changed)) goto failed;
    }
    /* 温度批次先预检最终上下限；按中间状态合法的顺序应用两项。 */
    if (desired.values[COMMAND_TEMP_LOW] >= current.values[COMMAND_TEMP]) {
        if ((requested & (1U << COMMAND_TEMP)) && !Apply(COMMAND_TEMP, desired.values[COMMAND_TEMP], &result.changed)) goto failed;
        if ((requested & (1U << COMMAND_TEMP_LOW)) && !Apply(COMMAND_TEMP_LOW, desired.values[COMMAND_TEMP_LOW], &result.changed)) goto failed;
    } else {
        if ((requested & (1U << COMMAND_TEMP_LOW)) && !Apply(COMMAND_TEMP_LOW, desired.values[COMMAND_TEMP_LOW], &result.changed)) goto failed;
        if ((requested & (1U << COMMAND_TEMP)) && !Apply(COMMAND_TEMP, desired.values[COMMAND_TEMP], &result.changed)) goto failed;
    }
    for (i = 0; i < COMMAND_KEY_COUNT; ++i) {
        if (i == COMMAND_SOIL_LOW || i == COMMAND_SOIL_HIGH || i == COMMAND_TEMP || i == COMMAND_TEMP_LOW) continue;
        if ((requested & (1U << i)) && !Apply((CommandKey)i, desired.values[i], &result.changed)) goto failed;
    }
    /* 同值成功也请求一次新快照，但不重复标脏、重置PID或重启模式。 */
    report_pending = 1U;
    return result;
failed:
    result.status = COMMAND_INTERNAL;
    if (result.changed) report_pending = 1U;
    return result;
}

uint8_t AppCommands_ReportPending(void) { return report_pending; }
uint8_t AppCommands_ReportDue(uint32_t now, uint32_t last_attempt, uint8_t connected)
{
    return (uint8_t)(report_pending && connected && (uint32_t)(now - last_attempt) >= APP_COMMAND_REPORT_GAP_MS);
}
void AppCommands_ReportSent(void) { report_pending = 0U; }

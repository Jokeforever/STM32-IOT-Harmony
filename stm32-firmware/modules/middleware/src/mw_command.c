#include "mw_command.h"
#include <stddef.h>
#include <string.h>

static const char *const names[COMMAND_KEY_COUNT] = {
    "led", "bump", "CO2threhold", "droughtthrehold", "moistthrehold", "tempthrehold", "temp_low_threshold"
};
static const int32_t minimum[COMMAND_KEY_COUNT] = {0, 0, 350, 0, 0, 1, 0};
static const int32_t maximum[COMMAND_KEY_COUNT] = {2, 2, 4000, 100, 100, 100, 99};

CommandKey Command_FindKey(const char *name)
{
    unsigned i;
    if (name != NULL) for (i = 0; i < COMMAND_KEY_COUNT; ++i)
        if (strcmp(name, names[i]) == 0) return (CommandKey)i;
    return COMMAND_KEY_COUNT;
}

CommandStatus Command_Plan(const CommandEntry *entries, uint8_t count,
                           const CommandState *current, CommandState *desired)
{
    uint8_t seen = 0U;
    unsigned i;
    CommandState candidate;
    if (entries == NULL || current == NULL || desired == NULL || count == 0U || count > COMMAND_CAPACITY)
        return COMMAND_BAD_REQUEST;
    candidate = *current;
    for (i = 0; i < count; ++i) {
        unsigned key = (unsigned)entries[i].key;
        if (key >= COMMAND_KEY_COUNT) return COMMAND_UNKNOWN_KEY;
        if ((seen & (1U << key)) != 0U) return COMMAND_DUPLICATE;
        seen |= (uint8_t)(1U << key);
        if (entries[i].value < minimum[key] || entries[i].value > maximum[key]) return COMMAND_OUT_OF_RANGE;
        candidate.values[key] = entries[i].value;
    }
    if (candidate.values[COMMAND_SOIL_LOW] > candidate.values[COMMAND_SOIL_HIGH]) return COMMAND_CONFLICT;
    if (candidate.values[COMMAND_TEMP_LOW] >= candidate.values[COMMAND_TEMP]) return COMMAND_CONFLICT;
    *desired = candidate;
    return COMMAND_OK;
}

const char *Command_Message(CommandStatus status)
{
    switch (status) {
        case COMMAND_OK: return "success";
        case COMMAND_UNKNOWN_KEY: return "unsupported property";
        case COMMAND_BAD_TYPE: return "integer value required";
        case COMMAND_OUT_OF_RANGE: return "value out of range";
        case COMMAND_DUPLICATE: return "duplicate property";
        case COMMAND_CONFLICT: return "threshold pair conflict";
        case COMMAND_INTERNAL: return "command apply failed";
        default: return "invalid request";
    }
}

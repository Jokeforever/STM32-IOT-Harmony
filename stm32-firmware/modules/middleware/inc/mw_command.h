/* 可移植命令契约：只依赖标准C，不包含HAL、JSON或网络。 */
#ifndef MW_COMMAND_H
#define MW_COMMAND_H
#include <stdint.h>

#define COMMAND_CAPACITY 7U
typedef enum {
    COMMAND_LED, COMMAND_PUMP, COMMAND_CO2, COMMAND_SOIL_LOW,
    COMMAND_SOIL_HIGH, COMMAND_TEMP, COMMAND_TEMP_LOW, COMMAND_KEY_COUNT
} CommandKey;
typedef struct { CommandKey key; int32_t value; } CommandEntry;
typedef struct { int32_t values[COMMAND_KEY_COUNT]; } CommandState;
typedef enum {
    COMMAND_OK = 0, COMMAND_BAD_REQUEST, COMMAND_UNKNOWN_KEY, COMMAND_BAD_TYPE,
    COMMAND_OUT_OF_RANGE, COMMAND_DUPLICATE, COMMAND_CONFLICT, COMMAND_INTERNAL
} CommandStatus;

CommandKey Command_FindKey(const char *name); /* 严格区分大小写 */
CommandStatus Command_Plan(const CommandEntry *entries, uint8_t count,
                           const CommandState *current, CommandState *desired);
const char *Command_Message(CommandStatus status);
#endif

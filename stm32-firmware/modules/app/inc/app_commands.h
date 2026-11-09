#ifndef APP_COMMANDS_H
#define APP_COMMANDS_H
#include "mw_command.h"
#define APP_COMMAND_REPORT_GAP_MS 1000U
typedef struct { CommandStatus status; uint8_t changed; } AppCommandResult;
/* 只由主循环调用。无效批次在修改前整批拒绝，不保证实体硬件事务或EEPROM立即落盘。 */
AppCommandResult AppCommands_Execute(const CommandEntry *entries, uint8_t count);
uint8_t AppCommands_ReportPending(void);
uint8_t AppCommands_ReportDue(uint32_t now, uint32_t last_attempt, uint8_t connected);
void AppCommands_ReportSent(void); /* 仅成功发送后清理待上报标记 */
#endif

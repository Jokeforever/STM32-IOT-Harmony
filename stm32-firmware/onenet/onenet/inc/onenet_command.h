#ifndef ONENET_COMMAND_H
#define ONENET_COMMAND_H
#include <stddef.h>
#include <stdint.h>
/* 合法消息ID才生成回复；返回0时不执行命令。payload必须是完整、已终止的JSON。 */
uint8_t OneNetCommand_Handle(const char *payload, char *reply, size_t capacity);
#endif

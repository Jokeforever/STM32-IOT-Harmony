/* OneJSON适配：只将协议字段转换成可移植命令，网络发布仍由onenet.c负责。 */
#include "onenet_command.h"
#include "app_commands.h"
#include "cJSON.h"
#include <limits.h>
#include <string.h>

static uint8_t SafeEnvelope(const char *s)
{
    unsigned length = 0U, depth = 0U;
    uint8_t quoted = 0U, escaped = 0U;
    if (s == NULL) return 0U;
    for (; *s != '\0'; ++s) {
        if (++length >= 256U) return 0U;
        if (quoted) {
            if (escaped) escaped = 0U;
            else if (*s == '\\') {
                if (strncmp(s, "\\u0000", 6U) == 0) return 0U;
                escaped = 1U;
            }
            else if (*s == '"') quoted = 0U;
        } else if (*s == '"') quoted = 1U;
        else if (*s == '{' || *s == '[') { if (++depth > 4U) return 0U; }
        else if (*s == '}' || *s == ']') { if (depth == 0U) return 0U; --depth; }
    }
    return (uint8_t)(!quoted && depth == 0U);
}

static uint8_t ValidId(const char *id)
{
    size_t length = 0U;
    if (id == NULL || *id == '\0') return 0U;
    for (; *id != '\0'; ++id) if (++length >= 64U || *id < '0' || *id > '9') return 0U;
    return 1U;
}

static void MakeReply(char *reply, const char *id, CommandStatus status)
{
    const char *code = status == COMMAND_OK ? "200" : (status == COMMAND_INTERNAL ? "500" : "400");
    strcpy(reply, "{\"id\":\""); strcat(reply, id); strcat(reply, "\",\"code\":");
    strcat(reply, code); strcat(reply, ",\"msg\":\""); strcat(reply, Command_Message(status)); strcat(reply, "\"}");
}

uint8_t OneNetCommand_Handle(const char *payload, char *reply, size_t capacity)
{
    cJSON *root, *item, *id = NULL, *params = NULL;
    CommandEntry entries[COMMAND_CAPACITY];
    AppCommandResult result;
    CommandStatus status = COMMAND_BAD_REQUEST;
    unsigned ids = 0U, parameter_sets = 0U;
    uint8_t count = 0U;
    if (reply == NULL || capacity == 0U) return 0U;
    reply[0] = '\0';
    if (!SafeEnvelope(payload)) return 0U;
    root = cJSON_ParseWithOpts(payload, NULL, 1);
    if (root == NULL) return 0U;
    if ((root->type & 255) != cJSON_Object) goto ignored;
    for (item = root->child; item != NULL; item = item->next) {
        if (item->string != NULL && strcmp(item->string, "id") == 0) { id = item; ++ids; }
        if (item->string != NULL && strcmp(item->string, "params") == 0) { params = item; ++parameter_sets; }
    }
    if (ids != 1U || id == NULL || (id->type & 255) != cJSON_String || !ValidId(id->valuestring)) goto ignored;
    /* 先确认最坏回复容量，再执行，避免执行成功却无法构造回复。 */
    if (capacity < strlen(id->valuestring) + 80U) goto ignored;
    if (parameter_sets != 1U || params == NULL || (params->type & 255) != cJSON_Object) goto replied;
    for (item = params->child; item != NULL; item = item->next) {
        double value;
        if (count >= COMMAND_CAPACITY) goto replied;
        entries[count].key = Command_FindKey(item->string);
        if (entries[count].key == COMMAND_KEY_COUNT) { status = COMMAND_UNKNOWN_KEY; goto replied; }
        if ((item->type & 255) != cJSON_Number) { status = COMMAND_BAD_TYPE; goto replied; }
        value = item->valuedouble;
        /* 比较写在窄化前；NaN、无穷、小数均不允许。 */
        if (!(value >= INT_MIN && value <= INT_MAX) || value != (double)(int32_t)value) { status = COMMAND_BAD_TYPE; goto replied; }
        entries[count++].value = (int32_t)value;
    }
    result = AppCommands_Execute(entries, count);
    status = result.status;
replied:
    MakeReply(reply, id->valuestring, status);
    cJSON_Delete(root);
    return 1U;
ignored:
    cJSON_Delete(root);
    return 0U;
}

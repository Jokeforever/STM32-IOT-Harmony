#ifndef _ESP8266_H_
#define _ESP8266_H_





#define REV_OK		0	//接收完成标志
#define REV_WAIT	1	//接收未完成标志


#include <stdint.h>

#define ESP8266_ONENET_INFO "AT+CIPSTART=\"TCP\",\"mqtts.heclouds.com\",1883\r\n"

typedef enum {
    ESP8266_START_WAIT, ESP8266_START_OK, ESP8266_START_FAIL
} Esp8266StartupResult;

typedef enum {
    ESP8266_START_AT, ESP8266_START_MODE, ESP8266_START_DHCP,
    ESP8266_START_JOIN, ESP8266_START_TCP, ESP8266_START_CLOSE
} Esp8266StartupCommand;

/* 冷启动期间独占接收缓冲。发送 MQTT 包前，调用者保留数据直至轮询结束。 */
uint8_t ESP8266_StartupBeginCommand(Esp8266StartupCommand command, uint32_t now_ms);
Esp8266StartupResult ESP8266_StartupPollCommand(uint32_t now_ms);
uint8_t ESP8266_StartupBeginPacket(const uint8_t *payload, uint16_t length, uint32_t now_ms);
Esp8266StartupResult ESP8266_StartupPollPacket(uint32_t now_ms, uint8_t *reply, uint8_t reply_len);
void ESP8266_StartupEnd(uint8_t preserve_rx);
void ESP8266_RxByte(uint8_t byte);
void ESP8266_RxError(void);

void ESP8266_Clear(void);

_Bool ESP8266_SendCmd(char *cmd, char *res);

_Bool ESP8266_SendData(unsigned char *data, unsigned short len);

unsigned char *ESP8266_GetIPD(unsigned short timeOut);


#endif

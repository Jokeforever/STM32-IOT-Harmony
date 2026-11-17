#ifndef _ONENET_H_
#define _ONENET_H_





_Bool OneNET_RegisterDevice(void);

#include "esp8266.h"

_Bool OneNet_DevLink(void);
uint8_t OneNet_StartupBegin(uint8_t subscribe, uint32_t now_ms);
Esp8266StartupResult OneNet_StartupPoll(uint32_t now_ms);

_Bool OneNet_IsConnected(void);

_Bool OneNet_SendData(void);

void OneNET_Subscribe(void);

void OneNet_RevPro(unsigned char *cmd);

#endif

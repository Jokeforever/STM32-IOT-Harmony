/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	esp8266.c
	*
	*	作者： 		张继瑞
	*
	*	日期： 		2017-05-08
	*
	*	版本： 		V1.0
	*
	*	说明： 		ESP8266的简单驱动
	*
	*	修改记录：	
	************************************************************
	************************************************************
	************************************************************
**/

//单片机头文件
#include "stm32f1xx_hal.h"

//网络设备驱动
#include "esp8266.h"

//硬件驱动

#include "usart.h"
#include "myusart.h"

//C库
#include <string.h>
#include <stdio.h>

extern void Watchdog_Feed(void);


#define ESP8266_WIFI_INFO		"AT+CWJAP=\"YOUR_WIFI_SSID\",\"YOUR_WIFI_PASSWORD\"\r\n"


extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
static uint8_t esp8266_buf[512];
static volatile uint16_t esp8266_cnt;
static uint16_t esp8266_cntPre;
static volatile uint8_t startup_rx_capture;
static volatile uint8_t startup_rx_overflow;

void ESP8266_RxByte(uint8_t byte)
{
    if (esp8266_cnt >= sizeof(esp8266_buf) - 1U) {
        if (startup_rx_capture) {
            startup_rx_overflow = 1U;
            return;
        }
        esp8266_cnt = 0U;
    }
    esp8266_buf[esp8266_cnt++] = byte;
    esp8266_buf[esp8266_cnt] = 0U;
}

void ESP8266_RxError(void)
{
    if (startup_rx_capture) startup_rx_overflow = 1U;
}


//==========================================================
//	函数名称：	ESP8266_Clear
//
//	函数功能：	清空缓存
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void ESP8266_Clear(void)
{

	const uint32_t mask = __get_PRIMASK();
	__disable_irq();
	memset(esp8266_buf, 0, sizeof(esp8266_buf));
	esp8266_cnt = 0;
	esp8266_cntPre = 0;
	__set_PRIMASK(mask);

}

//==========================================================
//	函数名称：	ESP8266_WaitRecive
//
//	函数功能：	等待接收完成
//
//	入口参数：	无
//
//	返回参数：	REV_OK-接收完成		REV_WAIT-接收超时未完成
//
//	说明：		循环调用检测是否接收完成
//==========================================================
_Bool ESP8266_WaitRecive(void)
{

	if(esp8266_cnt == 0) 							//如果接收计数为0 则说明没有处于接收数据中，所以直接跳出，结束函数
		return REV_WAIT;
		
	if(esp8266_cnt == esp8266_cntPre)				//如果上一次的值和这次相同，则说明接收完毕
	{
		esp8266_cnt = 0;							//清0接收计数
			

			
		esp8266_cntPre = 0;

		return REV_OK;								//返回接收完成标志
	}
		
	esp8266_cntPre = esp8266_cnt;					//置为相同
	
	return REV_WAIT;								//返回接收未完成标志

}

//==========================================================
//	函数名称：	ESP8266_SendCmd
//
//	函数功能：	发送命令
//
//	入口参数：	cmd：命令
//				res：需要检查的返回指令
//
//	返回参数：	0-成功	1-失败
//
//	说明：		
//==========================================================
_Bool ESP8266_SendCmd(char *cmd, char *res)
{
	
	unsigned short timeOut = 200;

	if(strstr(cmd, "CWJAP") != NULL)
	{
		timeOut = 3000;
	}

	ESP8266_Clear();
	Usart_SendString(&huart2, (unsigned char *)cmd, strlen((const char *)cmd));
	
	while(timeOut--)
	{
		if(ESP8266_WaitRecive() == REV_OK)
		{
			if(strstr((const char *)esp8266_buf, res) != NULL)
			{
				ESP8266_Clear();
				return 0;
			}

			if(strstr((const char *)esp8266_buf, "FAIL") != NULL ||
			   strstr((const char *)esp8266_buf, "ERROR") != NULL)
			{
				ESP8266_Clear();
				return 1;
			}
		}
		
		Watchdog_Feed();
		HAL_Delay(10);
	}
	
	return 1;

}



//_Bool ESP8266_SendCmd(char *cmd, char *res)
//{
//    unsigned short timeOut = 300; // 增至约3s，适配CWJAP慢场景
//    uint32_t recvStartTime = HAL_GetTick();

//    memset(esp8266_buf, 0, sizeof(esp8266_buf)); // 清旧数据
//    Usart_SendString(&huart2, (uint8_t*)cmd, strlen(cmd));

//    while (timeOut--) {
//        if (ESP8266_WaitRecive() == REV_OK) { 
//            // ★ 关键补丁：等待完整响应（模块空闲100ms无新数据算收完）
//            uint32_t idleStart = HAL_GetTick();
//            while (HAL_GetTick() - idleStart < 50 && !ESP8266_WaitRecive()) ; 

//            // ★ 打印原始响应（调试用）
////            printf("[RAW] %s\n", esp8266_buf);

//            if (strstr((char*)esp8266_buf, res) != NULL) {
//                ESP8266_Clear();
//                return 0;
//            }
//            
//            // ★ 额外识别常见错误（避免死循环）
//            if (strstr((char*)esp8266_buf, "FAIL") || strstr((char*)esp8266_buf, "ERROR")) {
////                printf("CMD FAILED: %s\n", cmd);
//                break;
//            }

//            memset(esp8266_buf, 0, sizeof(esp8266_buf)); // 清空准备下次接收
//        } else {
//            // 无数据时每隔20ms轮询一次（节省CPU）
//            HAL_Delay(20); 
//        }
//        
//        if (HAL_GetTick() - recvStartTime > 6000) { // 总超时保护
////            printf("TIMEOUT for cmd: %s\n", cmd);
//            break;
//        }
//    }
//    
//    return 1;
//}

//==========================================================
//	函数名称：	ESP8266_SendData
//
//	函数功能：	发送数据
//
//	入口参数：	data：数据
//				len：长度
//
//	返回参数：	无
//
//	说明：		
//==========================================================
_Bool ESP8266_SendData(unsigned char *data, unsigned short len)
{
    const uint8_t started = ESP8266_StartupBeginPacket(data, len, HAL_GetTick());
    Esp8266StartupResult result = ESP8266_START_FAIL;
    if (started) {
        result = ESP8266_StartupPollPacket(HAL_GetTick(), NULL, 0U);
        while (result == ESP8266_START_WAIT) {
            Watchdog_Feed();
            HAL_Delay(5);
            result = ESP8266_StartupPollPacket(HAL_GetTick(), NULL, 0U);
        }
    }
    /* 等到 SEND OK 才能启动下一次 CIPSEND。 */
    ESP8266_StartupEnd(result == ESP8266_START_OK);
    return result != ESP8266_START_OK;
}

//==========================================================
//	函数名称：	ESP8266_GetIPD
//
//	函数功能：	获取平台返回的数据
//
//	入口参数：	等待的时间(乘以10ms)
//
//	返回参数：	平台返回的原始数据
//
//	说明：		不同网络设备返回的格式不同，需要去调试
//				如ESP8266的返回格式为	"+IPD,x:yyy"	x代表数据长度，yyy是数据内容
//==========================================================
unsigned char *ESP8266_GetIPD(unsigned short timeOut)
{

	char *ptrIPD = NULL;
	
	do
	{
		if(ESP8266_WaitRecive() == REV_OK)								//如果接收完成
		{
			ptrIPD = strstr((char *)esp8266_buf, "IPD,");				//搜索“IPD”头
			if(ptrIPD == NULL)											//如果没找到，可能是IPD头的延迟，还是需要等待一会，但不会超过设定的时间
			{
				//UsartPrintf(USART_DEBUG, "\"IPD\" not found\r\n");
			}
			else
			{
				ptrIPD = strchr(ptrIPD, ':');							//找到':'
				if(ptrIPD != NULL)
				{
					ptrIPD++;
					return (unsigned char *)(ptrIPD);
				}
				else
					return NULL;
				
			}
		}
		
		Watchdog_Feed();
		HAL_Delay(5);													//延时等待
	} while(timeOut--);
	
	return NULL;														//超时还未找到，返回空指针

}

#define STARTUP_AT_TIMEOUT_MS    2000U
#define STARTUP_JOIN_TIMEOUT_MS 30000U
#define STARTUP_ACK_TIMEOUT_MS   2000U
#define STARTUP_TX_TIMEOUT_MS      50U
#define STARTUP_QUIET_MS           10U

typedef enum { BOOT_AT, BOOT_WAIT_PROMPT, BOOT_WAIT_PACKET } StartupRxPhase;

static StartupRxPhase startup_rx_phase;
static Esp8266StartupCommand startup_command;
static uint32_t startup_started_at;
static uint32_t startup_rx_changed_at;
static uint16_t startup_rx_observed;
static const uint8_t *startup_payload;
static uint16_t startup_payload_len;

static void Startup_ResetCapture(uint32_t now_ms)
{
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    startup_rx_capture = 1U;
    startup_rx_overflow = 0U;
    ESP8266_Clear();
    __set_PRIMASK(mask);
    startup_started_at = now_ms;
    startup_rx_changed_at = now_ms;
    startup_rx_observed = 0U;
}

void ESP8266_StartupEnd(uint8_t preserve_rx)
{
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if (!preserve_rx || startup_rx_overflow) {
        ESP8266_Clear();
    }
    startup_rx_capture = 0U;
    startup_rx_overflow = 0U;
    __set_PRIMASK(mask);
    startup_payload = NULL;
}

static uint8_t Startup_Send(const uint8_t *bytes, uint16_t length)
{
    return HAL_UART_Transmit(&huart2, bytes, length, STARTUP_TX_TIMEOUT_MS) == HAL_OK;
}

/* 冷启动期间 ISR 只在尾部追加。读取一次长度后，已有前缀不会被覆盖。 */
static uint16_t Startup_LastLineEnd(const uint8_t *data, uint16_t length, const char *text)
{
    const size_t n = strlen(text);
    uint16_t match = 0U;
    for (uint16_t i = 0U, line = 0U; i < length; ++i) {
        if (data[i] != '\n') continue;
        const uint16_t end = (i > line && data[i - 1U] == '\r') ? i - 1U : i;
        if ((size_t)(end - line) == n && memcmp(data + line, text, n) == 0) {
            match = i + 1U;
        }
        line = i + 1U;
    }
    return match;
}

static uint8_t Startup_HasError(uint16_t length)
{
    return startup_rx_overflow ||
           Startup_LastLineEnd(esp8266_buf, length, "ERROR") ||
           Startup_LastLineEnd(esp8266_buf, length, "FAIL") ||
           Startup_LastLineEnd(esp8266_buf, length, "CONNECT FAIL") ||
           Startup_LastLineEnd(esp8266_buf, length, "SEND FAIL") ||
           Startup_LastLineEnd(esp8266_buf, length, "busy p...") ||
           Startup_LastLineEnd(esp8266_buf, length, "busy s...");
}

uint8_t ESP8266_StartupBeginCommand(Esp8266StartupCommand command, uint32_t now_ms)
{
    static const char *const commands[] = {
        [ESP8266_START_AT] = "AT\r\n",
        [ESP8266_START_MODE] = "AT+CWMODE=1\r\n",
        [ESP8266_START_DHCP] = "AT+CWDHCP=1,1\r\n",
        [ESP8266_START_JOIN] = ESP8266_WIFI_INFO,
        [ESP8266_START_TCP] = ESP8266_ONENET_INFO,
        [ESP8266_START_CLOSE] = "AT+CIPCLOSE\r\n"
    };
    if ((unsigned)command >= sizeof(commands) / sizeof(commands[0])) return 0U;
    Startup_ResetCapture(now_ms);
    startup_rx_phase = BOOT_AT;
    startup_command = command;
    const char *const text = commands[command];
    return Startup_Send((const uint8_t *)text, (uint16_t)strlen(text));
}

Esp8266StartupResult ESP8266_StartupPollCommand(uint32_t now_ms)
{
    const uint16_t length = esp8266_cnt;
    if (length != startup_rx_observed) {
        startup_rx_observed = length;
        startup_rx_changed_at = now_ms;
    }
    const uint8_t quiet = now_ms - startup_rx_changed_at >= STARTUP_QUIET_MS;
    const uint8_t ok = Startup_LastLineEnd(esp8266_buf, length, "OK");
    if (!startup_rx_overflow) {
        if (startup_command == ESP8266_START_TCP) {
            const uint16_t connected = Startup_LastLineEnd(esp8266_buf, length, "CONNECT");
            const uint16_t closed = Startup_LastLineEnd(esp8266_buf, length, "CLOSED");
            const uint16_t already = Startup_LastLineEnd(esp8266_buf, length, "ALREADY CONNECTED");
            const uint16_t already_typo = Startup_LastLineEnd(esp8266_buf, length, "ALREADY CONNECTTED");
            if ((connected > closed && (ok || quiet)) ||
                ((already > closed || already_typo > closed) && quiet)) {
                return ESP8266_START_OK;
            }
            /* 部分固件在已连接时随后返回 ERROR，等待响应尾部再判定。 */
            if ((already > closed || already_typo > closed) && !quiet) return ESP8266_START_WAIT;
        } else if (startup_command == ESP8266_START_JOIN) {
            const uint16_t got_ip = Startup_LastLineEnd(esp8266_buf, length, "WIFI GOT IP");
            const uint16_t legacy_got_ip = Startup_LastLineEnd(esp8266_buf, length, "GOT IP");
            const uint16_t disconnected = Startup_LastLineEnd(esp8266_buf, length, "WIFI DISCONNECT");
            const uint16_t final_ok = Startup_LastLineEnd(esp8266_buf, length, "OK");
            if (final_ok > disconnected ||
                ((got_ip > disconnected || legacy_got_ip > disconnected) && quiet)) {
                return ESP8266_START_OK;
            }
        } else if (ok || (startup_command == ESP8266_START_CLOSE && quiet &&
                          Startup_LastLineEnd(esp8266_buf, length, "CLOSED"))) {
            return ESP8266_START_OK;
        }
    }
    const uint32_t timeout = startup_command == ESP8266_START_JOIN
        ? STARTUP_JOIN_TIMEOUT_MS : STARTUP_AT_TIMEOUT_MS;
    if (Startup_HasError(length) || now_ms - startup_started_at >= timeout) {
        return ESP8266_START_FAIL;
    }
    return ESP8266_START_WAIT;
}

uint8_t ESP8266_StartupBeginPacket(const uint8_t *payload, uint16_t length, uint32_t now_ms)
{
    char command[32];
    const int n = snprintf(command, sizeof(command), "AT+CIPSEND=%u\r\n", (unsigned)length);
    if (payload == NULL || length == 0U || n <= 0 || (size_t)n >= sizeof(command)) return 0U;
    Startup_ResetCapture(now_ms);
    startup_rx_phase = BOOT_WAIT_PROMPT;
    startup_payload = payload;
    startup_payload_len = length;
    return Startup_Send((const uint8_t *)command, (uint16_t)n);
}

/* 单连接 +IPD,<字节数>:<数据>；接受拆在多条 +IPD 中的 MQTT 确认。 */
static uint8_t Startup_CopyReply(uint16_t length, uint8_t *reply, uint8_t need)
{
    uint8_t copied = 0U;
    for (uint16_t i = 0U; i + 5U < length;) {
        if (memcmp(esp8266_buf + i, "+IPD,", 5U) != 0) {
            ++i;
            continue;
        }
        uint16_t cursor = i + 5U;
        unsigned size = 0U;
        while (cursor < length && esp8266_buf[cursor] >= '0' && esp8266_buf[cursor] <= '9') {
            size = 10U * size + (esp8266_buf[cursor++] - '0');
            if (size > sizeof(esp8266_buf)) return 0U;
        }
        if (cursor == length) return 0U;
        if (esp8266_buf[cursor] != ':' || cursor == i + 5U) {
            ++i;
            continue;
        }
        ++cursor;
        if (size > (unsigned)(length - cursor)) return 0U;
        const unsigned take = size < (unsigned)(need - copied) ? size : (unsigned)(need - copied);
        memcpy(reply + copied, esp8266_buf + cursor, take);
        copied += (uint8_t)take;
        if (copied == need) return 1U;
        i = (uint16_t)(cursor + size);
    }
    return 0U;
}

Esp8266StartupResult ESP8266_StartupPollPacket(uint32_t now_ms, uint8_t *reply, uint8_t reply_len)
{
    const uint16_t length = esp8266_cnt;
    if (Startup_HasError(length) ||
        Startup_LastLineEnd(esp8266_buf, length, "CLOSED") ||
        Startup_LastLineEnd(esp8266_buf, length, "WIFI DISCONNECT")) {
        return ESP8266_START_FAIL;
    }
    if (startup_rx_phase == BOOT_WAIT_PROMPT) {
        if (memchr(esp8266_buf, '>', length) != NULL) {
            Startup_ResetCapture(now_ms);
            startup_rx_phase = BOOT_WAIT_PACKET;
            if (!Startup_Send(startup_payload, startup_payload_len)) return ESP8266_START_FAIL;
            startup_payload = NULL;
        } else if (now_ms - startup_started_at >= STARTUP_AT_TIMEOUT_MS) {
            return ESP8266_START_FAIL;
        }
    } else if (startup_rx_phase == BOOT_WAIT_PACKET) {
        if (reply_len != 0U ? Startup_CopyReply(length, reply, reply_len) :
                             Startup_LastLineEnd(esp8266_buf, length, "SEND OK")) {
            return ESP8266_START_OK;
        }
        if (now_ms - startup_started_at >= STARTUP_ACK_TIMEOUT_MS) return ESP8266_START_FAIL;
    } else {
        return ESP8266_START_FAIL;
    }
    return ESP8266_START_WAIT;
}

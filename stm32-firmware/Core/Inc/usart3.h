#ifndef __USART3_H
#define __USART3_H

#include "stm32f1xx_hal.h"                 // Device header
#include "oled.h"
#include "usart.h"


#include <stdint.h>

/* 用户可配置宏 */
#define USART3_RX_PACKET_SIZE 6
#define CO2_THREHOLD 2000 

/* 全局变量声明 */
extern uint8_t RxState;
extern uint8_t pRxPacket;
extern uint8_t rx_byte;  // 单字节接收缓冲区
extern uint8_t Usart3_RxPacket[USART3_RX_PACKET_SIZE];
extern volatile uint8_t Usart3_RxFlag;

/* 函数声明 */
void USART3_Rx_Start_IT(void);  // 启动中断接收


uint8_t CO2GetData(uint16_t *data);

#endif



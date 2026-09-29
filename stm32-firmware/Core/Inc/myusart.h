#ifndef _MYUSART_H_
#define _MYUSART_H_


#include "stm32f1xx_hal.h"


#define USART_DEBUG		&huart1		//调试打印所使用的串口组
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern volatile uint8_t received_data;


void Usart1_Init(unsigned int baud);

void Usart2_Init(unsigned int baud);

//void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str, unsigned short len);
void Usart_SendString(UART_HandleTypeDef *huart, uint8_t *str, uint16_t len);

//void UsartPrintf(USART_TypeDef *USARTx, char *fmt,...);
void UsartPrintf(UART_HandleTypeDef *huart, char *fmt,...);

void UART_Command_Handler(char* command);
void UART1_RX_Handler(uint8_t data);

#endif

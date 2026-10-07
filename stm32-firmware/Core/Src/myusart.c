/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	usart.c
	*
	*	作者： 		张继瑞
	*
	*	日期： 		2016-11-23
	*
	*	版本： 		V1.0
	*
	*	说明： 		单片机串口外设初始化，格式化打印
	*
	*	修改记录：	
	************************************************************
	************************************************************
	************************************************************
**/

//硬件驱动
#include "myusart.h"
#include "usart.h"
#include "usart3.h"


//C库
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
//#include "Key.h"

//#define MAX_CMD_LENGTH 50
//static char uart1_rx_buffer[MAX_CMD_LENGTH];
//static uint16_t uart1_rx_index = 0;

extern uint8_t esp8266_buf[512];     // 你的接收缓冲区
extern unsigned short esp8266_cnt;
extern uint8_t rx_buffer[1];
volatile uint8_t received_data;

/*
************************************************************
*	函数名称：	Usart1_Init
*
*	函数功能：	串口1初始化
*
*	入口参数：	baud：设定的波特率
*
*	返回参数：	无
*
*	说明：		TX-PA9		RX-PA10
************************************************************
*/
//void Usart1_Init(unsigned int baud)
//{

//	GPIO_InitTypeDef gpio_initstruct;
//	USART_InitTypeDef usart_initstruct;
//	NVIC_InitTypeDef nvic_initstruct;
//	
//	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
//	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
//	
//	//PA9	TXD
//	gpio_initstruct.GPIO_Mode = GPIO_Mode_AF_PP;
//	gpio_initstruct.GPIO_Pin = GPIO_Pin_9;
//	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
//	GPIO_Init(GPIOA, &gpio_initstruct);
//	
//	//PA10	RXD
//	gpio_initstruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
//	gpio_initstruct.GPIO_Pin = GPIO_Pin_10;
//	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
//	GPIO_Init(GPIOA, &gpio_initstruct);
//	
//	usart_initstruct.USART_BaudRate = baud;
//	usart_initstruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;		//无硬件流控
//	usart_initstruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;						//接收和发送
//	usart_initstruct.USART_Parity = USART_Parity_No;									//无校验
//	usart_initstruct.USART_StopBits = USART_StopBits_1;								//1位停止位
//	usart_initstruct.USART_WordLength = USART_WordLength_8b;							//8位数据位
//	USART_Init(USART1, &usart_initstruct);
//	
//	USART_Cmd(USART1, ENABLE);														//使能串口
//	
//	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);									//使能接收中断
//	
//	nvic_initstruct.NVIC_IRQChannel = USART1_IRQn;
//	nvic_initstruct.NVIC_IRQChannelCmd = ENABLE;
//	nvic_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
//	nvic_initstruct.NVIC_IRQChannelSubPriority = 2;
//	NVIC_Init(&nvic_initstruct);

//}

/*
************************************************************
*	函数名称：	Usart2_Init
*
*	函数功能：	串口2初始化
*
*	入口参数：	baud：设定的波特率
*
*	返回参数：	无
*
*	说明：		TX-PA2		RX-PA3
************************************************************
*/
//void Usart2_Init(unsigned int baud)
//{

//	GPIO_InitTypeDef gpio_initstruct;
//	USART_InitTypeDef usart_initstruct;
//	NVIC_InitTypeDef nvic_initstruct;
//	
//	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
//	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
//	
//	//PA2	TXD
//	gpio_initstruct.GPIO_Mode = GPIO_Mode_AF_PP;
//	gpio_initstruct.GPIO_Pin = GPIO_Pin_2;
//	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
//	GPIO_Init(GPIOA, &gpio_initstruct);
//	
//	//PA3	RXD
//	gpio_initstruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
//	gpio_initstruct.GPIO_Pin = GPIO_Pin_3;
//	gpio_initstruct.GPIO_Speed = GPIO_Speed_50MHz;
//	GPIO_Init(GPIOA, &gpio_initstruct);
//	
//	usart_initstruct.USART_BaudRate = baud;
//	usart_initstruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;		//无硬件流控
//	usart_initstruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;						//接收和发送
//	usart_initstruct.USART_Parity = USART_Parity_No;									//无校验
//	usart_initstruct.USART_StopBits = USART_StopBits_1;								//1位停止位
//	usart_initstruct.USART_WordLength = USART_WordLength_8b;							//8位数据位
//	USART_Init(USART2, &usart_initstruct);
//	
//	USART_Cmd(USART2, ENABLE);														//使能串口
//	
//	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);									//使能接收中断
//	
//	nvic_initstruct.NVIC_IRQChannel = USART2_IRQn;
//	nvic_initstruct.NVIC_IRQChannelCmd = ENABLE;
//	nvic_initstruct.NVIC_IRQChannelPreemptionPriority = 0;
//	nvic_initstruct.NVIC_IRQChannelSubPriority = 0;
//	NVIC_Init(&nvic_initstruct);

//}

/*
************************************************************
*	函数名称：	Usart_SendString
*
*	函数功能：	串口数据发送
*
*	入口参数：	USARTx：串口组
*				str：要发送的数据
*				len：数据长度
*
*	返回参数：	无
*
*	说明：		
************************************************************
*/
//void Usart_SendString(USART_TypeDef *USARTx, unsigned char *str, unsigned short len)
//{

//	unsigned short count = 0;
//	
//	for(; count < len; count++)
//	{
//		HAL_UART_Transmit(USARTx, *str++);									//发送数据
//		while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);		//等待发送完成
//	}

//}
void Usart_SendString(UART_HandleTypeDef *huart, uint8_t *str, uint16_t len)
{
    uint16_t count = 0;

    for (; count < len; count++)
    {
        HAL_UART_Transmit(huart, &str[count], 1, HAL_MAX_DELAY);  // 发送单个字节
//        while (__HAL_UART_GET_FLAG(huart, UART_FLAG_TC) == RESET); // 等待发送完成
    }
}



/*
************************************************************
*	函数名称：	UsartPrintf
*
*	函数功能：	格式化打印
*
*	入口参数：	USARTx：串口组
*				fmt：不定长参
*
*	返回参数：	无
*
*	说明：		
************************************************************
*/
//void UsartPrintf(USART_TypeDef *USARTx, char *fmt,...)
//{

//	unsigned char UsartPrintfBuf[296];
//	va_list ap;
//	unsigned char *pStr = UsartPrintfBuf;
//	
//	va_start(ap, fmt);
//	vsnprintf((char *)UsartPrintfBuf, sizeof(UsartPrintfBuf), fmt, ap);							//格式化
//	va_end(ap);
//	
//	while(*pStr != 0)
//	{
//		
//		USART_SendData(USARTx, *pStr++);
//		while(USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);
//	}

//}

void UsartPrintf(UART_HandleTypeDef *huart, char *fmt,...)
{

	unsigned char UsartPrintfBuf[296];
	va_list ap;
	uint16_t len = 0;

	va_start(ap, fmt);
	vsnprintf((char *)UsartPrintfBuf, sizeof(UsartPrintfBuf), fmt, ap);
	va_end(ap);

	len = (uint16_t)strlen((char *)UsartPrintfBuf);
	if(len > 0)
	{
		HAL_UART_Transmit(huart, UsartPrintfBuf, len, HAL_MAX_DELAY);
	}

}/*
************************************************************
*	函数名称：	USART1_IRQHandler
*
*	函数功能：	串口1收发中断
*
*	入口参数：	无
*
*	返回参数：	无
*
*	说明：		
************************************************************
*/
//void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
//{

//	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) //接收中断
//	{
//		
//		USART_ClearFlag(USART1, USART_FLAG_RXNE);
//	}

//}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart == &huart1)
    {
        HAL_UART_Receive_IT(&huart1, rx_buffer, 1);
    }
		else	if(huart == &huart2)
    {
//        uint8_t received_data = huart->pRxBuffPtr[0]; // huart->pRxBuffPtr 指向启动接收时传入的缓冲区

        // 2. 应用你原有的防止溢出和存储数据的逻辑
        if (esp8266_cnt >= sizeof(esp8266_buf))
        {
            esp8266_cnt = 0; // 防止串口被刷爆 (回绕)
        }
        esp8266_buf[esp8266_cnt++] = received_data; // 存储接收到的数据

        // ****** 核心功能结束 (HAL 版本) ******

        // 5. 重新启动接收，准备接收下一个字节
        // 注意：这里的缓冲区地址和大小应该是你最初调用 HAL_UART_Receive_IT 时使用的
        // 如果你传入的是 esp8266_buf 并且大小是 1，这里也一样
        HAL_UART_Receive_IT(&huart2, (uint8_t *)&received_data, 1); // huart->pRxBuffPtr 确保就是你之前传入的缓冲区
    }
		 else if(huart->Instance == USART3)
    {
        /* 状态机处理逻辑（与原始代码完全一致） */
        switch(RxState)
        {
            case 0:  // 寻找起始字节0x2C
                if(rx_byte == 0x2C)  // 逗号
                {
                    Usart3_RxPacket[pRxPacket] = rx_byte;  // 保存数据
                    pRxPacket++;
                    RxState = 1;  // 进入状态1
                }
                else
                {
                    pRxPacket = 0;
                    RxState = 0;  // 保持状态0
                }
                break;
                
            case 1:  // 接收数据包主体
                Usart3_RxPacket[pRxPacket] = rx_byte;  // 保存数据
                pRxPacket++;
                
                if(pRxPacket >= 6)  // 接收到6个字节
                {
                    pRxPacket = 0;
                    RxState = 2;  // 进入校验状态
                    
                    /* 校验逻辑（与原始代码完全一致） */
                    if((Usart3_RxPacket[3] == 0x03U) &&
                       (Usart3_RxPacket[4] == 0xFFU) &&
                       (Usart3_RxPacket[5] == (uint8_t)(Usart3_RxPacket[0] + Usart3_RxPacket[1]
                                                         + Usart3_RxPacket[2] + Usart3_RxPacket[3]
                                                         + Usart3_RxPacket[4])))
                    {
                        // 校验成功
                        RxState = 0;
                        pRxPacket = 0;
                        Usart3_RxFlag = 1;  // 置位标志位
                    }
                    else
                    {
                        // 校验失败
                        pRxPacket = 0;
                        RxState = 0;
                    }
                }
                break;
        }
        
        /* 重新启动中断接收（只在状态0和状态1时） */
        if(RxState == 0 || RxState == 1)
        {
            HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
        }
    }
}

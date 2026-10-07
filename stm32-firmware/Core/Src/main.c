/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
//网络协议层
#include "onenet.h"

//网络设备
#include "esp8266.h"

//硬件驱动
#include "myusart.h"
#include "dht11.h"
#include "TS.h"
#include "usart3.h"
#include "LDR.h"
#include "LED.h"
#include "beep.h"
#include "oled.h"
#include "Display.h"
#include "Key.h"
#include "delay.h"
#include "bump.h"

//C库
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */


/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ESP8266_ONENET_INFO		"AT+CIPSTART=\"TCP\",\"mqtts.heclouds.com\",1883\r\n"
#define ONENET_UPLOAD_INTERVAL	5000	// 5s
#define USE_WATCHDOG			1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
Key Keys[4];
extern System_Mode current_mode ;
extern System_Mode last_mode ;
extern Bump_Mode CurrentBump_mode;
extern Bump_Mode LastBump_mode ;
uint8_t rx_buffer[1];
uint8_t temp = 20,humi = 40;
uint16_t light = 200,moist= 20,ppm=350;
float water_vol = 0.0f;
extern uint8_t esp8266_buf[512];
volatile Abnormal abnormal[3] = {NONE};
volatile DisplayScreen CurrentScreen = DISPLAY_NONE; // 当前显示界面

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Hardware_Init(void);
void Watchdog_Init(void);
void Watchdog_Feed(void);


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_TIM4_Init();
  MX_TIM1_Init();
  MX_USART3_UART_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
	uint32_t last_upload_tick = 0;
	uint32_t last_reconnect_tick = 0;
	unsigned char *dataPtr = NULL;
	uint8_t net_retry = 0;
	uint8_t login_retry = 0;
	Hardware_Init();
	HAL_UART_Receive_IT(&huart1, rx_buffer, 1);
	HAL_UART_Receive_IT(&huart2, (uint8_t *)&received_data, 1);
	USART3_Rx_Start_IT();
	HAL_TIM_Base_Start_IT(&htim1);
	HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_2);
	ESP8266_Init();	
	OLED_Clear();
	OLED_ShowString(0,0,(uint8_t*)" Connect MQTTs Server...",16);
	net_retry = 0;
	while(ESP8266_SendCmd(ESP8266_ONENET_INFO, "CONNECT") && net_retry++ < 5)
		delay_ms(500);
	OLED_ShowString(0,4,(uint8_t*)" Connect MQTT Server Success...",16);
		delay_ms(500);
	OLED_Clear();
	OLED_ShowString(0,3,(uint8_t*)" Device login...",16);
	//等待onenet平台连接
	login_retry = 0;
	while(OneNet_DevLink() && login_retry++ < 5)	
	{
		ESP8266_SendCmd(ESP8266_ONENET_INFO, "CONNECT");
		delay_ms(500);
	}		//接入OneNET
		delay_ms(500);
	
	OneNET_Subscribe();
	Watchdog_Init();
	OLED_Clear();
	CurrentScreen = DISPLAY_NONE;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{
		Watchdog_Feed();

		/* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		
		Key_Func();

		if(!OneNet_IsConnected() &&
		   (HAL_GetTick() - last_reconnect_tick >= 10000))
		{
			last_reconnect_tick = HAL_GetTick();
			if(ESP8266_SendCmd(ESP8266_ONENET_INFO, "CONNECT") == 0)
			{
				if(OneNet_DevLink() == 0)
				{
					OneNET_Subscribe();
				}
			}
		}

		uint32_t now_tick = HAL_GetTick();
		if((now_tick - last_upload_tick) >= ONENET_UPLOAD_INTERVAL)	//发送间隔5s
		{
			last_upload_tick = now_tick;
			TS_GetData(&moist);
			CO2GetData(&ppm);
			DHT11_Read_Data(&temp,&humi);
			LDR_LuxData(&light);
			OneNet_SendData();									//发送数据
		
			ESP8266_Clear();
		}
				
				dataPtr = ESP8266_GetIPD(0);
				if(dataPtr != NULL)
				OneNet_RevPro(dataPtr);
				
				Bump_Control();
				Monitor_Temp();
				LED_Func();
				IS_Normal();
				Display_Data();

				delay_ms(10);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void Hardware_Init(void)
{
//	Beep_Init();
//	BUMP_Init();
//	LED_Init();
//  JDQ_Init();
//	FAN_Init();
	OLED_Init();
	uint8_t dht_retry = 0;
	while(DHT11_Init() && dht_retry < 3)
	{
		OLED_ShowString(0,0,"DHT11 Error",16);
		delay_ms(1000);
		dht_retry++;
	}
	if(dht_retry >= 3)
	{
		OLED_ShowString(0,2,"DHT11 FAIL",16);
		delay_ms(500);
	}
	
		OLED_Clear();
		OLED_ShowString(0,0,"Hardware init OK",16);
		delay_ms(1000);
		OLED_Clear();
	
}

static volatile uint8_t watchdog_enabled = 0;

void Watchdog_Init(void)
{
#if USE_WATCHDOG
	IWDG->KR = 0xCCCC;
	IWDG->KR = 0x5555;
	IWDG->PR = 6;      // LSI / 256
	IWDG->RLR = 1250;  // about 8s at 40kHz LSI
	while (IWDG->SR) {}
	IWDG->KR = 0xAAAA;
	watchdog_enabled = 1;
#endif
}

void Watchdog_Feed(void)
{
#if USE_WATCHDOG
	if(watchdog_enabled)
	{
		IWDG->KR = 0xAAAA;
	}
#endif
}




/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

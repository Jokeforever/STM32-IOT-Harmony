/*
 * MODULE OVERVIEW
 * 文件：main.c
 * 作用：系统主流程、外设初始化、5 秒上报、模式控制、配置加载和掉电保存。
 * 关键流程：
 *   1. HAL/RCC/GPIO/ADC/I2C/USART/TIM 初始化
 *   2. Hardware_Init() 初始化显示、传感器、LED、水泵和 EEPROM 参数
 *   3. Config_Load() 读取 24C02 参数并迁移旧版本
 *   4. 主循环读取传感器、上报 OneNET、处理按键和执行器
 * 注意：
 *   delay_ms() 依赖 TIM4 1 MHz 时基，delay_us 使用 DWT。
 *   sensor_status 只反映可可靠判断的 DHT11 和 CO2 状态。
 */
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

//中间件层
#include "mw_config.h"

//硬件驱动
#include "myusart.h"
#include "dht11.h"
#include "TS.h"
#include "usart3.h"
#include "LDR.h"
#include "LED.h"
#include "beep.h"
#include "jdq.h"
#include "oled.h"
#include "Display.h"
#include "Key.h"
#include "delay.h"
#include "bump.h"

//C库
#include <string.h>
#include <stddef.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */


/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ESP8266_ONENET_INFO		"AT+CIPSTART=\"TCP\",\"mqtts.heclouds.com\",1883\r\n"
#define ONENET_UPLOAD_INTERVAL	5000	// 5s
#define USE_WATCHDOG			1

#define SENSOR_VALID_TEMP	(1U << 0)
#define SENSOR_VALID_HUMI	(1U << 1)
#define SENSOR_VALID_CO2		(1U << 3)
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
extern uint8_t temp_threhold;
extern uint16_t Bump_threhold[2];
extern uint16_t LDR_threhold[2];
extern uint16_t CO2_Threhold;
extern uint8_t LED_Manu_PWM;
uint8_t rx_buffer[1];
uint8_t sensor_valid = 0U;
static uint8_t s_temp_filter_history[3] = {0};
static uint8_t s_humi_filter_history[3] = {0};
static uint8_t s_temp_filter_count = 0U;
static uint8_t s_humi_filter_count = 0U;
static uint16_t s_co2_filter_history[3] = {0};
static uint8_t s_co2_filter_count = 0U;
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

static uint8_t Filter_Median3_U8(uint8_t history[3], uint8_t *count, uint8_t value)
{
    uint8_t a;
    uint8_t b;
    uint8_t c;

    if (*count < 3U)
    {
        history[*count] = value;
        (*count)++;
        return value;
    }

    history[0] = history[1];
    history[1] = history[2];
    history[2] = value;

    a = history[0];
    b = history[1];
    c = history[2];

    if (((a >= b) && (a <= c)) || ((a <= b) && (a >= c)))
    {
        return a;
    }
    if (((b >= a) && (b <= c)) || ((b <= a) && (b >= c)))
    {
        return b;
    }
    return c;
}

static uint16_t Filter_Median3_U16(uint16_t history[3], uint8_t *count, uint16_t value)
{
    uint16_t a;
    uint16_t b;
    uint16_t c;

    if (*count < 3U)
    {
        history[*count] = value;
        (*count)++;
        return value;
    }

    history[0] = history[1];
    history[1] = history[2];
    history[2] = value;

    a = history[0];
    b = history[1];
    c = history[2];

    if (((a >= b) && (a <= c)) || ((a <= b) && (a >= c)))
    {
        return a;
    }
    if (((b >= a) && (b <= c)) || ((b <= a) && (b >= c)))
    {
        return b;
    }
    return c;
}

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
	uint32_t last_led_control_tick = 0;
	unsigned char *dataPtr = NULL;
	uint8_t net_retry = 0;
	uint8_t login_retry = 0;
	Hardware_Init();
	HAL_UART_Receive_IT(&huart1, rx_buffer, 1);
	HAL_UART_Receive_IT(&huart2, (uint8_t *)&received_data, 1);
	USART3_Rx_Start_IT();
	Config_Load();
	HAL_TIM_Base_Start_IT(&htim1);
	HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_2);
	Mode_Switch();
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
		Config_Process();

		/* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		
		Key_Func();

		if ((current_mode == MODE_AUTO_PID) &&
		    ((HAL_GetTick() - last_led_control_tick) >= 1000U))
		{
			last_led_control_tick = HAL_GetTick();

			if (LDR_LuxData(&light) != 0U)
			{
				LED_Update(light);
			}
		}

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
            (void)TS_GetData(&moist);

            uint16_t raw_ppm = 0U;
            if (CO2GetData(&raw_ppm) != 0U)
            {
                ppm = Filter_Median3_U16(s_co2_filter_history, &s_co2_filter_count, raw_ppm);
                sensor_valid |= SENSOR_VALID_CO2;
            }
            else
            {
                sensor_valid &= (uint8_t)~SENSOR_VALID_CO2;
            }

            uint8_t raw_temp = 0U;
            uint8_t raw_humi = 0U;
            if (DHT11_Read_Data(&raw_temp,&raw_humi) == DHT11_OK)
            {
                temp = Filter_Median3_U8(s_temp_filter_history, &s_temp_filter_count, raw_temp);
                humi = Filter_Median3_U8(s_humi_filter_history, &s_humi_filter_count, raw_humi);
                sensor_valid |= (SENSOR_VALID_TEMP | SENSOR_VALID_HUMI);
            }
            else
            {
                sensor_valid &= (uint8_t)~(SENSOR_VALID_TEMP | SENSOR_VALID_HUMI);
            }

            (void)LDR_LuxData(&light);
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

/* ============================================================
 * 配置管理模块已抽离到 modules/middleware/src/mw_config.c
 * 原 414-821 行的 12 个 Config_* 函数和 DeviceConfig 结构
 * 现由 mw_config.h 提供接口，逻辑未做任何改动。
 * ============================================================ */


void Hardware_Init(void)
{
	delay_init();
	delay_us_init();
	Beep_Init();
	BUMP_Init();
	LED_Init();
	JDQ_Init();
	FAN_Init();
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

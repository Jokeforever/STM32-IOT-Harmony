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
#define SENSOR_VALID_LIGHT	(1U << 2)
#define SENSOR_VALID_CO2		(1U << 3)
#define SENSOR_VALID_MOIST	(1U << 4)
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
            if (TS_GetData(&moist) != 0U)
            {
                sensor_valid |= SENSOR_VALID_MOIST;
            }
            else
            {
                sensor_valid &= (uint8_t)~SENSOR_VALID_MOIST;
            }

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

            if (LDR_LuxData(&light) != 0U)
            {
                sensor_valid |= SENSOR_VALID_LIGHT;
            }
            else
            {
                sensor_valid &= (uint8_t)~SENSOR_VALID_LIGHT;
            }
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

static DeviceConfig g_device_config;
static uint16_t g_config_sequence = 0U;
static uint8_t g_config_active_slot = 0U;
static volatile uint8_t g_config_dirty = 0U;
static uint32_t g_config_dirty_tick = 0U;

static uint32_t Config_CRC32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;

    for (uint32_t i = 0U; i < len; i++)
    {
        crc ^= (uint32_t)data[i];
        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1) ^ 0xEDB88320U;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFFU;
}

static uint8_t Config_IsValid(const DeviceConfig *config)
{
    if (config == NULL)
    {
        return 0U;
    }
    if (config->magic != CONFIG_MAGIC)
    {
        return 0U;
    }
    if (config->version != CONFIG_VERSION)
    {
        return 0U;
    }
    if (config->size != (uint16_t)sizeof(DeviceConfig))
    {
        return 0U;
    }

    return (Config_CRC32((const uint8_t *)config,
                         (uint32_t)(sizeof(DeviceConfig) - sizeof(uint32_t))) == config->crc32) ? 1U : 0U;
}

static void Config_SetDefault(void)
{
    memset(&g_device_config, 0, sizeof(g_device_config));
    g_device_config.magic = CONFIG_MAGIC;
    g_device_config.version = CONFIG_VERSION;
    g_device_config.size = (uint16_t)sizeof(DeviceConfig);
    g_device_config.sequence = 0U;
    g_device_config.led_mode = (uint8_t)MODE_AUTO_PID;
    g_device_config.bump_mode = (uint8_t)Bump_OFF;
    g_device_config.led_manual_pwm = 50U;
    g_device_config.temp_threshold = (uint8_t)TEMP_THREHOLD;
    g_device_config.co2_threshold = CO2_THREHOLD;
    g_device_config.ldr_low = LDR_THREHOLD_LOW;
    g_device_config.ldr_high = LDR_THREHOLD_HIGH;
    g_device_config.soil_low = HUMI_THRESHOLD_LOW;
    g_device_config.soil_high = HUMI_THRESHOLD_HIGH;
    g_device_config.soil_wet_adc = 1241U;
    g_device_config.soil_dry_adc = 4095U;
    g_device_config.crc32 = Config_CRC32((const uint8_t *)&g_device_config,
                                         (uint32_t)(sizeof(DeviceConfig) - sizeof(uint32_t)));
}

static void Config_Sanitize(DeviceConfig *config)
{
    if (config->led_mode > (uint8_t)MODE_AUTO_PID)
    {
        config->led_mode = (uint8_t)MODE_AUTO_PID;
    }
    if (config->bump_mode > (uint8_t)Bump_AUTO)
    {
        config->bump_mode = (uint8_t)Bump_OFF;
    }
    if (config->led_manual_pwm > 100U)
    {
        config->led_manual_pwm = 100U;
    }
    if (config->temp_threshold > 100U)
    {
        config->temp_threshold = 100U;
    }
    if ((config->co2_threshold < 350U) || (config->co2_threshold > 4000U))
    {
        config->co2_threshold = CO2_THREHOLD;
    }
    if ((config->ldr_low > config->ldr_high) || (config->ldr_high > 9999U))
    {
        config->ldr_low = LDR_THREHOLD_LOW;
        config->ldr_high = LDR_THREHOLD_HIGH;
    }
    if ((config->soil_low > config->soil_high) || (config->soil_high > 100U))
    {
        config->soil_low = HUMI_THRESHOLD_LOW;
        config->soil_high = HUMI_THRESHOLD_HIGH;
    }
    if ((config->soil_wet_adc < 1U) || (config->soil_wet_adc >= config->soil_dry_adc))
    {
        config->soil_wet_adc = 1241U;
        config->soil_dry_adc = 4095U;
    }
}

static void Config_Apply(void)
{
    current_mode = (System_Mode)g_device_config.led_mode;
    last_mode = current_mode;
    CurrentBump_mode = (Bump_Mode)g_device_config.bump_mode;
    LastBump_mode = CurrentBump_mode;
    LED_Manu_PWM = g_device_config.led_manual_pwm;
    temp_threhold = g_device_config.temp_threshold;
    CO2_Threhold = g_device_config.co2_threshold;
    LDR_threhold[0] = g_device_config.ldr_low;
    LDR_threhold[1] = g_device_config.ldr_high;
    Bump_threhold[0] = g_device_config.soil_low;
    Bump_threhold[1] = g_device_config.soil_high;
}

static void Config_FromCurrent(DeviceConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->magic = CONFIG_MAGIC;
    config->version = CONFIG_VERSION;
    config->size = (uint16_t)sizeof(DeviceConfig);
    config->sequence = g_config_sequence + 1U;
    if (config->sequence == 0U)
    {
        config->sequence = 1U;
    }
    config->led_mode = (uint8_t)current_mode;
    config->bump_mode = (uint8_t)CurrentBump_mode;
    config->led_manual_pwm = LED_Manu_PWM;
    config->temp_threshold = temp_threhold;
    config->co2_threshold = CO2_Threhold;
    config->ldr_low = LDR_threhold[0];
    config->ldr_high = LDR_threhold[1];
    config->soil_low = Bump_threhold[0];
    config->soil_high = Bump_threhold[1];
    config->soil_wet_adc = 1241U;
    config->soil_dry_adc = 4095U;
    config->crc32 = Config_CRC32((const uint8_t *)config,
                                  (uint32_t)(sizeof(DeviceConfig) - sizeof(uint32_t)));
}

void Config_Load(void)
{
    DeviceConfig slot_a;
    DeviceConfig slot_b;
    uint8_t a_ok;
    uint8_t b_ok;

    a_ok = EEPROM24C02_Read(CONFIG_SLOT_A, (uint8_t *)&slot_a, (uint16_t)sizeof(slot_a));
    if (a_ok != 0U)
    {
        a_ok = Config_IsValid(&slot_a);
    }

    b_ok = EEPROM24C02_Read(CONFIG_SLOT_B, (uint8_t *)&slot_b, (uint16_t)sizeof(slot_b));
    if (b_ok != 0U)
    {
        b_ok = Config_IsValid(&slot_b);
    }

    if ((a_ok != 0U) && (b_ok != 0U))
    {
        if (slot_b.sequence > slot_a.sequence)
        {
            g_device_config = slot_b;
            g_config_active_slot = 1U;
        }
        else
        {
            g_device_config = slot_a;
            g_config_active_slot = 0U;
        }
    }
    else if (a_ok != 0U)
    {
        g_device_config = slot_a;
        g_config_active_slot = 0U;
    }
    else if (b_ok != 0U)
    {
        g_device_config = slot_b;
        g_config_active_slot = 1U;
    }
    else
    {
        Config_SetDefault();
        g_config_active_slot = 0U;
        g_config_dirty = 1U;
        g_config_dirty_tick = HAL_GetTick();
    }

    g_config_sequence = g_device_config.sequence;
    Config_Sanitize(&g_device_config);
    Config_Apply();
}

uint8_t Config_Save(void)
{
    DeviceConfig config;
    DeviceConfig verify;
    uint8_t slot = (g_config_active_slot == 0U) ? CONFIG_SLOT_B : CONFIG_SLOT_A;

    Config_FromCurrent(&config);

    if (EEPROM24C02_Write(slot, (const uint8_t *)&config, (uint16_t)sizeof(config)) == 0U)
    {
        return 0U;
    }
    if (EEPROM24C02_Read(slot, (uint8_t *)&verify, (uint16_t)sizeof(verify)) == 0U)
    {
        return 0U;
    }
    if (Config_IsValid(&verify) == 0U)
    {
        return 0U;
    }

    g_device_config = verify;
    g_config_sequence = verify.sequence;
    g_config_active_slot = (slot == CONFIG_SLOT_A) ? 0U : 1U;
    return 1U;
}

void Config_MarkDirty(void)
{
    g_config_dirty = 1U;
    g_config_dirty_tick = HAL_GetTick();
}

void Config_Process(void)
{
    if (g_config_dirty == 0U)
    {
        return;
    }
    if ((HAL_GetTick() - g_config_dirty_tick) < 1000U)
    {
        return;
    }

    if (Config_Save() != 0U)
    {
        g_config_dirty = 0U;
    }
    else
    {
        g_config_dirty_tick = HAL_GetTick();
    }
}

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

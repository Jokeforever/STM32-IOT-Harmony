/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

#define CONFIG_MAGIC       0x43464731U
#define CONFIG_VERSION     1U
#define CONFIG_SLOT_A      0x00U
#define CONFIG_SLOT_B      0x40U

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint16_t sequence;
    uint8_t led_mode;
    uint8_t bump_mode;
    uint8_t led_manual_pwm;
    uint8_t temp_threshold;
    uint16_t co2_threshold;
    uint16_t ldr_low;
    uint16_t ldr_high;
    uint16_t soil_low;
    uint16_t soil_high;
    uint16_t soil_wet_adc;
    uint16_t soil_dry_adc;
    uint32_t crc32;
} DeviceConfig;

void Config_Load(void);
uint8_t Config_Save(void);
void Config_MarkDirty(void);
void Config_Process(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

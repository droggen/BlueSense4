/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
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
#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal.h"
#include "stm32l4xx_ll_tim.h"
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_cortex.h"
#include "stm32l4xx_ll_system.h"
#include "stm32l4xx_ll_utils.h"
#include "stm32l4xx_ll_pwr.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_dma.h"

#include "stm32l4xx_ll_exti.h"

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

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define MOTION_INT_Pin GPIO_PIN_13
#define MOTION_INT_GPIO_Port GPIOC
#define X_0_ADC0_Pin GPIO_PIN_0
#define X_0_ADC0_GPIO_Port GPIOC
#define X_1_ADC1_Pin GPIO_PIN_1
#define X_1_ADC1_GPIO_Port GPIOC
#define X_AUD_CLK_Pin GPIO_PIN_2
#define X_AUD_CLK_GPIO_Port GPIOC
#define X_2_ADC2_Pin GPIO_PIN_3
#define X_2_ADC2_GPIO_Port GPIOC
#define X_4_ADC_DAC_Pin GPIO_PIN_4
#define X_4_ADC_DAC_GPIO_Port GPIOA
#define X_3_ADC3_Pin GPIO_PIN_4
#define X_3_ADC3_GPIO_Port GPIOC
#define PWR_PBSTAT_Pin GPIO_PIN_5
#define PWR_PBSTAT_GPIO_Port GPIOC
#define SPI1_NSS_Pin GPIO_PIN_0
#define SPI1_NSS_GPIO_Port GPIOB
#define LED_2_Pin GPIO_PIN_1
#define LED_2_GPIO_Port GPIOB
#define LED_1_Pin GPIO_PIN_2
#define LED_1_GPIO_Port GPIOB
#define LED_0_Pin GPIO_PIN_11
#define LED_0_GPIO_Port GPIOB
#define X_SAIA2_FS_NSS_Pin GPIO_PIN_12
#define X_SAIA2_FS_NSS_GPIO_Port GPIOB
#define X_SAIA2_SCK_Pin GPIO_PIN_13
#define X_SAIA2_SCK_GPIO_Port GPIOB
#define X_SAIA2_MCLK_MISO_Pin GPIO_PIN_14
#define X_SAIA2_MCLK_MISO_GPIO_Port GPIOB
#define X_SAIA2_SD_MOSI_Pin GPIO_PIN_15
#define X_SAIA2_SD_MOSI_GPIO_Port GPIOB
#define X_5_Pin GPIO_PIN_6
#define X_5_GPIO_Port GPIOC
#define X_AUD_DAT_Pin GPIO_PIN_7
#define X_AUD_DAT_GPIO_Port GPIOC
#define SD_DETECT_Pin GPIO_PIN_8
#define SD_DETECT_GPIO_Port GPIOA
#define USB_SENSE_Pin GPIO_PIN_9
#define USB_SENSE_GPIO_Port GPIOA
#define PWR_MOTION_EN_Pin GPIO_PIN_10
#define PWR_MOTION_EN_GPIO_Port GPIOA
#define MOTION_INTb_Pin GPIO_PIN_15
#define MOTION_INTb_GPIO_Port GPIOA
#define BLUE_Connect_Pin GPIO_PIN_4
#define BLUE_Connect_GPIO_Port GPIOB
#define BLUE_RST__Pin GPIO_PIN_5
#define BLUE_RST__GPIO_Port GPIOB
#define AUD_DAT_Pin GPIO_PIN_8
#define AUD_DAT_GPIO_Port GPIOB
#define PERIPH_EN_Pin GPIO_PIN_9
#define PERIPH_EN_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

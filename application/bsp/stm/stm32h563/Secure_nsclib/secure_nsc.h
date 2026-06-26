/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Secure_nsclib/secure_nsc.h
  * @author  MCD Application Team
  * @brief   Header for secure non-secure callable APIs list
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

/* USER CODE BEGIN Non_Secure_CallLib_h */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SECURE_NSC_H
#define SECURE_NSC_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported types ------------------------------------------------------------*/
/**
  * @brief  non-secure callback ID enumeration definition
  */
typedef enum
{
  SECURE_FAULT_CB_ID     = 0x00U, /*!< System secure fault callback ID */
  GTZC_ERROR_CB_ID       = 0x01U  /*!< GTZC secure error callback ID */
} SECURE_CallbackIDTypeDef;

/**
  * @brief  Shared RTC Time and Date structure
  */
typedef struct
{
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint8_t date;
  uint8_t month;
  uint8_t year;
  uint8_t weekday;
  uint32_t subseconds;
  uint32_t second_fraction;
} App_RTC_TimeTypeDef;

/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void SECURE_RegisterCallback(SECURE_CallbackIDTypeDef CallbackId, void *func);
uint32_t SECURE_RTC_GetTimeDate(App_RTC_TimeTypeDef *pTimeDate);
uint32_t SECURE_RTC_SetTimeDate(const App_RTC_TimeTypeDef *pTimeDate);

#endif /* SECURE_NSC_H */
/* USER CODE END Non_Secure_CallLib_h */


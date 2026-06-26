/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    Secure/Src/secure_nsc.c
  * @author  MCD Application Team
  * @brief   This file contains the non-secure callable APIs (secure world)
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

/* USER CODE BEGIN Non_Secure_CallLib */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "secure_nsc.h"
/** @addtogroup STM32H5xx_HAL_Examples

  * @{
  */

/** @addtogroup Templates
  * @{
  */

/* Global variables ----------------------------------------------------------*/
void *pSecureFaultCallback = NULL;   /* Pointer to secure fault callback in Non-secure */
void *pSecureErrorCallback = NULL;   /* Pointer to secure error callback in Non-secure */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Secure registration of non-secure callback.
  * @param  CallbackId  callback identifier
  * @param  func        pointer to non-secure function
  * @retval None
  */
CMSE_NS_ENTRY void SECURE_RegisterCallback(SECURE_CallbackIDTypeDef CallbackId, void *func)
{
  if(func != NULL)
  {
    switch(CallbackId)
    {
      case SECURE_FAULT_CB_ID:           /* SecureFault Interrupt occurred */
        pSecureFaultCallback = func;
        break;
      case GTZC_ERROR_CB_ID:             /* GTZC Interrupt occurred */
        pSecureErrorCallback = func;
        break;
      default:
        /* unknown */
        break;
    }
  }
}

extern RTC_HandleTypeDef hrtc;

/**
  * @brief  Get RTC Time and Date from Secure world.
  */
CMSE_NS_ENTRY uint32_t SECURE_RTC_GetTimeDate(App_RTC_TimeTypeDef *pTimeDate)
{
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  if (pTimeDate == NULL)
  {
    return 1; /* Error: Invalid argument */
  }

  /* Get Time */
  if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 1; /* Error */
  }

  /* Get Date (Unlocks shadow registers) */
  if (HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 1; /* Error */
  }

  pTimeDate->hours = sTime.Hours;
  pTimeDate->minutes = sTime.Minutes;
  pTimeDate->seconds = sTime.Seconds;
  pTimeDate->date = sDate.Date;
  pTimeDate->month = sDate.Month;
  pTimeDate->year = sDate.Year;
  pTimeDate->weekday = sDate.WeekDay;

  return 0; /* OK */
}

/**
  * @brief  Set RTC Time and Date from Secure world.
  */
CMSE_NS_ENTRY uint32_t SECURE_RTC_SetTimeDate(const App_RTC_TimeTypeDef *pTimeDate)
{
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  if (pTimeDate == NULL)
  {
    return 1; /* Error: Invalid argument */
  }
  
  sTime.Hours = pTimeDate->hours;
  sTime.Minutes = pTimeDate->minutes;
  sTime.Seconds = pTimeDate->seconds;
  sTime.TimeFormat = RTC_HOURFORMAT_24;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;

  sDate.Date = pTimeDate->date;
  sDate.Month = pTimeDate->month;
  sDate.Year = pTimeDate->year;
  sDate.WeekDay = pTimeDate->weekday;

  /* Set Time first, then Date */
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 1; /* Error */
  }

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
  {
    return 1; /* Error */
  }

  return 0; /* OK */
}

/**
  * @}
  */

/**
  * @}
  */
/* USER CODE END Non_Secure_CallLib */


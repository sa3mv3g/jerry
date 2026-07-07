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

/* USER CODE BEGIN Calibration_NSC */

/**
 * @brief  Calibration virtual address map (shared between Secure and NonSecure).
 *
 *         All values are stored as uint32_t (float reinterpreted via bit-cast).
 *         EDATA accessed via Secure alias 0x0D000000 (FLASH_EDATA_BASE_S) —
 *         stable across SWAP_BANK changes.
 *
 *         Layout: 4 ADC channels × 3 float parameters = 12 variables
 *           Channel index: 0–3
 *           Parameter:
 *             +0  scaling_factor  (float, bit-cast to uint32_t)
 *             +1  offset_term     (float, bit-cast to uint32_t)
 *             +2  deadzone        (float, bit-cast to uint32_t)
 *
 *         Virtual address = CAL_ADC_BASE_VADDR + (channel * 3) + param_offset
 *         e.g. CH0 scaling_factor = 0x0001, CH1 scaling_factor = 0x0004, etc.
 */

/* ADC calibration base — 4 channels × 3 params = 12 addresses (0x0001–0x000C) */
#define CAL_ADC_BASE_VADDR          0x0001U  /*!< Base virtual address for ADC calibration */
#define CAL_ADC_PARAMS_PER_CH       3U       /*!< scaling_factor, offset_term, deadzone */
#define CAL_ADC_NUM_CHANNELS        4U       /*!< Number of ADC channels */

/* Per-channel parameter offsets (relative to channel base) */
#define CAL_ADC_SCALING_FACTOR_OFF  0U       /*!< float: raw-to-engineering-unit scale */
#define CAL_ADC_OFFSET_TERM_OFF     1U       /*!< float: additive offset after scaling */
#define CAL_ADC_DEADZONE_OFF        2U       /*!< float: deadzone threshold */

/**
 * @brief  Compute virtual address for a given ADC channel and parameter.
 * @param  ch    Channel index (0–3)
 * @param  param Parameter offset (CAL_ADC_*_OFF)
 */
#define CAL_ADC_VADDR(ch, param) \
    ((uint16_t)(CAL_ADC_BASE_VADDR + (uint16_t)((ch) * CAL_ADC_PARAMS_PER_CH) + (uint16_t)(param)))

/* System configuration variables */
#define CAL_MODBUS_ADDR_VADDR   0x0010U  /*!< uint32_t: Modbus device address (1–247) */
#define CAL_BAUD_RATE_VADDR     0x0011U  /*!< uint32_t: Baud rate configuration */

/* FOTA control */
#define FOTA_VALID_FLAG_VADDR   0x0030U  /*!< uint32_t: FOTA firmware valid flag (0xA5A5A5A5) */

/**
 * @brief  Read a calibration variable from EEPROM emulation (Bank 1 EDATA).
 * @param  vaddr   Virtual address (see CAL_*_VADDR defines above)
 * @param  pValue  Output: value read from EEPROM emulation
 * @retval 0 = success (EE_OK), non-zero = error code
 */
uint32_t SECURE_CAL_Read(uint16_t vaddr, uint32_t *pValue);

/**
 * @brief  Write a calibration variable to EEPROM emulation (Bank 1 EDATA).
 * @param  vaddr   Virtual address (see CAL_*_VADDR defines above)
 * @param  value   Value to write
 * @retval 0 = success (EE_OK), non-zero = error code
 */
uint32_t SECURE_CAL_Write(uint16_t vaddr, uint32_t value);

/* USER CODE END Calibration_NSC */

#endif /* SECURE_NSC_H */
/* USER CODE END Non_Secure_CallLib_h */


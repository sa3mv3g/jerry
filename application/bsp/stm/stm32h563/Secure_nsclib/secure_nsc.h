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
    SECURE_FAULT_CB_ID = 0x00U, /*!< System secure fault callback ID */
    GTZC_ERROR_CB_ID   = 0x01U  /*!< GTZC secure error callback ID */
} SECURE_CallbackIDTypeDef;

/**
 * @brief  Shared RTC Time and Date structure
 */
typedef struct
{
    uint8_t  hours;
    uint8_t  minutes;
    uint8_t  seconds;
    uint8_t  date;
    uint8_t  month;
    uint8_t  year;
    uint8_t  weekday;
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
 * @brief  Calibration virtual address map (shared between Secure and
 * NonSecure).
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

/* ADC calibration base — 4 channels × 3 params = 12 addresses (0x0001–0x000C)
 */
#define CAL_ADC_BASE_VADDR \
    0x0001U /*!< Base virtual address for ADC calibration */
#define CAL_ADC_PARAMS_PER_CH 3U /*!< scaling_factor, offset_term, deadzone */
#define CAL_ADC_NUM_CHANNELS  4U /*!< Number of ADC channels */

/* Per-channel parameter offsets (relative to channel base) */
#define CAL_ADC_SCALING_FACTOR_OFF \
    0U /*!< float: raw-to-engineering-unit scale */
#define CAL_ADC_OFFSET_TERM_OFF                                           \
    1U                          /*!< float: additive offset after scaling \
                                 */
#define CAL_ADC_DEADZONE_OFF 2U /*!< float: deadzone threshold */

/**
 * @brief  Compute virtual address for a given ADC channel and parameter.
 * @param  ch    Channel index (0–3)
 * @param  param Parameter offset (CAL_ADC_*_OFF)
 */
#define CAL_ADC_VADDR(ch, param)     \
    ((uint16_t)(CAL_ADC_BASE_VADDR + \
                (uint16_t)((ch) * CAL_ADC_PARAMS_PER_CH) + (uint16_t)(param)))

/* System configuration variables */
#define CAL_MODBUS_ADDR_VADDR \
    0x0010U /*!< uint32_t: Modbus device address (1–247) */
#define CAL_BAUD_RATE_VADDR 0x0011U /*!< uint32_t: Baud rate configuration */

/* FOTA control */
#define FOTA_VALID_FLAG_VADDR \
    0x0030U /*!< uint32_t: FOTA firmware valid flag (0xA5A5A5A5) */

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

/* USER CODE BEGIN FOTA_NSC */

/**
 * @defgroup FOTA_NSC  FOTA Non-Secure Callable Interface
 * @brief    Secure-world flash operations for firmware-over-the-air update.
 *
 * Firmware package format (written to inactive bank via WriteChunk):
 *
 *   Bytes [0 .. fw_size-1]          Raw firmware binary (16-byte aligned)
 *   Bytes [fw_size .. total-9]      X.509 certificate DER
 *                                     Custom extension OID 1.3.6.1.4.1.99999.1
 *                                     contains SHA-256(firmware) as OCTET
 * STRING Bytes [total-8 .. total-5]      cert_size (uint32_t LE) Bytes [total-4
 * .. total-1]      magic = 0x464F5441 ("FOTA")
 *
 * Typical usage:
 *   1. SECURE_FOTA_EraseTarget()              — erase inactive bank
 *   2. SECURE_FOTA_WriteChunk(0, buf, len)    — write first chunk
 *   3. SECURE_FOTA_WriteChunk(len, buf2, len) — write next chunk
 *   ...
 *   4. SECURE_FOTA_Commit(total_size)         — verify + swap + reset
 *
 * On startup after FOTA reset:
 *   5. SECURE_CAL_Read(FOTA_VALID_FLAG_VADDR, &flag)
 *   6. If flag != 0xA5A5A5A5 → SECURE_FOTA_Rollback()
 *   7. After self-test: SECURE_CAL_Write(FOTA_VALID_FLAG_VADDR, 0xA5A5A5A5)
 * @{
 */

/** @defgroup FOTA_ERR  FOTA error codes @{ */
#define FOTA_OK                0U /*!< Success */
#define FOTA_ERR_FLASH_UNLOCK  1U /*!< HAL_FLASH_Unlock() failed */
#define FOTA_ERR_ERASE         2U /*!< Sector erase failed */
#define FOTA_ERR_WRITE         3U /*!< Flash program failed */
#define FOTA_ERR_BAD_POINTER   4U /*!< pData not in NonSecure memory */
#define FOTA_ERR_ALIGNMENT     5U /*!< offset or len not 16-byte aligned */
#define FOTA_ERR_BAD_SIZE      6U /*!< total_size too small or cert_size invalid */
#define FOTA_ERR_BAD_MAGIC     7U  /*!< Trailer magic != 0x464F5441 */
#define FOTA_ERR_CERT_PARSE    8U  /*!< wolfSSL failed to parse firmware cert */
#define FOTA_ERR_CA_PARSE      9U  /*!< wolfSSL failed to parse CA cert */
#define FOTA_ERR_CERT_VERIFY   10U /*!< Firmware cert signature invalid */
#define FOTA_ERR_NO_HASH       11U /*!< SHA-256 hash extension not found in cert */
#define FOTA_ERR_HASH_COMPUTE  12U /*!< SHA-256 computation error */
#define FOTA_ERR_HASH_MISMATCH 13U /*!< Computed hash != hash in cert */
/** @} */

/**
 * @brief  Erase the inactive bank sectors 32-119 (704KB NS firmware area).
 *         Must be called once before streaming firmware chunks.
 * @retval FOTA_OK (0) on success, FOTA_ERR_* on failure
 */
uint32_t SECURE_FOTA_EraseTarget(void);

/**
 * @brief  Write a chunk of firmware data to the inactive bank.
 * @param  offset  Byte offset from NS firmware start (must be 16-byte aligned)
 * @param  pData   Pointer to data in NonSecure memory
 * @param  len     Number of bytes (must be multiple of 16)
 * @retval FOTA_OK (0) on success, FOTA_ERR_* on failure
 */
uint32_t SECURE_FOTA_WriteChunk(uint32_t offset, const uint8_t *pData,
                                uint32_t len);

/**
 * @brief  Verify the firmware package and commit by toggling SWAP_BANK.
 *         On success: triggers a system reset — does NOT return.
 *         On failure: returns FOTA_ERR_* error code.
 * @param  total_size  Total bytes written (firmware + cert + 8-byte trailer)
 * @retval FOTA_ERR_* on failure (does not return on success)
 */
uint32_t SECURE_FOTA_Commit(uint32_t total_size);

/**
 * @brief  Roll back to the previous firmware by toggling SWAP_BANK.
 *         Triggers a system reset — does NOT return.
 *         Call from NS startup if FOTA_VALID_FLAG is not set.
 */
void SECURE_FOTA_Rollback(void);

/** @} */ /* FOTA_NSC */

/* USER CODE END FOTA_NSC */

#endif /* SECURE_NSC_H */
/* USER CODE END Non_Secure_CallLib_h */

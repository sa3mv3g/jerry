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
  pTimeDate->subseconds = sTime.SubSeconds;
  pTimeDate->second_fraction = sTime.SecondFraction;

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

/* USER CODE BEGIN Calibration_NSC_Impl */

/* Include EEPROM emulation API (compiled in Secure build with EDATA_ENABLED).
 * The middleware uses FLASH_EDATA_BASE_S = 0x0D000000 (Secure alias) because
 * this is a Secure build (CMSE=3), making calibration data stable across
 * SWAP_BANK changes. */
#include "eeprom_emul.h"

/**
  * @brief  Read a calibration variable from EEPROM emulation.
  *         EEPROM emulation runs on Bank 1 EDATA via Secure alias 0x0D000000.
  *         This alias is NOT affected by SWAP_BANK — calibration survives FOTA.
  * @param  vaddr   Virtual address (see CAL_*_VADDR in secure_nsc.h)
  * @param  pValue  Output: value read
  * @retval 0 = EE_OK, non-zero = error
  */
CMSE_NS_ENTRY uint32_t SECURE_CAL_Read(uint16_t vaddr, uint32_t *pValue)
{
  if (pValue == NULL)
  {
    return 1U; /* Invalid argument */
  }

  /* Validate pointer is in NonSecure memory before writing to it */
  if (cmse_check_address_range(pValue, sizeof(uint32_t), CMSE_NONSECURE) == NULL)
  {
    return 2U; /* Pointer not in NonSecure memory */
  }

  EE_Status status = EE_ReadVariable32bits(vaddr, pValue);
  return (status == EE_OK) ? 0U : (uint32_t)status;
}

/**
  * @brief  Write a calibration variable to EEPROM emulation.
  *         EEPROM emulation runs on Bank 1 EDATA via Secure alias 0x0D000000.
  *         This alias is NOT affected by SWAP_BANK — calibration survives FOTA.
  * @param  vaddr   Virtual address (see CAL_*_VADDR in secure_nsc.h)
  * @param  value   Value to write
  * @retval 0 = EE_OK, non-zero = error
  */
CMSE_NS_ENTRY uint32_t SECURE_CAL_Write(uint16_t vaddr, uint32_t value)
{
  EE_Status status = EE_WriteVariable32bits(vaddr, value);
  return (status == EE_OK) ? 0U : (uint32_t)status;
}

/* USER CODE END Calibration_NSC_Impl */

/* USER CODE BEGIN FOTA_NSC_Impl */

/*
 * FOTA NSC functions — Secure world flash operations for firmware update.
 *
 * Firmware package format (written to inactive bank by WriteChunk):
 *
 *   [0 .. fw_size-1]          Raw firmware binary (16-byte aligned)
 *   [fw_size .. total-9]      X.509 certificate DER
 *                               Custom extension OID 1.3.6.1.4.1.99999.1
 *                               contains SHA-256(firmware) as OCTET STRING
 *   [total-8 .. total-5]      cert_size (uint32_t LE)
 *   [total-4 .. total-1]      magic = 0x464F5441 ("FOTA")
 *
 * Target bank selection (always writes to the INACTIVE bank):
 *   SWAP_BANK=0: active NS in Bank 1 → write to Bank 2 (base 0x08100000)
 *   SWAP_BANK=1: active NS in Bank 2 → write to Bank 1 (base 0x08000000)
 *   In both cases, skip sectors 0-31 (256KB Secure partition).
 *   FOTA_NS_SECTOR_OFFSET = 32, FOTA_NS_BYTE_OFFSET = 32 * 8KB = 256KB
 */

#include "fota_crypto.h"
#include <stdbool.h>
#include <string.h>

/* FOTA target: skip first 256KB (sectors 0-31 = Secure partition) */
#define FOTA_NS_SECTOR_OFFSET   32U
#define FOTA_NS_BYTE_OFFSET     (FOTA_NS_SECTOR_OFFSET * 8192U)   /* 0x40000 = 256KB */

/* Firmware package trailer */
#define FOTA_TRAILER_SIZE       8U
#define FOTA_MAGIC              0x464F5441UL   /* "FOTA" */

/* Public key buffer size for P-256 SubjectPublicKeyInfo DER (91 bytes) */
#define FOTA_PUBKEY_BUF_SIZE    128U

/* CA certificate DER — Jerry FOTA CA (ECDSA-P256, valid 2026-2036)
 * Generated by: openssl ecparam -name prime256v1 -genkey -noout -out keys/fota_ca.key
 *               openssl req -new -x509 -key keys/fota_ca.key -out keys/fota_ca.crt \
 *                   -subj "/CN=Jerry FOTA CA" -days 3650
 * To regenerate: delete keys/fota_ca.key and keys/fota_ca.crt, re-run above commands,
 *   then update this array with: openssl x509 -in keys/fota_ca.crt -outform DER | xxd -i */
static const uint8_t fota_ca_cert_der[] = {
    0x30, 0x82, 0x01, 0x85, 0x30, 0x82, 0x01, 0x2B, 0xA0, 0x03, 0x02, 0x01,
    0x02, 0x02, 0x14, 0x38, 0xB8, 0x8A, 0xC8, 0xCB, 0x51, 0x4A, 0x29, 0x63,
    0xE3, 0x53, 0xFD, 0xB5, 0x81, 0x85, 0x44, 0x9B, 0x28, 0xE2, 0xEE, 0x30,
    0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x30,
    0x18, 0x31, 0x16, 0x30, 0x14, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x0D,
    0x4A, 0x65, 0x72, 0x72, 0x79, 0x20, 0x46, 0x4F, 0x54, 0x41, 0x20, 0x43,
    0x41, 0x30, 0x1E, 0x17, 0x0D, 0x32, 0x36, 0x30, 0x37, 0x30, 0x37, 0x31,
    0x31, 0x35, 0x32, 0x32, 0x35, 0x5A, 0x17, 0x0D, 0x33, 0x36, 0x30, 0x37,
    0x30, 0x34, 0x31, 0x31, 0x35, 0x32, 0x32, 0x35, 0x5A, 0x30, 0x18, 0x31,
    0x16, 0x30, 0x14, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x0D, 0x4A, 0x65,
    0x72, 0x72, 0x79, 0x20, 0x46, 0x4F, 0x54, 0x41, 0x20, 0x43, 0x41, 0x30,
    0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
    0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42,
    0x00, 0x04, 0x77, 0x35, 0x8D, 0xB9, 0x07, 0xFD, 0x60, 0xF6, 0xC4, 0x63,
    0xBB, 0x34, 0x8D, 0x1B, 0xCC, 0xD6, 0x87, 0x8E, 0x88, 0x5C, 0xC2, 0x79,
    0x18, 0x6D, 0x74, 0x82, 0xBC, 0xC1, 0xDA, 0xCF, 0xB8, 0xBD, 0x4D, 0xFB,
    0xDA, 0x05, 0x96, 0x48, 0xE3, 0xF3, 0xA2, 0x71, 0x10, 0xCB, 0x87, 0x30,
    0xF1, 0xC1, 0xA2, 0x13, 0x5E, 0xFB, 0xF7, 0xEF, 0x1A, 0x07, 0x5B, 0xAD,
    0x08, 0xE6, 0xD5, 0x6E, 0xF2, 0xD3, 0xA3, 0x53, 0x30, 0x51, 0x30, 0x1D,
    0x06, 0x03, 0x55, 0x1D, 0x0E, 0x04, 0x16, 0x04, 0x14, 0x45, 0xDB, 0x85,
    0x47, 0x25, 0xA6, 0xE0, 0x6C, 0x5A, 0x77, 0x97, 0xA7, 0x12, 0xF0, 0x60,
    0x67, 0x69, 0x52, 0x85, 0x8D, 0x30, 0x1F, 0x06, 0x03, 0x55, 0x1D, 0x23,
    0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0x45, 0xDB, 0x85, 0x47, 0x25, 0xA6,
    0xE0, 0x6C, 0x5A, 0x77, 0x97, 0xA7, 0x12, 0xF0, 0x60, 0x67, 0x69, 0x52,
    0x85, 0x8D, 0x30, 0x0F, 0x06, 0x03, 0x55, 0x1D, 0x13, 0x01, 0x01, 0xFF,
    0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xFF, 0x30, 0x0A, 0x06, 0x08, 0x2A,
    0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x03, 0x48, 0x00, 0x30, 0x45,
    0x02, 0x20, 0x54, 0x06, 0x99, 0x61, 0x80, 0xE9, 0x9C, 0x64, 0x58, 0x97,
    0x4B, 0xED, 0x41, 0x78, 0xC1, 0x82, 0x89, 0x06, 0x99, 0x55, 0xD1, 0xF8,
    0xE3, 0x59, 0xA9, 0x42, 0xFB, 0xC0, 0x8D, 0x39, 0x26, 0x4C, 0x02, 0x21,
    0x00, 0xCB, 0xFC, 0x65, 0x53, 0xF1, 0x62, 0x23, 0x3D, 0xDE, 0xF3, 0x2F,
    0x41, 0xDF, 0xFF, 0x16, 0x04, 0xE2, 0xB1, 0xC0, 0xA2, 0xD5, 0xD4, 0xF2,
    0x54, 0xFD, 0xD1, 0x40, 0xA9, 0xDC, 0xB4, 0x3F, 0x5C
};
static const uint32_t fota_ca_cert_der_len = 393U;

/**
 * @brief  Get the base address of the inactive bank for FOTA writes.
 */
static uint32_t fota_get_target_base(void)
{
    bool swapped = (READ_BIT(FLASH->OPTSR_CUR, FLASH_OPTSR_SWAP_BANK) != 0U);
    uint32_t bank_base = swapped ? 0x08000000UL : 0x08100000UL;
    return bank_base + FOTA_NS_BYTE_OFFSET;
}

/**
 * @brief  Get the bank selector for the inactive bank.
 */
static uint32_t fota_get_target_bank(void)
{
    bool swapped = (READ_BIT(FLASH->OPTSR_CUR, FLASH_OPTSR_SWAP_BANK) != 0U);
    return swapped ? FLASH_BANK_1 : FLASH_BANK_2;
}

/**
 * @brief  Erase the inactive bank sectors 32-119 (704KB NS firmware area).
 */
CMSE_NS_ENTRY uint32_t SECURE_FOTA_EraseTarget(void)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks     = fota_get_target_bank();
    erase.Sector    = FOTA_NS_SECTOR_OFFSET;
    erase.NbSectors = 120U - FOTA_NS_SECTOR_OFFSET;   /* Sectors 32-119 = 88 sectors */

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return FOTA_ERR_FLASH_UNLOCK;
    }

    HAL_StatusTypeDef ret = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();

    if (ret != HAL_OK || page_error != 0xFFFFFFFFU)
    {
        return FOTA_ERR_ERASE;
    }
    return FOTA_OK;
}

/**
 * @brief  Write a chunk of firmware data to the inactive bank.
 */
CMSE_NS_ENTRY uint32_t SECURE_FOTA_WriteChunk(uint32_t offset,
                                               const uint8_t *pData,
                                               uint32_t len)
{
    if (cmse_check_address_range((void *)pData, len, CMSE_NONSECURE) == NULL)
    {
        return FOTA_ERR_BAD_POINTER;
    }
    if ((offset % 16U) != 0U || (len % 16U) != 0U || len == 0U)
    {
        return FOTA_ERR_ALIGNMENT;
    }

    uint32_t base = fota_get_target_base();

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return FOTA_ERR_FLASH_UNLOCK;
    }

    HAL_StatusTypeDef ret = HAL_OK;
    for (uint32_t i = 0U; i < len; i += 16U)
    {
        ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD,
                                base + offset + i,
                                (uint32_t)(pData + i));
        if (ret != HAL_OK) { break; }
    }

    HAL_FLASH_Lock();
    return (ret == HAL_OK) ? FOTA_OK : FOTA_ERR_WRITE;
}

/**
 * @brief  Verify the firmware package and commit by toggling SWAP_BANK.
 *         Does NOT return on success — device resets.
 *         Returns FOTA_ERR_* on failure.
 */
CMSE_NS_ENTRY uint32_t SECURE_FOTA_Commit(uint32_t total_size)
{
    if (total_size < (FOTA_TRAILER_SIZE + 1U))
    {
        return FOTA_ERR_BAD_SIZE;
    }

    uint32_t base = fota_get_target_base();

    /* Step 1: Read and validate trailer */
    const uint8_t *trailer = (const uint8_t *)(base + total_size - FOTA_TRAILER_SIZE);
    uint32_t cert_size = ((uint32_t)trailer[0])
                       | ((uint32_t)trailer[1] << 8U)
                       | ((uint32_t)trailer[2] << 16U)
                       | ((uint32_t)trailer[3] << 24U);
    uint32_t magic = ((uint32_t)trailer[4])
                   | ((uint32_t)trailer[5] << 8U)
                   | ((uint32_t)trailer[6] << 16U)
                   | ((uint32_t)trailer[7] << 24U);

    if (magic != FOTA_MAGIC)                                  { return FOTA_ERR_BAD_MAGIC; }
    if (cert_size == 0U || cert_size > (total_size - FOTA_TRAILER_SIZE)) { return FOTA_ERR_BAD_SIZE; }

    uint32_t fw_size  = total_size - FOTA_TRAILER_SIZE - cert_size;
    const uint8_t *cert_ptr = (const uint8_t *)(base + fw_size);

    /* Step 2: Initialize wolfSSL static pool */
    fota_crypto_init();

    /* Step 3: Verify firmware cert against CA */
    if (fota_x509_verify_cert(cert_ptr, cert_size,
                               fota_ca_cert_der, fota_ca_cert_der_len) != 0)
    {
        fota_crypto_reset();
        return FOTA_ERR_CERT_VERIFY;
    }

    /* Step 4: Extract SHA-256 hash and public key from cert */
    uint8_t  cert_hash[32]                  = {0};
    uint8_t  pub_key_buf[FOTA_PUBKEY_BUF_SIZE] = {0};
    uint32_t pub_key_len                    = 0U;

    if (fota_x509_parse(cert_ptr, cert_size,
                         cert_hash,
                         pub_key_buf, sizeof(pub_key_buf), &pub_key_len) != 0)
    {
        fota_crypto_reset();
        return FOTA_ERR_NO_HASH;
    }

    /* Step 5: Compute SHA-256 over firmware region */
    uint8_t computed_hash[32] = {0};
    if (fota_sha256((const uint8_t *)base, fw_size, computed_hash) != 0)
    {
        fota_crypto_reset();
        return FOTA_ERR_HASH_COMPUTE;
    }

    /* Step 6: Compare hashes */
    if (memcmp(computed_hash, cert_hash, 32U) != 0)
    {
        fota_crypto_reset();
        return FOTA_ERR_HASH_MISMATCH;
    }

    fota_crypto_reset();

    /* Step 7: All checks passed — toggle SWAP_BANK and reset */
    bool swapped = (READ_BIT(FLASH->OPTSR_CUR, FLASH_OPTSR_SWAP_BANK) != 0U);

    FLASH_OBProgramInitTypeDef ob = {0};
    ob.OptionType = OPTIONBYTE_USER;
    ob.USERType   = OB_USER_SWAP_BANK;
    ob.USERConfig = swapped ? OB_SWAP_BANK_DISABLE : OB_SWAP_BANK_ENABLE;

    HAL_FLASH_OB_Unlock();
    HAL_FLASHEx_OBProgram(&ob);
    HAL_FLASH_OB_Launch();  /* Triggers system reset — does not return */

    return FOTA_OK;  /* Never reached */
}

/**
 * @brief  Roll back to the previous firmware by toggling SWAP_BANK.
 *         Does NOT return — triggers a system reset.
 */
CMSE_NS_ENTRY void SECURE_FOTA_Rollback(void)
{
    bool swapped = (READ_BIT(FLASH->OPTSR_CUR, FLASH_OPTSR_SWAP_BANK) != 0U);

    FLASH_OBProgramInitTypeDef ob = {0};
    ob.OptionType = OPTIONBYTE_USER;
    ob.USERType   = OB_USER_SWAP_BANK;
    ob.USERConfig = swapped ? OB_SWAP_BANK_DISABLE : OB_SWAP_BANK_ENABLE;

    HAL_FLASH_OB_Unlock();
    HAL_FLASHEx_OBProgram(&ob);
    HAL_FLASH_OB_Launch();  /* Triggers system reset — does not return */

    while (1) {}
}

/* USER CODE END FOTA_NSC_Impl */
/* USER CODE END Non_Secure_CallLib */


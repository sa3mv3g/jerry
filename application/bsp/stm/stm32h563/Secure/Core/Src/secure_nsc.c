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
#include "secure_nsc.h"

#include "main.h"
/** @addtogroup STM32H5xx_HAL_Examples

  * @{
  */

/** @addtogroup Templates
 * @{
 */

/* Global variables ----------------------------------------------------------*/
void *pSecureFaultCallback =
    NULL; /* Pointer to secure fault callback in Non-secure */
void *pSecureErrorCallback =
    NULL; /* Pointer to secure error callback in Non-secure */

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
CMSE_NS_ENTRY void SECURE_RegisterCallback(SECURE_CallbackIDTypeDef CallbackId,
                                           void                    *func)
{
    if (func != NULL)
    {
        switch (CallbackId)
        {
            case SECURE_FAULT_CB_ID: /* SecureFault Interrupt occurred */
                pSecureFaultCallback = func;
                break;
            case GTZC_ERROR_CB_ID: /* GTZC Interrupt occurred */
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

    pTimeDate->hours           = sTime.Hours;
    pTimeDate->minutes         = sTime.Minutes;
    pTimeDate->seconds         = sTime.Seconds;
    pTimeDate->date            = sDate.Date;
    pTimeDate->month           = sDate.Month;
    pTimeDate->year            = sDate.Year;
    pTimeDate->weekday         = sDate.WeekDay;
    pTimeDate->subseconds      = sTime.SubSeconds;
    pTimeDate->second_fraction = sTime.SecondFraction;

    return 0; /* OK */
}

/**
 * @brief  Set RTC Time and Date from Secure world.
 */
CMSE_NS_ENTRY uint32_t
SECURE_RTC_SetTimeDate(const App_RTC_TimeTypeDef *pTimeDate)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    if (pTimeDate == NULL)
    {
        return 1; /* Error: Invalid argument */
    }

    sTime.Hours          = pTimeDate->hours;
    sTime.Minutes        = pTimeDate->minutes;
    sTime.Seconds        = pTimeDate->seconds;
    sTime.TimeFormat     = RTC_HOURFORMAT_24;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;

    sDate.Date    = pTimeDate->date;
    sDate.Month   = pTimeDate->month;
    sDate.Year    = pTimeDate->year;
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
    if (cmse_check_address_range(pValue, sizeof(uint32_t), CMSE_NONSECURE) ==
        NULL)
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

#include <stdbool.h>
#include <string.h>

#include "fota_crypto.h"

/* FOTA target: skip first 256KB (sectors 0-31 = Secure partition) */
#define FOTA_NS_SECTOR_OFFSET 32U
#define FOTA_NS_BYTE_OFFSET \
    (FOTA_NS_SECTOR_OFFSET * 8192U) /* 0x40000 = 256KB */

/* Firmware package trailer */
#define FOTA_TRAILER_SIZE 8U
#define FOTA_MAGIC        0x464F5441UL /* "FOTA" */

/* Public key buffer size for P-256 SubjectPublicKeyInfo DER (91 bytes) */
#define FOTA_PUBKEY_BUF_SIZE 128U

/* CA certificate DER — Jerry FOTA CA (ECDSA-P256, valid 2026-2036)
 * Generated by: openssl ecparam -name prime256v1 -genkey -noout -out
 * keys/fota_ca.key
 *               openssl req -new -x509 -key keys/fota_ca.key -out
 * keys/fota_ca.crt \ -subj "/CN=Jerry FOTA CA" -days 3650 To regenerate: delete
 * keys/fota_ca.key and keys/fota_ca.crt, re-run above commands, then update
 * this array with: openssl x509 -in keys/fota_ca.crt -outform DER | xxd -i */
/* Not static — accessible from main.c via extern for fota_verify_at_boot() */
const uint8_t fota_ca_cert_der[] = {
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
    0x54, 0xFD, 0xD1, 0x40, 0xA9, 0xDC, 0xB4, 0x3F, 0x5C};
const uint32_t fota_ca_cert_der_len = 393U;

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
    FLASH_EraseInitTypeDef erase      = {0};
    uint32_t               page_error = 0U;

    /* FIX: Use FLASH_TYPEERASE_SECTORS_NS (not FLASH_TYPEERASE_SECTORS).
     *
     * Bank 2 sectors 32-119 are Non-Secure flash. Erasing them from Secure
     * context requires FLASH_TYPEERASE_SECTORS_NS so the HAL uses NSCR
     * (Non-Secure Control Register) instead of SECCR. Using FLASH_TYPEERASE_SECTORS
     * causes IS_FLASH_SECURE_OPERATION() to return true, which routes the erase
     * through SECCR — but SECCR cannot erase NS sectors, so HAL_FLASHEx_Erase()
     * returns HAL_ERROR immediately with page_error = first sector. */
    erase.TypeErase = FLASH_TYPEERASE_SECTORS_NS;
    erase.Banks     = fota_get_target_bank();
    erase.Sector    = FOTA_NS_SECTOR_OFFSET;
    erase.NbSectors =
        120U - FOTA_NS_SECTOR_OFFSET; /* Sectors 32-119 = 88 sectors */

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
CMSE_NS_ENTRY uint32_t SECURE_FOTA_WriteChunk(uint32_t       offset,
                                              const uint8_t *pData,
                                              uint32_t       len)
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
        /* FIX: Use FLASH_TYPEPROGRAM_QUADWORD_NS — Bank 2 is NS flash.
         * Using FLASH_TYPEPROGRAM_QUADWORD routes through SECCR which
         * cannot program NS sectors. */
        ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD_NS, base + offset + i,
                                (uint32_t)(pData + i));
        if (ret != HAL_OK)
        {
            break;
        }
    }

    /* FIX: HAL_FLASH_Program() (blocking path) does NOT clear
     * pFlash.ProcedureOnGoing — only the interrupt handler does.
     * If ProcedureOnGoing has FLASH_NON_SECURE_MASK set, subsequent
     * Secure flash operations (EEPROM emulation) will incorrectly use
     * NSCR instead of SECCR, causing EE_ERROR_NOERASING_PAGE.
     * Explicitly reset it here so the next Secure operation works. */
    extern FLASH_ProcessTypeDef pFlash;
    pFlash.ProcedureOnGoing = 0U;

    HAL_FLASH_Lock();
    return (ret == HAL_OK) ? FOTA_OK : FOTA_ERR_WRITE;
}

/**
 * @brief  Stage the firmware for activation on next external POR.
 *
 * Sets FOTA_PENDING flag in EEPROM to signal the Secure firmware to verify
 * and activate the staged firmware on the next power-on reset.
 *
 * Does NOT toggle SWAP_BANK. Does NOT reset.
 * The current firmware keeps running after this call.
 *
 * On the next external POR, Secure main.c fota_boot_check():
 *   1. Reads FOTA_PENDING — if set, toggles SWAP_BANK
 *   2. Verifies the firmware at 0x08040000 (X.509 + SHA-256)
 *   3. Valid:   clears FOTA_PENDING, boots NS from new firmware
 *   4. Invalid: toggles SWAP_BANK back, clears FOTA_PENDING, boots old firmware
 *
 * @retval FOTA_OK on success, FOTA_ERR_* on failure
 */
CMSE_NS_ENTRY uint32_t SECURE_FOTA_Stage(void)
{
    /* Write FOTA_PENDING flag to EEPROM.
     * Value 0xF0F0F0F0 = "FOTA staged, verify and activate on next POR".
     * Secure main.c clears this flag after verification (pass or fail).
     *
     * FIX: SECURE_FOTA_WriteChunk() calls HAL_FLASH_Lock() after writing,
     * which locks NSCR. The EEPROM emulation uses FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS
     * which routes through NSCR. If NSCR is locked, the write fails with
     * EE_ERROR_NOACTIVE_PAGE (no active page can be written).
     * Unlock the flash before calling EE_WriteVariable32bits(). */
    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return FOTA_ERR_FLASH_UNLOCK;
    }

    EE_Status status =
        EE_WriteVariable32bits(FOTA_PENDING_VADDR, FOTA_PENDING_MAGIC);

    /* Recovery: EE_ERROR_NOERASING_PAGE means the EEPROM page state machine
     * is inconsistent (first page of a group is STATE_PAGE_ERASING but
     * subsequent pages are not). Force a full reformat and retry. */
    if ((status & EE_STATUSMASK_ERROR) == EE_ERROR_NOACTIVE_PAGE ||
        (status & EE_STATUSMASK_ERROR) == EE_ERROR_NOERASING_PAGE)
    {
        /* FIX: Reset pFlash state and clear any stale flash error flags before
         * calling EE_Format(). HAL_FLASH_Program() (blocking path) does not
         * call __HAL_UNLOCK() or clear pFlash.ProcedureOnGoing. Also clear
         * OPTCHANGEERR in NSSR which is set after option byte writes and causes
         * FLASH_WaitForLastOperation() to return HAL_ERROR immediately. */
        extern FLASH_ProcessTypeDef pFlash;
        pFlash.Lock             = HAL_UNLOCKED;
        pFlash.ProcedureOnGoing = 0U;
        pFlash.ErrorCode        = HAL_FLASH_ERROR_NONE;
        /* Clear OPTCHANGEERR and all other error flags in NSSR and SECSR */
        WRITE_REG(FLASH_NS->NSCCR, FLASH_FLAG_OPTCHANGEERR | FLASH_FLAG_SR_ERRORS);
        WRITE_REG(FLASH->SECCCR,   FLASH_FLAG_OPTCHANGEERR | FLASH_FLAG_SR_ERRORS);

        EE_Status fmt_status = EE_Format(EE_FORCED_ERASE);
        if (fmt_status != EE_OK)
        {
            /* EE_Format failed: marker=0xEE, fmt_status in bits [23:16] */
            return FOTA_ERR_WRITE | (((uint32_t)fmt_status & 0xFFU) << 16U) | (0xEEU << 24U);
        }
        /* EE_Format erased all pages but did not set up an active page.
         * Reset pFlash state again (EE_Format's FI_WriteDoubleWord calls
         * __HAL_LOCK which leaves pFlash.Lock = HAL_LOCKED in blocking path).
         * Then call EE_Init() to initialize the EEPROM state machine. */
        pFlash.Lock             = HAL_UNLOCKED;
        pFlash.ProcedureOnGoing = 0U;
        pFlash.ErrorCode        = HAL_FLASH_ERROR_NONE;
        WRITE_REG(FLASH_NS->NSCCR, FLASH_FLAG_OPTCHANGEERR | FLASH_FLAG_SR_ERRORS);
        WRITE_REG(FLASH->SECCCR,   FLASH_FLAG_OPTCHANGEERR | FLASH_FLAG_SR_ERRORS);
        EE_Status init_status = EE_Init(EE_CONDITIONAL_ERASE);
        if ((init_status & EE_STATUSMASK_ERROR) != 0U)
        {
            return FOTA_ERR_WRITE | (((uint32_t)init_status & 0xFFU) << 16U) | (0xCCU << 24U);
        }
        /* Retry the write after re-initialization */
        status = EE_WriteVariable32bits(FOTA_PENDING_VADDR, FOTA_PENDING_MAGIC);
        if ((status & EE_STATUSMASK_ERROR) != 0U)
        {
            /* Retry failed after format+init: marker=0xBB, retry status in bits [23:16] */
            return FOTA_ERR_WRITE | (((uint32_t)(uint16_t)status) << 16U) | (0xBBU << 24U);
        }
        /* Retry succeeded */
        if ((status & EE_STATUSMASK_CLEANUP) != 0U)
        {
            (void)EE_CleanUp();
        }
        return FOTA_OK;
    }

    /* Success: EE_OK or EE_CLEANUP_REQUIRED (write succeeded, cleanup needed) */
    if ((status & EE_STATUSMASK_ERROR) == 0U)
    {
        if ((status & EE_STATUSMASK_CLEANUP) != 0U)
        {
            (void)EE_CleanUp();
        }
        return FOTA_OK;
    }

    /* Failure: encode raw EE_Status in bits [31:16] for diagnosis */
    return FOTA_ERR_WRITE | (((uint32_t)(uint16_t)status) << 16U);
}

/* USER CODE END FOTA_NSC_Impl */
/* USER CODE END Non_Secure_CallLib */

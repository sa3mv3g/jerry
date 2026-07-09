/**

  ******************************************************************************
  * @file    EEPROM_Emul/Porting/STM32H5/flash_interface.c
  * @author  MCD Application Team
  * @brief   This file provides all the EEPROM emulation flash interface functions.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "eeprom_emul.h"
#include "flash_interface.h"
#include "eeprom_emul_types.h"
#include <stdint.h>
#include "stm32h5xx_hal.h"

#define RAM_FUNC  __attribute__((section(".RamFunc")))

/** @addtogroup EEPROM_Emulation
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
/* Private constants ---------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint64_t FlashWord[2] =
{
  0x0, 0x0
};
uint8_t FlashWord_status = 0; /* 0 is FlashWord is empty, 1 it is full */
 const uint32_t QuadWord[4] =
{
 0x00000000,
 0x00000000,
 0x00000000,
 0x00000000
};

/* Private function prototypes -----------------------------------------------*/
#ifdef FLASH_BANK_2
static uint32_t GetBankNumber(uint32_t Address);
#endif

/* Exported functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/** @addtogroup EEPROM_Private_Functions
  * @{
  */

/**
  * @brief  Write a quad word at the given address in Flash (function is called FI_WriteDoubleWord to respect legacy for X-CUBE-EEPROM)
  * @param  Address Where to write
  * @param  Data What to write
  * @param  Write_type Type of writing on going (see EE_Write_type)
  * @retval EE_Status
  *           - EE_OK: on success
  *           - EE_WRITE_ERROR: if an error occurs
  *           - EE_FLASH_USED: flash currently used by CPU2
  */
#ifdef EDATA_ENABLED
RAM_FUNC EE_Status FI_WriteDoubleWord(uint32_t Address, uint64_t Data)
{
  /* EDATA sectors 120-127 are Non-Secure (outside SECWM1_END=8).
   * Must use FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS (writes via FLASH->NSCR),
   * NOT FLASH_TYPEPROGRAM_HALFWORD_EDATA (writes via FLASH->SECCR).
   * Using SECCR on a Non-Secure sector triggers WRPERR.
   * [stm32h5xx_hal_flash.h: FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS uses FLASH_NON_SECURE_MASK] */
  uint16_t *HalfWordData = (uint16_t*)&Data;
  EE_Status status = EE_OK;
  if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS, Address, (uint32_t)&(HalfWordData[0])) != HAL_OK)
  {
    status = EE_WRITE_ERROR;
  }
  if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS, Address+2, (uint32_t)&(HalfWordData[1])) != HAL_OK)
  {
    status = EE_WRITE_ERROR;
  }
  if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS, Address+4, (uint32_t)&(HalfWordData[2])) != HAL_OK)
  {
    status = EE_WRITE_ERROR;
  }
  if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS, Address+6, (uint32_t)&(HalfWordData[3])) != HAL_OK)
  {
    status = EE_WRITE_ERROR;
  }

  return status;
}
#else
RAM_FUNC EE_Status FI_WriteDoubleWord(uint32_t Address, uint64_t* Data, EE_Write_type Write_type)
{
  EE_Status status = EE_OK;
  if(Write_type == EE_SET_PAGE)
  {
    FlashWord[0] = Data[0];
    FlashWord[1] = Data[0];
  }
  else
  {
    FlashWord[0] = Data[0];
    FlashWord[1] = Data[1];
  }

  if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, Address, (uint32_t)FlashWord) != HAL_OK)
  {
      status = EE_WRITE_ERROR;
  }
  return status;
}
#endif

/**
  * @brief  Erase a page in polling mode
  * @param  Page Page number
  * @param  NbPages Number of pages to erase
  * @retval EE_Status
  *           - EE_OK: on success
  *           - EE error code: if an error occurs
  */
RAM_FUNC EE_Status FI_PageErase(uint32_t Page, uint16_t NbPages)
{
  EE_Status status = EE_OK;
  FLASH_EraseInitTypeDef s_eraseinit;
  uint32_t bank = FLASH_BANK_1, page_error = 0U;

#ifdef FLASH_BANK_2
  bank = GetBankNumber(PAGE_ADDRESS(Page));
#endif

  /* EDATA sectors 120-127 are outside SECWM1_END=8, so they are Non-Secure sectors.
   * Secure code must erase Non-Secure sectors via FLASH->NSCR (FLASH_TYPEERASE_SECTORS_NS),
   * NOT via FLASH->SECCR (FLASH_TYPEERASE_SECTORS). Using SECCR on a Non-Secure sector
   * triggers WRPERR (write protection error) because the hardware enforces that SECCR
   * can only erase sectors within the Secure Watermark.
   * [RM0481 §7.6.1: Secure watermark protects sectors 0-SECWM1_END from NS access;
   *  sectors outside SECWM are NS and must be erased via NSCR even from Secure code]
   * [stm32h5xx_hal_flash.h: IS_FLASH_SECURE_OPERATION() checks FLASH_NON_SECURE_MASK] */
#ifdef EDATA_ENABLED
  s_eraseinit.TypeErase   = FLASH_EDATA_BASE;  /* EDATA sectors are Non-Secure */
  s_eraseinit.Sector      = Page + 120;
#else
  s_eraseinit.TypeErase   = FLASH_TYPEERASE_SECTORS;
  s_eraseinit.Sector      = Page;
#endif
  s_eraseinit.NbSectors   = NbPages;
  s_eraseinit.Banks       = bank;

  /* Erase the Page: Set Page status to ERASED status */
  if (HAL_FLASHEx_Erase(&s_eraseinit, &page_error) != HAL_OK)
  {
    status = EE_ERASE_ERROR;
  }
  return status;
}

/**
  * @brief  Erase a page with interrupt enabled
  * @param  Page Page number
  * @param  NbPages Number of pages to erase
  * @retval EE_Status
  *           - EE_OK: on success
  *           - EE error code: if an error occurs
  */
RAM_FUNC EE_Status FI_PageErase_IT(uint32_t Page, uint16_t NbPages)
{
  EE_Status status = EE_OK;
  FLASH_EraseInitTypeDef s_eraseinit;
  uint32_t bank = FLASH_BANK_1;

#ifdef FLASH_BANK_2
  bank = GetBankNumber(PAGE_ADDRESS(Page));
#endif

  /* Same NS erase rule as FI_PageErase: EDATA sectors are Non-Secure, use NSCR */
#ifdef EDATA_ENABLED
  s_eraseinit.TypeErase   = FLASH_TYPEERASE_SECTORS_NS;
  s_eraseinit.Sector      = Page + 120;
#else
  s_eraseinit.TypeErase   = FLASH_TYPEERASE_SECTORS;
  s_eraseinit.Sector      = Page;
#endif
  s_eraseinit.NbSectors   = NbPages;
  s_eraseinit.Banks       = bank;

  /* Erase the Page: Set Page status to ERASED status */
  if (HAL_FLASHEx_Erase_IT(&s_eraseinit) != HAL_OK)
  {
    status = EE_ERASE_ERROR;
  }
  return status;
}

/**
  * @brief  Flush the caches if needed to keep coherency when the flash content is modified
  */
RAM_FUNC void FI_CacheFlush()
{
  /* No flush needed. EEPROM flash area is defined as non-cacheable thanks to the MPU in main.c. */
  return;
}

#ifdef FLASH_BANK_2
/**
 * @brief  Gets the bank of a given address
 * @param  Address Address of the FLASH Memory
 * @retval Bank_Number The bank of a given address
 */
 RAM_FUNC static uint32_t GetBankNumber(uint32_t Address)
{
  uint32_t bank = 0U;

  #ifdef EDATA_ENABLED
    /* EDATA is always in bank1 */
    bank = FLASH_BANK_1;
  #else
    if (READ_BIT(FLASH->OPTSR_CUR, FLASH_OPTSR_SWAP_BANK) == 0U)
    {
      /* No Bank swap */
      if (Address < (FLASH_BASE + FLASH_BANK_SIZE))
      {
        bank = FLASH_BANK_1;
      }
      else
      {
        bank = FLASH_BANK_2;
      }
    }
    else
    {
      /* Bank swap */
      if (Address < (FLASH_BASE + FLASH_BANK_SIZE))
      {
        bank = FLASH_BANK_2;
      }
      else
      {
        bank = FLASH_BANK_1;
      }
    }
  #endif

  return bank;
}
#endif

/**
  * @brief  Delete corrupted Flash address, can be called from NMI. No Timeout.
  * @param  Address Address of the FLASH Memory to delete
  * @retval EE_Status
  *           - EE_OK: on success
  *           - EE error code: if an error occurs
  */
RAM_FUNC EE_Status FI_DeleteCorruptedFlashAddress(uint32_t Address)
{
#ifdef EDATA_ENABLED
  uint16_t HalfWord[4] = {0U, 0U, 0U, 0U};
#endif
  EE_Status status = EE_OK;
  /* Wait for previous programmation completion */
  while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY))
  {
  }

  /* Clear previous non-secure error programming flag */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  /* Set FLASH Programmation bit */
  SET_BIT(FLASH->NSCR, FLASH_CR_PG);

  /* Program quad word of value 0 */

#ifdef EDATA_ENABLED
   HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS, Address, (uint32_t)&(HalfWord[0]));
   HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS, Address+2, (uint32_t)&(HalfWord[1]));
   HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS, Address+4, (uint32_t)&(HalfWord[2]));
   HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS, Address+6, (uint32_t)&(HalfWord[3]));
#else
  // HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, Address, ((uint32_t)QuadWord));
   *(__IO uint64_t*)(Address) = (uint64_t)0U;
   *(__IO uint64_t*)(Address+8U) = (uint64_t)0U;
#endif
  /* Wait programmation completion */
  while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY))
  {
  }

  /* Check if error occured */
  if((__HAL_FLASH_GET_FLAG(FLASH_FLAG_WRPERR))  || (__HAL_FLASH_GET_FLAG(FLASH_FLAG_PGSERR)) ||
     (__HAL_FLASH_GET_FLAG(FLASH_FLAG_STRBERR)) || (__HAL_FLASH_GET_FLAG(FLASH_FLAG_INCERR)))
  {
    status = EE_DELETE_ERROR;
  }

  /* Check FLASH End of Operation flag  */
  if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_EOP))
  {
    /* Clear FLASH End of Operation pending bit */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);
  }

  /* Clear FLASH Programmation bit */
  CLEAR_BIT(FLASH->NSCR, FLASH_CR_PG);

  /* Clear FLASH ECCD bit */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ECCD);

  return status;
}

/**
  * @brief  Check if the configuration is 128-bits bank or 2*64-bits bank
  * @param  None
  * @retval EE_Status
  *           - EE_OK: on success
  *           - EE error code: if an error occurs
  */
RAM_FUNC EE_Status FI_CheckBankConfig(void)
{
#if defined (FLASH_OPTR_DBANK)
  FLASH_OBProgramInitTypeDef sOBCfg;
  EE_Status status;

  /* Request the Option Byte configuration :
     - User and RDP level are always returned
     - WRP and PCROP are not requested */
  sOBCfg.WRPArea     = 0xFF;
  HAL_FLASHEx_OBGetConfig(&sOBCfg);

  /* Check the value of the DBANK user option byte */
  if ((sOBCfg.USERConfig & OB_DBANK_64_BITS) != 0)
  {
    status = EE_OK;
  }
  else
  {
    status = EE_INVALID_BANK_CFG;
  }

  return status;
#else
  /* No feature 128-bits single bank, so always 64-bits dual bank */
  return EE_OK;
#endif
}

/**
  * @brief  INSERT BRIEF
  * @param  INSERT BRIEF
  *         INSERT BRIEF
  *         INSERT BRIEF
  *         INSERT BRIEF
  *         INSERT BRIEF
  * @retval None
  */
RAM_FUNC void OB_Init( void )
{
  /* EDATA option bytes are now programmed by flash_nucleo.py --option-bytes-only
   * during board setup (alongside NSBOOTADD, SECWM, TZEN).
   * This function is intentionally a no-op — EDATA configuration is a
   * one-time board setup step, not a runtime firmware responsibility.
   *
   * flash_nucleo.py programs:
   *   EDATA1 = 0x8007  (Bank 1: enabled, 8 sectors 120-127, 100K-cycle)
   *   EDATA2 = 0x0000  (Bank 2: disabled)
   */
}

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/



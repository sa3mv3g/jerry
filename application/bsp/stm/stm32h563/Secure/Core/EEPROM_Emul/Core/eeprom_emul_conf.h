/**
  ******************************************************************************
  * @file    eeprom_emul_conf.h
  * @author  MCD Application Team
  * @brief   EEPROM emulation configuration file.
  *          This file should be copied to the application folder and renamed
  *          to eeprom_emul_conf.h.
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

/** @addtogroup EEPROM_Emulation
  * @{
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __EEPROM_EMUL_CONF_H
#define __EEPROM_EMUL_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Private constants ---------------------------------------------------------*/
/** @addtogroup EEPROM_Private_Constants
  * @{
  */

/** @defgroup Private_Configuration_Constants Private Configuration Constants
  * @{
  */

/* Configuration of EEPROM emulation in flash.
 *
 * This configuration is for the SECURE firmware build only.
 * EDATA_ENABLED must be defined in the Secure CMakeLists.txt.
 *
 * When EDATA_ENABLED is defined:
 *   - FLASH_EDATA_BASE = FLASH_EDATA_BASE_S = 0x0D000000 (Secure alias, stable)
 *   - BANK_SIZE = FLASH_EDATA_SIZE >> 1 = 48KB
 *   - FLASH_PAGE_SIZE = FLASH_EDATA_SIZE >> 4 = 6KB
 *   - START_PAGE_ADDRESS is NOT used (base derived from FLASH_EDATA_BASE)
 *
 * The Secure alias 0x0D000000 does NOT change with SWAP_BANK, so calibration
 * data stored here survives all FOTA firmware updates.
 * [stm32h563xx.h: FLASH_EDATA_BASE_S = 0x0D000000UL]
 * [RM0481 §7.3.10: EDATA sectors 120-127 for STM32H562/563/573xx]
 */

/* When EDATA_ENABLED is defined, START_PAGE_ADDRESS must be set to the EDATA base.
 * In the Secure build (CMSE=3), FLASH_EDATA_BASE = FLASH_EDATA_BASE_S = 0x0D000000.
 * This is used by eeprom_emul.h macros: END_EEPROM_ADDRESS, START_PAGE, PAGE_ADDRESS.
 * [stm32h563xx.h: FLASH_EDATA_BASE_S = 0x0D000000UL, FLASH_EDATA_SIZE = 0x18000U] */
#ifdef EDATA_ENABLED
#define START_PAGE_ADDRESS      FLASH_EDATA_BASE   /*!< EDATA base: 0x0D000000 (Secure alias, stable across SWAP_BANK) */
#else
#define START_PAGE_ADDRESS      0x081F0000U        /*!< Legacy: Bank 2 last sector (non-EDATA mode, not used in this project) */
#endif

#define CYCLES_NUMBER           1U   /*!< 1 x 10K base cycles; EDATA provides 100K cycles endurance.
                                         [RM0481 §7.2: "100 kcycles" for high-cycle data area] */
#define GUARD_PAGES_NUMBER      2U   /*!< Number of guard pages avoiding frequent transfers (must be multiple of 2): 0,2,4.. */

/* Configuration of crc calculation for eeprom emulation in flash */
#define CRC_POLYNOMIAL_LENGTH   LL_CRC_POLYLENGTH_16B /* CRC polynomial lenght 16 bits */
#define CRC_POLYNOMIAL_VALUE    0x8005U /* Polynomial to use for CRC calculation */

/**
  * @}
  */

/**
  * @}
  */

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/** @defgroup EEPROM_Exported_Constants EEPROM Exported Constants
  * @{
  */

/** @defgroup Exported_Configuration_Constants Exported Configuration Constants
  * @{
  */
/* 4 channels × 3 params (scaling_factor, offset_term, deadzone) = 12 ADC cal vars
 * + 2 system vars (Modbus address, baud rate) + 1 FOTA flag = 15 total.
 * Set to 20 for headroom. [eeprom_emul.h: NB_OF_VARIABLES drives page count] */
#define NB_OF_VARIABLES         20U    /*!< Number of variables to handle in eeprom */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

/**
  * @}
  */

#endif /* __EEPROM_EMUL_CONF_H */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

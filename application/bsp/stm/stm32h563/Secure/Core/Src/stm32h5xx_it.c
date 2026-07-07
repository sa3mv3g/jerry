/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h5xx_it.c
  * @brief   Interrupt Service Routines.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32h5xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "eeprom_emul.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */
/* EEPROM emulation globals defined in eeprom_emul.c — needed by NMI_Handler */
extern __IO uint32_t AddressRead;   /*!< Address being read during cleanup phase */
extern __IO uint8_t  CleanupPhase;  /*!< 1 when EE_Init cleanup is in progress  */
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */
  uint32_t Address;

  /* Check if NMI is due to flash ECCD (double-bit ECC error detection) in EDATA region.
   * The Secure firmware owns FLASH_EDATA_BASE_S (0x0D000000) and all EEPROM emulation
   * state. This handler mirrors the ST EEPROM emulation example NMI handler.
   * [RM0481 §7.3.10: EDATA 6-bit ECC on 16-bit words; ECCD triggers NMI] */
  if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_ECCD))
  {
#ifdef EDATA_ENABLED
    /* Check if NMI is related to a 0xFFFF value (empty value or possible header) */
    if (READ_REG(FLASH->ECCDR) == 0xFFFF)
    {
      /* Clear the flag and exit — do not invalidate; may be an empty/header line */
      __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ECCD);
      return;
    }
#endif
    if (CleanupPhase == 1)
    {
      if ((AddressRead >= START_PAGE_ADDRESS) && (AddressRead <= END_EEPROM_ADDRESS))
      {
        /* Delete the corrupted flash address */
#ifdef EDATA_ENABLED
        Address = (uint32_t)(AddressRead & 0xFFFFFFF8U);
#else
        Address = (uint32_t)(AddressRead & 0xFFFFFFF0U);
#endif
        if (EE_DeleteCorruptedFlashAddress((uint32_t)Address) == EE_OK)
        {
          /* Resume execution if deletion succeeds */
          return;
        }
        /* Deletion failed — check for programming error flags */
        else
        {
          if ((__HAL_FLASH_GET_FLAG(FLASH_FLAG_WRPERR))  ||
              (__HAL_FLASH_GET_FLAG(FLASH_FLAG_PGSERR))  ||
              (__HAL_FLASH_GET_FLAG(FLASH_FLAG_STRBERR)) ||
              (__HAL_FLASH_GET_FLAG(FLASH_FLAG_INCERR)))
          {
            __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);
            __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGSERR);
            __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_STRBERR);
            __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_INCERR);
            return;
          }
        }
      }
    }
    else
    {
      __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ECCD);
      return;
    }
  }
  /* USER CODE END NonMaskableInt_IRQn 0 */

  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  /* Go to infinite loop when NMI occurs in case:
   * - ECCD raised in EDATA pages but corrupted address deletion fails
   * - ECCD raised outside EDATA pages
   * - no ECCD raised */
  while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  /* Get fault status */
  volatile uint32_t cfsr = SCB->CFSR;
  volatile uint32_t hfsr = SCB->HFSR;
  volatile uint32_t bfar = SCB->BFAR;
  volatile uint32_t mmfar = SCB->MMFAR;

  /* Determine fault type for blink pattern */
  uint32_t blink_count = 4;  /* Default: unknown */
  if (cfsr & 0xFFFF0000) {
    blink_count = 1;  /* Usage Fault */
  } else if (cfsr & 0x0000FF00) {
    blink_count = 2;  /* Bus Fault */
  } else if (cfsr & 0x000000FF) {
    blink_count = 3;  /* Memory Management Fault */
  }

  /* Store fault info in a known RAM location for debugger inspection */
  /* These can be read via debugger at addresses 0x20000000-0x20000010 */
  volatile uint32_t *fault_info = (volatile uint32_t *)0x20000000;
  fault_info[0] = 0xDEADBEEF;  /* Marker */
  fault_info[1] = cfsr;
  fault_info[2] = hfsr;
  fault_info[3] = bfar;
  fault_info[4] = mmfar;

  /* Setup LED for blink pattern */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_0;  /* Green LED on Nucleo-H563ZI */
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Also turn on Red LED (PB14) to indicate fault */
  GPIO_InitStruct.Pin = GPIO_PIN_14;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);  /* Red LED ON */
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* Blink pattern: N blinks, then long pause */
    for (uint32_t b = 0; b < blink_count; b++) {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
      for (volatile uint32_t i = 0; i < 300000; i++);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
      for (volatile uint32_t i = 0; i < 300000; i++);
    }
    /* Long pause between patterns */
    for (volatile uint32_t i = 0; i < 2000000; i++);
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Secure fault.
  */
void SecureFault_Handler(void)
{
  /* USER CODE BEGIN SecureFault_IRQn 0 */
  /* Get Secure Fault Status Register */
  volatile uint32_t sfsr = SAU->SFSR;
  volatile uint32_t sfar = SAU->SFAR;

  /* Store fault info in a known RAM location for debugger inspection */
  /* Offset from HardFault info to avoid overlap */
  volatile uint32_t *fault_info = (volatile uint32_t *)0x20000020;
  fault_info[0] = 0x5ECF0000;  /* Marker for SecureFault */
  fault_info[1] = sfsr;
  fault_info[2] = sfar;
  fault_info[3] = SCB->CFSR;
  fault_info[4] = SCB->HFSR;

  /* Setup LEDs */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Green LED (PB0) for blink pattern */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Yellow LED (PF4) ON to indicate SecureFault */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_4, GPIO_PIN_SET);  /* Yellow LED ON */
  /* USER CODE END SecureFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_SecureFault_IRQn 0 */
    /* 5 blinks = SecureFault (TrustZone violation) */
    for (uint32_t b = 0; b < 5; b++) {
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
      for (volatile uint32_t i = 0; i < 200000; i++);
      HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
      for (volatile uint32_t i = 0; i < 200000; i++);
    }
    /* Long pause between patterns */
    for (volatile uint32_t i = 0; i < 2000000; i++);
    /* USER CODE END W1_SecureFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32H5xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h5xx.s).                    */
/******************************************************************************/

/* USER CODE BEGIN 1 */

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/**
  * @brief This function handles WWDG interrupt (for debugging).
  */
void WWDG_IRQHandler(void)
{
  while (1)
  {
    /* If we get here, WWDG actually fired */
  }
}

/**
  * @brief This function handles FLASH secure interrupts (for debugging).
  */
void FLASH_S_IRQHandler(void)
{
  while (1)
  {
    /* If we get here, FLASH_S triggered an interrupt */
  }
}

/* USER CODE END 1 */

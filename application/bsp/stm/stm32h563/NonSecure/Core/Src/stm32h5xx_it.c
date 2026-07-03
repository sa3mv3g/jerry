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
#include <stdint.h>
#include <stdio.h>
#include "eeprom_emul.h"
#include "log.h"
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
extern DMA_NodeTypeDef Node_GPDMA1_Channel0;
extern DMA_QListTypeDef List_GPDMA1_Channel0;
extern DMA_HandleTypeDef handle_GPDMA1_Channel0;
extern ETH_HandleTypeDef heth;
extern I2C_HandleTypeDef hi2c4;
extern DMA_NodeTypeDef Node_GPDMA1_Channel4;
extern DMA_QListTypeDef List_GPDMA1_Channel4;
extern DMA_HandleTypeDef handle_GPDMA1_Channel4;
extern DMA_NodeTypeDef Node_GPDMA1_Channel3;
extern DMA_QListTypeDef List_GPDMA1_Channel3;
extern DMA_HandleTypeDef handle_GPDMA1_Channel3;
extern DMA_NodeTypeDef Node_GPDMA1_Channel2;
extern DMA_QListTypeDef List_GPDMA1_Channel2;
extern DMA_HandleTypeDef handle_GPDMA1_Channel2;
extern DMA_NodeTypeDef Node_GPDMA1_Channel1;
extern DMA_QListTypeDef List_GPDMA1_Channel1;
extern DMA_HandleTypeDef handle_GPDMA1_Channel1;
extern TIM_HandleTypeDef htim6;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
  LOG_ERR("!!! MemManage_Handler !!!");
  LOG_ERR("CFSR=0x%08lX, MMFAR=0x%08lX",
         SCB->CFSR, SCB->MMFAR);
  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
  LOG_ERR("!!! UsageFault_Handler !!!");
  LOG_ERR("CFSR=0x%08lX", SCB->CFSR);
  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/******************************************************************************/
/* STM32H5xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h5xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI Line13 interrupt.
  */
void EXTI13_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI13_IRQn 0 */

  /* USER CODE END EXTI13_IRQn 0 */
  BSP_PB_IRQHandler(BUTTON_USER);
  /* USER CODE BEGIN EXTI13_IRQn 1 */

  /* USER CODE END EXTI13_IRQn 1 */
}

/**
  * @brief This function handles GPDMA1 Channel 0 global interrupt.
  */
void GPDMA1_Channel0_IRQHandler(void)
{
  /* USER CODE BEGIN GPDMA1_Channel0_IRQn 0 */

  /* USER CODE END GPDMA1_Channel0_IRQn 0 */
  HAL_DMA_IRQHandler(&handle_GPDMA1_Channel0);
  /* USER CODE BEGIN GPDMA1_Channel0_IRQn 1 */

  /* USER CODE END GPDMA1_Channel0_IRQn 1 */
}

/**
  * @brief This function handles GPDMA1 Channel 1 global interrupt.
  */
void GPDMA1_Channel1_IRQHandler(void)
{
  /* USER CODE BEGIN GPDMA1_Channel1_IRQn 0 */

  /* USER CODE END GPDMA1_Channel1_IRQn 0 */
  HAL_DMA_IRQHandler(&handle_GPDMA1_Channel1);
  /* USER CODE BEGIN GPDMA1_Channel1_IRQn 1 */

  /* USER CODE END GPDMA1_Channel1_IRQn 1 */
}

/**
  * @brief This function handles GPDMA1 Channel 2 global interrupt.
  */
void GPDMA1_Channel2_IRQHandler(void)
{
  /* USER CODE BEGIN GPDMA1_Channel2_IRQn 0 */

  /* USER CODE END GPDMA1_Channel2_IRQn 0 */
  HAL_DMA_IRQHandler(&handle_GPDMA1_Channel2);
  /* USER CODE BEGIN GPDMA1_Channel2_IRQn 1 */

  /* USER CODE END GPDMA1_Channel2_IRQn 1 */
}

/**
  * @brief This function handles GPDMA1 Channel 3 global interrupt.
  */
void GPDMA1_Channel3_IRQHandler(void)
{
  /* USER CODE BEGIN GPDMA1_Channel3_IRQn 0 */

  /* USER CODE END GPDMA1_Channel3_IRQn 0 */
  HAL_DMA_IRQHandler(&handle_GPDMA1_Channel3);
  /* USER CODE BEGIN GPDMA1_Channel3_IRQn 1 */

  /* USER CODE END GPDMA1_Channel3_IRQn 1 */
}

/**
  * @brief This function handles GPDMA1 Channel 4 global interrupt.
  */
void GPDMA1_Channel4_IRQHandler(void)
{
  /* USER CODE BEGIN GPDMA1_Channel4_IRQn 0 */

  /* USER CODE END GPDMA1_Channel4_IRQn 0 */
  HAL_DMA_IRQHandler(&handle_GPDMA1_Channel4);
  /* USER CODE BEGIN GPDMA1_Channel4_IRQn 1 */

  /* USER CODE END GPDMA1_Channel4_IRQn 1 */
}

/**
  * @brief This function handles TIM6 global interrupt.
  */
void TIM6_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_IRQn 0 */

  /* USER CODE END TIM6_IRQn 0 */
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_IRQn 1 */

  /* USER CODE END TIM6_IRQn 1 */
}

/**
  * @brief This function handles Ethernet global interrupt.
  */
void ETH_IRQHandler(void)
{
  /* USER CODE BEGIN ETH_IRQn 0 */

  /* USER CODE END ETH_IRQn 0 */
  HAL_ETH_IRQHandler(&heth);
  /* USER CODE BEGIN ETH_IRQn 1 */

  /* USER CODE END ETH_IRQn 1 */
}

/**
  * @brief This function handles I2C4 Event interrupt.
  */
void I2C4_EV_IRQHandler(void)
{
  /* USER CODE BEGIN I2C4_EV_IRQn 0 */

  /* USER CODE END I2C4_EV_IRQn 0 */
  HAL_I2C_EV_IRQHandler(&hi2c4);
  /* USER CODE BEGIN I2C4_EV_IRQn 1 */

  /* USER CODE END I2C4_EV_IRQn 1 */
}

/**
  * @brief This function handles I2C4 Error interrupt.
  */
void I2C4_ER_IRQHandler(void)
{
  /* USER CODE BEGIN I2C4_ER_IRQn 0 */

  /* USER CODE END I2C4_ER_IRQn 0 */
  HAL_I2C_ER_IRQHandler(&hi2c4);
  /* USER CODE BEGIN I2C4_ER_IRQn 1 */

  /* USER CODE END I2C4_ER_IRQn 1 */
}

/* USER CODE BEGIN 1 */

void EXTI15_10_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
}

void PVD_AVD_IRQHandler(void)
{
  HAL_PWR_PVD_IRQHandler();
}

/**
  * @brief  This function handles PVD interrupt callback.
  * @param  None
  * @retval None
  */
void HAL_PWR_PVDCallback(void)
{
  /* Loop inside the handler to prevent the Cortex from using the Flash,
     allowing the flash interface to finish any ongoing transfer. */
  while (__HAL_PWR_GET_FLAG(PWR_FLAG_PVDO) != RESET)
  {
  }
}

void NMI_Handler(void)
{
  uint32_t Address;

    /* Check if NMI is due to flash ECCD (error detection) */
  if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_ECCD))
  {
#ifdef EDATA_ENABLED
	/* Check if NMI is related to a 0xFFFF value (empty value or possible header*/
    if (READ_REG(FLASH->ECCDR) == 0xFFFF)
    {
	/* If yes, we clear the flag */
      __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ECCD);

	/* And we exit from NMI without doing anything */
	/* We do not invalidate that line because it may try to reprogramm at 0 a header line or an empty line read at 0xFFFF */
	/* The only consequence is that this line will trigger a new NMI later */
      return;
    }
#endif
    if(CleanupPhase==1)
    {
      if ((AddressRead >= START_PAGE_ADDRESS) && (AddressRead <= END_EEPROM_ADDRESS))
      {
      	/* Delete the corrupted flash address */
#ifdef EDATA_ENABLED
        Address=(uint32_t)(AddressRead&0xFFFFFFF8);
#else
        Address=(uint32_t)(AddressRead&0xFFFFFFF0);
#endif
        if (EE_DeleteCorruptedFlashAddress((uint32_t)Address) == EE_OK)
        {
          /* Resume execution if deletion succeeds */
          return;
        }
        /* If we do not succeed to delete the corrupted flash address */
        /* This might be because we try to write 0 at a line already considered at 0 which is a forbidden operation */
        /* This problem triggers PROGERR, PGAERR and PGSERR flags */
        else
        {
          /* We check if the flags concerned have been triggered */
          if((__HAL_FLASH_GET_FLAG(FLASH_FLAG_WRPERR))  || (__HAL_FLASH_GET_FLAG(FLASH_FLAG_PGSERR)) ||
             (__HAL_FLASH_GET_FLAG(FLASH_FLAG_STRBERR)) || (__HAL_FLASH_GET_FLAG(FLASH_FLAG_INCERR)))
          {
            /* If yes, we clear them */
            __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_WRPERR);
            __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGSERR);
            __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_STRBERR);
            __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_INCERR);

            /* And we exit from NMI without doing anything */
            /* We do not invalidate that line because it is not programmable at 0 till the next page erase */
            /* The only consequence is that this line will trigger a new NMI later */
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

  /* Go to infinite loop when NMI occurs in case:
     - ECCD is raised in eeprom emulation flash pages but corrupted flash address deletion fails (except PROGERR, PGAERR and PGSERR)
     - ECCD is raised out of eeprom emulation flash pages
     - no ECCD is raised */

  /* Go to infinite loop when NMI occurs */
  while (1)
  {
    /* Toggle LED3 fast */
    BSP_LED_Toggle(LED3);
    HAL_Delay(40);
  }

}

/* USER CODE END 1 */

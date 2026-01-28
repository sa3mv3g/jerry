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
extern ETH_HandleTypeDef heth;
extern HCD_HandleTypeDef hhcd_USB_DRD_FS;
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
  printf("\r\n!!! MemManage_Handler !!!\r\n");
  printf("CFSR=0x%08lX, MMFAR=0x%08lX\r\n",
         SCB->CFSR, SCB->MMFAR);
  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Hard fault.
  *
  * This handler prints detailed fault information to help debug the crash.
  * The CFSR (Configurable Fault Status Register) contains:
  *   - UFSR (bits 31:16): Usage Fault Status
  *   - BFSR (bits 15:8): Bus Fault Status
  *   - MMFSR (bits 7:0): Memory Management Fault Status
  */
void HardFault_Handler(void)
{
  /* Get the stack pointer that was active when the fault occurred */
  volatile uint32_t *sp;
  __asm volatile ("mrs %0, msp" : "=r" (sp));

  printf("\r\n");
  printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
  printf("!!! HARD FAULT DETECTED !!!\r\n");
  printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");

  /* Print fault status registers */
  printf("CFSR  = 0x%08lX\r\n", SCB->CFSR);
  printf("HFSR  = 0x%08lX\r\n", SCB->HFSR);
  printf("BFAR  = 0x%08lX\r\n", SCB->BFAR);
  printf("MMFAR = 0x%08lX\r\n", SCB->MMFAR);

  /* Decode CFSR */
  if (SCB->CFSR & 0xFFFF0000) {
    printf("Usage Fault:\r\n");
    if (SCB->CFSR & (1 << 25)) printf("  - DIVBYZERO: Divide by zero\r\n");
    if (SCB->CFSR & (1 << 24)) printf("  - UNALIGNED: Unaligned access\r\n");
    if (SCB->CFSR & (1 << 19)) printf("  - NOCP: No coprocessor\r\n");
    if (SCB->CFSR & (1 << 18)) printf("  - INVPC: Invalid PC\r\n");
    if (SCB->CFSR & (1 << 17)) printf("  - INVSTATE: Invalid state\r\n");
    if (SCB->CFSR & (1 << 16)) printf("  - UNDEFINSTR: Undefined instruction\r\n");
  }
  if (SCB->CFSR & 0x0000FF00) {
    printf("Bus Fault:\r\n");
    if (SCB->CFSR & (1 << 15)) printf("  - BFARVALID: BFAR valid\r\n");
    if (SCB->CFSR & (1 << 13)) printf("  - LSPERR: FP lazy state preservation\r\n");
    if (SCB->CFSR & (1 << 12)) printf("  - STKERR: Stacking error\r\n");
    if (SCB->CFSR & (1 << 11)) printf("  - UNSTKERR: Unstacking error\r\n");
    if (SCB->CFSR & (1 << 10)) printf("  - IMPRECISERR: Imprecise data bus error\r\n");
    if (SCB->CFSR & (1 << 9))  printf("  - PRECISERR: Precise data bus error\r\n");
    if (SCB->CFSR & (1 << 8))  printf("  - IBUSERR: Instruction bus error\r\n");
  }
  if (SCB->CFSR & 0x000000FF) {
    printf("Memory Management Fault:\r\n");
    if (SCB->CFSR & (1 << 7)) printf("  - MMARVALID: MMFAR valid\r\n");
    if (SCB->CFSR & (1 << 5)) printf("  - MLSPERR: FP lazy state preservation\r\n");
    if (SCB->CFSR & (1 << 4)) printf("  - MSTKERR: Stacking error\r\n");
    if (SCB->CFSR & (1 << 3)) printf("  - MUNSTKERR: Unstacking error\r\n");
    if (SCB->CFSR & (1 << 1)) printf("  - DACCVIOL: Data access violation\r\n");
    if (SCB->CFSR & (1 << 0)) printf("  - IACCVIOL: Instruction access violation\r\n");
  }

  /* Decode HFSR */
  if (SCB->HFSR & (1 << 31)) printf("HFSR: DEBUGEVT - Debug event\r\n");
  if (SCB->HFSR & (1 << 30)) printf("HFSR: FORCED - Forced hard fault (escalated)\r\n");
  if (SCB->HFSR & (1 << 1))  printf("HFSR: VECTTBL - Vector table read fault\r\n");

  /* Print stacked registers (if stack pointer is valid) */
  if (sp != NULL && ((uint32_t)sp >= 0x20000000) && ((uint32_t)sp < 0x30000000)) {
    printf("\r\nStacked registers:\r\n");
    printf("  R0  = 0x%08lX\r\n", sp[0]);
    printf("  R1  = 0x%08lX\r\n", sp[1]);
    printf("  R2  = 0x%08lX\r\n", sp[2]);
    printf("  R3  = 0x%08lX\r\n", sp[3]);
    printf("  R12 = 0x%08lX\r\n", sp[4]);
    printf("  LR  = 0x%08lX  <-- Return address\r\n", sp[5]);
    printf("  PC  = 0x%08lX  <-- Faulting instruction\r\n", sp[6]);
    printf("  xPSR= 0x%08lX\r\n", sp[7]);
  }

  printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
  printf("\r\n");

  /* Flush output */
  fflush(stdout);

  /* Halt */
  __disable_irq();
  while (1)
  {
    /* Blink LED if available to indicate fault */
  }
}

/**
  * @brief This function handles Bus fault.
  */
void BusFault_Handler(void)
{
  printf("\r\n!!! BusFault_Handler !!!\r\n");
  printf("CFSR=0x%08lX, BFAR=0x%08lX\r\n", SCB->CFSR, SCB->BFAR);
  while (1)
  {
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
  printf("\r\n!!! UsageFault_Handler !!!\r\n");
  printf("CFSR=0x%08lX\r\n", SCB->CFSR);
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
  * @brief This function handles USB FS global interrupt.
  */
void USB_DRD_FS_IRQHandler(void)
{
  /* USER CODE BEGIN USB_DRD_FS_IRQn 0 */

  /* USER CODE END USB_DRD_FS_IRQn 0 */
  HAL_HCD_IRQHandler(&hhcd_USB_DRD_FS);
  /* USER CODE BEGIN USB_DRD_FS_IRQn 1 */

  /* USER CODE END USB_DRD_FS_IRQn 1 */
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

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

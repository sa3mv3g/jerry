/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "eeprom_emul.h"  
#include "flash_interface.h" 
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* USER CODE BEGIN VTOR_TABLE */

/* Non-secure Vector table start address.
 * Must match NSBOOTADD option byte (tools/flash_nucleo.py) and NS linker script ORIGIN.
 * Layout: Bank 1 sectors 0-7 (64KB code) + sector 8 (8KB NSC) = 72KB Secure.
 * NS firmware starts at Bank 1 base + 9 sectors x 8KB = 0x0801_2000.
 * After SWAP_BANK=1 (FOTA): NSBOOTADD maps 0x0801_2000 to Bank 2 sector 9.
 * [RM0481 Table 48: sector 9 of Bank 1 = 0x0801_2000]
 * [plans/flash_storage_and_fota_plan.md §2.2] */
#define VTOR_TABLE_NS_START_ADDR  0x08040000UL

/* USER CODE END VTOR_TABLE*/

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

RTC_HandleTypeDef hrtc;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void NonSecure_Init(void);
void SystemClock_Config(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */
static void MPU_Config_EDATA(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  MX_GTZC_S_Init();
  MX_GPIO_Init();
  MX_ICACHE_Init();
  MX_RTC_Init();

  /* Configure MPU for EDATA_S region (must be before EE_Init / EEPROM emulation).
   * Marks 0x0D000000-0x0D00BFFF (Bank 1 EDATA, Secure alias) as non-cacheable.
   * Required because EDATA uses 6-bit ECC on 16-bit words — caching would corrupt ECC.
   * [RM0481 §7.3.2: "For regions where caching is not practical (OTP, RO, data area),
   *  MPU must be used to disable local cacheability."] */
  MPU_Config_EDATA();

  /* Unlock flash before EEPROM emulation init.
   * EE_Init → EE_Format → FI_PageErase → HAL_FLASHEx_Erase → FLASH_Erase_Sector
   * writes directly to FLASH->SECCR/NSCR. EDATA sectors 120-127 are outside
   * SECWM1_END=8 (Non-Secure), so they must be erased via FLASH->NSCR.
   * HAL_FLASH_Unlock() is required: it unlocks both NSCR and SECCR.
   * The HAL guards against double-unlock (checks FLASH_CR_LOCK before writing keys).
   * [RM0481 §7.6.1: Sectors outside SECWM are Non-Secure; use NSCR for erase]
   * [stm32h5xx_hal_flash.c line 563/579: double-unlock guard] */
  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    Error_Handler();
  }

  /* Initialize EEPROM emulation on Bank 1 EDATA via 0x0D000000 (Secure alias).
   * EDATA option bytes (EDATA1_EN=1, EDATA1_STRT=7, EDATA2=disabled) are programmed
   * once during board setup by flash_nucleo.py --option-bytes-only, not at runtime.
   * EE_FORCED_ERASE recovers from incomplete operations after cold boot.
   * FI_PageErase uses FLASH_TYPEERASE_SECTORS_NS because EDATA sectors are Non-Secure.
   * [RM0481 §7.3.10: EDATA sectors 120-127, accessible at 0x0D000000 via Secure alias] */
  if (EE_Init(EE_FORCED_ERASE) != EE_OK)
  {
    Error_Handler();
  }

  /* Disable Secure SysTick before jumping to Non-Secure */
  SysTick->CTRL = 0;

  /* Clear all NS NVIC pending interrupts before jumping to NS.
   * In TrustZone, NVIC has two views:
   *   Secure alias:    0xE000E000 (NVIC_S) — used by Secure code via NVIC pointer
   *   Non-Secure alias: 0xE002E000 (NVIC_NS) — controls NS interrupt pending state
   * NVIC->ICPR[] in Secure code writes to NVIC_S (Secure NVIC), NOT NVIC_NS.
   * To clear NS pending bits, we must write to NVIC_NS->ICPR[] directly.
   * [Cortex-M33 TRM: NVIC_NS base = 0xE002E000, ICPR offset = 0x280]
   * [ARM TRM: In Secure state, NVIC_NS is accessible at 0xE002E000] */
  NVIC_NS->ICPR[0] = 0xFFFFFFFFU;  /* Clear NS pending IRQs 0-31  */
  NVIC_NS->ICPR[1] = 0xFFFFFFFFU;  /* Clear NS pending IRQs 32-63 */
  NVIC_NS->ICPR[2] = 0xFFFFFFFFU;  /* Clear NS pending IRQs 64-95 */
  NVIC_NS->ICPR[3] = 0xFFFFFFFFU;  /* Clear NS pending IRQs 96-127 */
  NVIC_NS->ICPR[4] = 0xFFFFFFFFU;  /* Clear NS pending IRQs 128-130 */

  NonSecure_Init();

  /* Non-secure software does not return, this code is not executed */
  while (1)
  {
  }
  /* USER CODE END 3 */
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */

/**
  * @brief  Non-secure call function
  *         This function is responsible for Non-secure initialization and switch
  *         to non-secure state
  * @retval None
  */
static void NonSecure_Init(void)
{
  funcptr_NS NonSecure_ResetHandler;

  SCB_NS->VTOR = VTOR_TABLE_NS_START_ADDR;

  /* Set non-secure main stack (MSP_NS) */
  __TZ_set_MSP_NS((*(uint32_t *)VTOR_TABLE_NS_START_ADDR));

  /* Get non-secure reset handler */
  NonSecure_ResetHandler = (funcptr_NS)(*((uint32_t *)((VTOR_TABLE_NS_START_ADDR) + 4U)));

  /* Start non-secure state software application */
  NonSecure_ResetHandler();
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI
                              |RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS_DIGITAL;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLL1_SOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 250;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1_VCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1_VCORANGE_WIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_2);
}

/**
  * @brief GTZC_S Initialization Function
  * @param None
  * @retval None
  */
void MX_GTZC_S_Init(void)
{

  /* USER CODE BEGIN GTZC_S_Init 0 */

  /* USER CODE END GTZC_S_Init 0 */

  MPCBB_ConfigTypeDef MPCBB_Area_Desc = {0};

  /* USER CODE BEGIN GTZC_S_Init 1 */
  /* Configure ETH peripheral as non-secure so DMA can access non-secure SRAM3 */
  if (HAL_GTZC_TZSC_ConfigPeriphAttributes(GTZC_PERIPH_ETHERNET, GTZC_TZSC_PERIPH_NSEC) != HAL_OK)
  {
    Error_Handler();
  }
  /* Configure I2C3 peripheral as non-secure for non-secure application access */
  if (HAL_GTZC_TZSC_ConfigPeriphAttributes(GTZC_PERIPH_I2C3, GTZC_TZSC_PERIPH_NSEC) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END GTZC_S_Init 1 */
  MPCBB_Area_Desc.SecureRWIllegalMode = GTZC_MPCBB_SRWILADIS_ENABLE;
  MPCBB_Area_Desc.InvertSecureState = GTZC_MPCBB_INVSECSTATE_NOT_INVERTED;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[0] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[1] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[2] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[3] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[4] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[5] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[6] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[7] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[8] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[9] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[10] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[11] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[12] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[13] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[14] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[15] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[16] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[17] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[18] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_SecConfig_array[19] =   0x00000000;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[0] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[1] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[2] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[3] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[4] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[5] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[6] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[7] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[8] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[9] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[10] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[11] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[12] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[13] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[14] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[15] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[16] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[17] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[18] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[19] =   0xFFFFFFFF;
  MPCBB_Area_Desc.AttributeConfig.MPCBB_LockConfig_array[0] =   0x00000000;
  if (HAL_GTZC_MPCBB_ConfigMem(SRAM3_BASE, &MPCBB_Area_Desc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN GTZC_S_Init 2 */
  /* Override SRAM3 privilege configuration for ETH DMA access.
  * ETH DMA is a bus master that runs in non-privileged mode.
  * If SRAM3 is configured as privileged (0xFFFFFFFF), the ETH DMA cannot
  * access the DMA descriptors and RX buffers, causing RBU errors and
  * RPS=4 (Suspended). Setting all blocks to non-privileged (0x00000000)
  * allows the ETH DMA to function correctly.
  *
  * This override is placed in USER CODE section to survive CubeMX regeneration.
  */
  for (uint32_t i = 0; i < 20U; i++)
  {
    MPCBB_Area_Desc.AttributeConfig.MPCBB_PrivConfig_array[i] = 0x00000000U;
  }
  if (HAL_GTZC_MPCBB_ConfigMem(SRAM3_BASE, &MPCBB_Area_Desc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END GTZC_S_Init 2 */

}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_PrivilegeStateTypeDef privilegeState = {0};
  RTC_SecureStateTypeDef secureState = {0};
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
  hrtc.Init.BinMode = RTC_BINARY_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  privilegeState.rtcPrivilegeFull = RTC_PRIVILEGE_FULL_NO;
  privilegeState.backupRegisterPrivZone = RTC_PRIVILEGE_BKUP_ZONE_NONE;
  privilegeState.backupRegisterStartZone2 = RTC_BKP_DR0;
  privilegeState.backupRegisterStartZone3 = RTC_BKP_DR0;
  if (HAL_RTCEx_PrivilegeModeSet(&hrtc, &privilegeState) != HAL_OK)
  {
    Error_Handler();
  }
  secureState.rtcSecureFull = RTC_SECURE_FULL_YES;
  secureState.backupRegisterStartZone2 = RTC_BKP_DR0;
  secureState.backupRegisterStartZone3 = RTC_BKP_DR0;
  if (HAL_RTCEx_SecureModeSet(&hrtc, &secureState) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_FRIDAY;
  sDate.Month = RTC_MONTH_JUNE;
  sDate.Date = 0x26;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*IO attributes management functions */
  HAL_GPIO_ConfigPinAttributes(GPIOF, DI4_Pin|DI3_Pin|DI2_Pin|DI0_Pin
                          |DI1_Pin|GPIO_PIN_11, GPIO_PIN_NSEC);

  /*IO attributes management functions */
  HAL_GPIO_ConfigPinAttributes(GPIOC, GPIO_PIN_0|RMII_MDC_Pin|GPIO_PIN_2|GPIO_PIN_3
                          |RMII_RXD0_Pin|RMII_RXD1_Pin|GPIO_PIN_6|GPIO_PIN_7
                          |GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_NSEC);

  /*IO attributes management functions */
  HAL_GPIO_ConfigPinAttributes(GPIOA, RMII_REF_CLK_Pin|RMII_MDIO_Pin|VBUS_SENSE_Pin|GPIO_PIN_6
                          |RMII_CRS_DV_Pin|USB_FS_N_Pin|USB_FS_P_Pin, GPIO_PIN_NSEC);

  /*IO attributes management functions */
  HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_1|RMII_TXD1_Pin|ARD_D1_TX_Pin|ARD_D0_RX_Pin
                          |I2C4_SCL_Pin|I2C4_SDA_Pin, GPIO_PIN_NSEC);

  /*IO attributes management functions */
  HAL_GPIO_ConfigPinAttributes(GPIOG, DI7_Pin|EN_AMPLIFIER_Pin|RMII_TXT_EN_Pin|RMI_TXD0_Pin
                          |DEVADDR0_Pin, GPIO_PIN_NSEC);

  /*IO attributes management functions */
  HAL_GPIO_ConfigPinAttributes(GPIOE, DEVADDR3_Pin|DEVADDR1_Pin|DEVADDR2_Pin, GPIO_PIN_NSEC);

  /*IO attributes management functions */
  HAL_GPIO_ConfigPinAttributes(GPIOD, DI5_Pin|DI6_Pin, GPIO_PIN_NSEC);

  /*IO attributes management functions needed for BSP*/
  HAL_GPIO_ConfigPinAttributes(GPIOG, GPIO_PIN_4, GPIO_PIN_NSEC);
  HAL_GPIO_ConfigPinAttributes(GPIOB, GPIO_PIN_0, GPIO_PIN_NSEC);
  HAL_GPIO_ConfigPinAttributes(GPIOC, GPIO_PIN_13, GPIO_PIN_NSEC);
  HAL_GPIO_ConfigPinAttributes(GPIOD, GPIO_PIN_9|GPIO_PIN_8, GPIO_PIN_NSEC);
  HAL_GPIO_ConfigPinAttributes(GPIOF, GPIO_PIN_4, GPIO_PIN_NSEC);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * @brief  Configure the Secure MPU for the EDATA1 high-cycle data area.
  *
  * The EDATA1 region (Bank 1 sectors 120-127) is accessed via the Secure AHB
  * alias at 0x0D000000 (FLASH_EDATA_BASE_S). This alias is stable and does NOT
  * change with SWAP_BANK, making it safe for persistent calibration storage.
  *
  * MPU must mark this region as non-cacheable because:
  * - EDATA uses 6-bit ECC on 16-bit words (not 9-bit ECC on 128-bit words)
  * - The AHB read buffer caches 128-bit words; caching would corrupt ECC
  * [RM0481 §7.3.2: "For regions where caching is not practical (OTP, RO, data
  *  area), MPU must be used to disable local cacheability."]
  *
  * This function MUST be called before EE_Init() / EEPROM emulation access.
  *
  * @retval None
  */
static void MPU_Config_EDATA(void)
{
  MPU_Region_InitTypeDef     MPU_InitStruct      = {0};
  MPU_Attributes_InitTypeDef MPU_AttributesInit  = {0};

  /* Disable MPU before reconfiguring */
  HAL_MPU_Disable();

  /* Attribute 0: Normal memory, Non-cacheable, Non-shareable.
   * EDATA is flash memory (not a device), so it must use Normal memory attributes.
   * Caching is disabled because EDATA uses 6-bit ECC on 16-bit words — the AHB
   * read buffer would cache 128-bit words and corrupt the 16-bit ECC granularity.
   * Using Device-nGnRnE would cause bus faults on EDATA reads (Device memory
   * has stricter access rules than Normal memory for flash regions).
   * [RM0481 §7.3.2: "For regions where caching is not practical (OTP, RO, data
   *  area), MPU must be used to disable local cacheability."]
   * ARM_MPU_ATTR: outer=non-cacheable (0x4), inner=non-cacheable (0x4) */
  MPU_AttributesInit.Number     = MPU_ATTRIBUTES_NUMBER0;
  MPU_AttributesInit.Attributes = ARM_MPU_ATTR(ARM_MPU_ATTR_NON_CACHEABLE, ARM_MPU_ATTR_NON_CACHEABLE);
  HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);

  /* Region 0: EDATA1 — Bank 1 high-cycle data (Secure alias 0x0D000000)
   * Base:  0x0D000000 = FLASH_EDATA_BASE_S (stm32h563xx.h line 2117)
   * Limit: 0x0D00BFFF = base + 48KB - 1 (8 sectors x 6KB)
   * [RM0481 §7.3.10 lines 913-915: "data are accessible from 0x0900_0000 to
   *  0x0900_BFFF for Bank 1" — same 48KB size via Secure alias at 0x0D000000]
   * Permissions: Privileged read/write only (Secure firmware only)
   * Execute Never: EDATA is data, not code */
  MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
  MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress      = 0x0D000000U;
  MPU_InitStruct.LimitAddress     = 0x0D017FFFU;  /* 96KB: 16 sectors x 6KB, covers all EDATA */
  MPU_InitStruct.AttributesIndex  = MPU_ATTRIBUTES_NUMBER0;
  MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RW;
  MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Re-enable MPU with privileged default map for background regions */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

#include "bsp.h"

#include "main.h"
#include "stm32h5xx_hal.h"

void SystemClock_Config(void);

extern uint32_t __eth_dma_start;

static void MPU_Config(void)
{
    MPU_Region_InitTypeDef     MPU_InitStruct     = {0};
    MPU_Attributes_InitTypeDef MPU_AttributesInit = {0};

    /* Disables the MPU */
    HAL_MPU_Disable();

    /* Configure the MPU attributes for Ethernet Buffers */
    /* Attribute 0: Normal, Non-Cacheable */
    MPU_AttributesInit.Number     = MPU_ATTRIBUTES_NUMBER0;
    MPU_AttributesInit.Attributes = MPU_NOT_CACHEABLE;
    HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);

    /* Configure the MPU region */
    MPU_InitStruct.Enable      = MPU_REGION_ENABLE;
    MPU_InitStruct.Number      = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = (uint32_t)&__eth_dma_start;
    MPU_InitStruct.LimitAddress =
        (uint32_t)&__eth_dma_start + 32 * 1024 - 1; /* 32KB region */
    MPU_InitStruct.AttributesIndex  = MPU_ATTRIBUTES_NUMBER0;
    MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RW;
    MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_OUTER_SHAREABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /* Enables the MPU */
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

bsp_error_t BSP_Init()
{
    /* Reset of all peripherals, Initializes the Flash interface and the
     * Systick. */
    HAL_Init();

    /* Configure MPU for Ethernet DMA */
    MPU_Config();

    SystemClock_Config();
    MX_GTZC_NS_Init();

    /* Initialize all configured peripherals */
    MX_GPDMA1_Init();
    MX_GPIO_Init();
    MX_I2C3_Init();
    MX_LPUART1_UART_Init();
    MX_ADC1_Init();

    BSP_LED_Init(LED_GREEN);
    BSP_LED_Init(LED_YELLOW);
    BSP_LED_Init(LED_RED);

    /* Initialize USER push-button, will be used to trigger an interrupt each
     * time it's pressed.*/
    BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

    BspCOMInit.BaudRate   = 115200;
    BspCOMInit.WordLength = COM_WORDLENGTH_8B;
    BspCOMInit.StopBits   = COM_STOPBITS_1;
    BspCOMInit.Parity     = COM_PARITY_NONE;
    BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
    if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
    {
        Error_Handler();
    }

    MX_ETH_Init();
    MX_USB_HCD_Init();

    return BSP_OK;
}

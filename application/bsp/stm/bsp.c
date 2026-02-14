#include "bsp.h"

#include <string.h>

#include "adc_filter.h"
#include "main.h"
#include "stm32h5xx_hal.h"

/* External declarations */
void                     SystemClock_Config(void);
extern uint32_t          __eth_dma_start;
extern TIM_HandleTypeDef htim1;
extern I2C_HandleTypeDef hi2c3;

/* Note: hadc1, Node_GPDMA1_Channel0, List_GPDMA1_Channel0, and
 * handle_GPDMA1_Channel0 are declared extern in main.h */

/*============================================================================*/
/*                          ADC1 Private Variables                            */
/*============================================================================*/

/** @brief DMA buffer for ADC1 conversion results */
static uint32_t adc1_dma_buffer[BSP_ADC1_NUM_CHANNELS];

/** @brief Flag indicating ADC1 is running */
static volatile bool adc1_running = false;

/** @brief Flag indicating a complete conversion sequence is available */
static volatile bool adc1_conversion_complete = false;

/** @brief Flag indicating ADC1 error occurred (for restart) */
static volatile bool adc1_error_occurred = false;

/** @brief ADC1 error code for debugging */
static volatile uint32_t adc1_last_error = 0;

/*============================================================================*/
/*                     Filtered ADC Private Variables                         */
/*============================================================================*/

/** @brief Filter context for all ADC channels */
static adc_filter_context_t g_adc_filter_ctx;

/** @brief Filtered output values for all channels (continuously updated) */
static volatile float32_t g_filtered_values[BSP_ADC1_NUM_CHANNELS];

/** @brief Sample counter since filter initialization (for settling detection)
 */
static volatile uint32_t g_filter_sample_count = 0;

/** @brief Flag indicating filter subsystem is initialized and running */
static volatile bool g_filter_initialized = false;

/*============================================================================*/
/*                     I2C based Digital output                               */
/*============================================================================*/

/*============================================================================*/
/*                          ADC1 DMA Callbacks                                */
/*============================================================================*/

/**
 * @brief ADC conversion complete callback (called by HAL from DMA IRQ)
 * @param hadc ADC handle
 *
 * This callback is invoked by the HAL when a DMA transfer completes.
 * In continuous filtering mode, it always processes each sample through
 * the 12-stage biquad filter chain in the ISR context (~12µs per call).
 *
 * The filtered values are continuously updated and can be read at any time
 * via BSP_ADC1_GetFilteredValue() with instant response.
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        adc1_conversion_complete = true;

        /* Continuous filtering: always process samples when filter is
         * initialized */
        if (g_filter_initialized)
        {
            /* Convert and filter each channel */
            for (uint8_t ch = 0; ch < BSP_ADC1_NUM_CHANNELS; ch++)
            {
                /* Convert 12-bit ADC value to normalized float (0.0 to 1.0) */
                float32_t input = (float32_t)adc1_dma_buffer[ch] / 4095.0f;

                /* Apply 12-stage biquad filter */
                g_filtered_values[ch] =
                    adc_filter_process_sample(&g_adc_filter_ctx, ch, input);
            }

            /* Increment sample counter (for settling detection) */
            g_filter_sample_count++;
        }
    }
}

/**
 * @brief ADC error callback (called by HAL when ADC error occurs)
 * @param hadc ADC handle
 *
 * This callback is invoked when an ADC error occurs (overrun, DMA error, etc.)
 * It sets a flag that can be checked to restart the ADC.
 */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        adc1_error_occurred = true;
        adc1_last_error     = hadc->ErrorCode;
        adc1_running        = false;
    }
}

/*============================================================================*/
/*                          MPU Configuration                                 */
/*============================================================================*/

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

/*============================================================================*/
/*                          BSP Initialization                                */
/*============================================================================*/

bsp_error_t BSP_Init(void)
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
    MX_LPUART1_UART_Init();
    MX_TIM1_Init();
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

    /* Initialize ADC filter subsystem */
    BSP_ADC1_FilterInit();

    HAL_TIM_OC_Start(&htim1, TIM_CHANNEL_1);
    BSP_ADC1_Start();

    return BSP_OK;
}

/*============================================================================*/
/*                          ADC1 Functions                                    */
/*============================================================================*/

bsp_error_t BSP_ADC1_Start(void)
{
    bsp_error_t         ret         = BSP_OK;
    DMA_NodeConfTypeDef node_config = {0};

    /* Check if already running */
    if (adc1_running)
    {
        ret = BSP_BUSY;
    }
    else
    {
        /* Clear the conversion complete flag */
        adc1_conversion_complete = false;

        /* Configure DMA node for ADC1 */
        node_config.NodeType             = DMA_GPDMA_LINEAR_NODE;
        node_config.Init.Request         = GPDMA1_REQUEST_ADC1;
        node_config.Init.BlkHWRequest    = DMA_BREQ_SINGLE_BURST;
        node_config.Init.Direction       = DMA_PERIPH_TO_MEMORY;
        node_config.Init.SrcInc          = DMA_SINC_FIXED;
        node_config.Init.DestInc         = DMA_DINC_INCREMENTED;
        node_config.Init.SrcDataWidth    = DMA_SRC_DATAWIDTH_WORD;
        node_config.Init.DestDataWidth   = DMA_DEST_DATAWIDTH_WORD;
        node_config.Init.SrcBurstLength  = 1;
        node_config.Init.DestBurstLength = 1;
        node_config.Init.TransferAllocatedPort =
            DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT1;
        node_config.Init.TransferEventMode          = DMA_TCEM_BLOCK_TRANSFER;
        node_config.Init.Mode                       = DMA_NORMAL;
        node_config.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
        node_config.DataHandlingConfig.DataAlignment =
            DMA_DATA_RIGHTALIGN_ZEROPADDED;
        node_config.TriggerConfig.TriggerMode        = DMA_TRIGM_BLOCK_TRANSFER;
        node_config.TriggerConfig.TriggerPolarity    = DMA_TRIG_POLARITY_MASKED;
        node_config.TriggerConfig.TriggerSelection   = 0;
        node_config.RepeatBlockConfig.RepeatCount    = 1;
        node_config.RepeatBlockConfig.SrcAddrOffset  = 0;
        node_config.RepeatBlockConfig.DestAddrOffset = 0;
        node_config.RepeatBlockConfig.BlkSrcAddrOffset  = 0;
        node_config.RepeatBlockConfig.BlkDestAddrOffset = 0;
        node_config.SrcAddress                          = (uint32_t)&ADC1->DR;
        node_config.DstAddress = (uint32_t)adc1_dma_buffer;
        node_config.DataSize   = BSP_ADC1_NUM_CHANNELS * sizeof(uint32_t);

        /* Build the DMA node from configuration */
        if (HAL_DMAEx_List_BuildNode(&node_config, &Node_GPDMA1_Channel0) !=
            HAL_OK)
        {
            ret = BSP_ERROR;
        }
    }

    /* Reset the queue before use */
    if (ret == BSP_OK)
    {
        if (HAL_DMAEx_List_ResetQ(&List_GPDMA1_Channel0) != HAL_OK)
        {
            ret = BSP_ERROR;
        }
    }

    /* Insert node into the queue */
    if (ret == BSP_OK)
    {
        if (HAL_DMAEx_List_InsertNode_Tail(&List_GPDMA1_Channel0,
                                           &Node_GPDMA1_Channel0) != HAL_OK)
        {
            ret = BSP_ERROR;
        }
    }

    /* Make the list circular for continuous conversion */
    if (ret == BSP_OK)
    {
        if (HAL_DMAEx_List_SetCircularMode(&List_GPDMA1_Channel0) != HAL_OK)
        {
            ret = BSP_ERROR;
        }
    }

    /* Initialize the DMA handle for linked list mode */
    if (ret == BSP_OK)
    {
        handle_GPDMA1_Channel0.Instance = GPDMA1_Channel0;
        handle_GPDMA1_Channel0.InitLinkedList.Priority =
            DMA_LOW_PRIORITY_LOW_WEIGHT;
        handle_GPDMA1_Channel0.InitLinkedList.LinkStepMode =
            DMA_LSM_FULL_EXECUTION;
        handle_GPDMA1_Channel0.InitLinkedList.LinkAllocatedPort =
            DMA_LINK_ALLOCATED_PORT0;
        handle_GPDMA1_Channel0.InitLinkedList.TransferEventMode =
            DMA_TCEM_BLOCK_TRANSFER;
        handle_GPDMA1_Channel0.InitLinkedList.LinkedListMode =
            DMA_LINKEDLIST_CIRCULAR;

        if (HAL_DMAEx_List_Init(&handle_GPDMA1_Channel0) != HAL_OK)
        {
            ret = BSP_ERROR;
        }
    }

    /* Link the queue to the DMA handle */
    if (ret == BSP_OK)
    {
        if (HAL_DMAEx_List_LinkQ(&handle_GPDMA1_Channel0,
                                 &List_GPDMA1_Channel0) != HAL_OK)
        {
            ret = BSP_ERROR;
        }
    }

    /* Link DMA handle to ADC handle */
    if (ret == BSP_OK)
    {
        __HAL_LINKDMA(&hadc1, DMA_Handle, handle_GPDMA1_Channel0);

        /* Run ADC calibration */
        if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
        {
            ret = BSP_ERROR;
        }
    }

    /* Start ADC with DMA */
    if (ret == BSP_OK)
    {
        if (HAL_ADC_Start_DMA(&hadc1, adc1_dma_buffer, BSP_ADC1_NUM_CHANNELS) !=
            HAL_OK)
        {
            ret = BSP_ERROR;
        }
        else
        {
            adc1_running = true;
        }
    }

    return ret;
}

bsp_error_t BSP_ADC1_Stop(void)
{
    bsp_error_t ret = BSP_OK;

    if (adc1_running)
    {
        /* Stop ADC DMA */
        if (HAL_ADC_Stop_DMA(&hadc1) != HAL_OK)
        {
            ret = BSP_ERROR;
        }
        else
        {
            adc1_running             = false;
            adc1_conversion_complete = false;
        }
    }
    /* If not running, return BSP_OK (already stopped) */

    return ret;
}

bool BSP_ADC1_IsConversionComplete(void)
{
    bool result = false;

    if (adc1_running && adc1_conversion_complete)
    {
        /* Clear the flag so caller can detect next conversion */
        adc1_conversion_complete = false;
        result                   = true;
    }

    return result;
}

bsp_error_t BSP_ADC1_GetResults(const uint32_t **results)
{
    bsp_error_t ret = BSP_OK;

    if (results == NULL)
    {
        ret = BSP_INVALID_ARG;
    }
    else if (!adc1_running)
    {
        *results = NULL;
        ret      = BSP_ERROR;
    }
    else
    {
        *results = adc1_dma_buffer;
    }

    return ret;
}

bsp_error_t BSP_ADC1_GetResultsCopy(uint32_t *buffer)
{
    bsp_error_t ret = BSP_OK;

    if (buffer == NULL)
    {
        ret = BSP_INVALID_ARG;
    }
    else if (!adc1_running)
    {
        ret = BSP_ERROR;
    }
    else
    {
        /* Disable interrupts briefly to get consistent snapshot */
        __disable_irq();
        memcpy(buffer, adc1_dma_buffer, sizeof(adc1_dma_buffer));
        __enable_irq();
    }

    return ret;
}

bool BSP_ADC1_HasError(void) { return adc1_error_occurred; }

uint32_t BSP_ADC1_GetLastError(void) { return adc1_last_error; }

bsp_error_t BSP_ADC1_Restart(void)
{
    bsp_error_t ret = BSP_OK;

    /* Stop ADC if running */
    if (adc1_running)
    {
        (void)HAL_ADC_Stop_DMA(&hadc1);
        adc1_running = false;
    }

    /* Clear error flags */
    adc1_error_occurred = false;
    adc1_last_error     = 0;

    /* De-initialize and re-initialize the DMA handle */
    (void)HAL_DMAEx_List_DeInit(&handle_GPDMA1_Channel0);

    /* Restart ADC */
    ret = BSP_ADC1_Start();

    return ret;
}

bool BSP_ADC1_CheckAndRestart(void)
{
    bool restarted = false;

    if (adc1_error_occurred || !adc1_running)
    {
        if (BSP_ADC1_Restart() == BSP_OK)
        {
            restarted = true;
        }
    }

    return restarted;
}

/*============================================================================*/
/*                     Filtered ADC1 Functions (Continuous Mode)              */
/*============================================================================*/

void BSP_ADC1_FilterInit(void)
{
    if (!g_filter_initialized)
    {
        /* Initialize the filter context for all channels */
        adc_filter_init(&g_adc_filter_ctx);

        /* Clear filtered values */
        for (uint8_t ch = 0; ch < BSP_ADC1_NUM_CHANNELS; ch++)
        {
            g_filtered_values[ch] = 0.0f;
        }

        /* Reset sample counter */
        g_filter_sample_count = 0;

        /* Mark as initialized - ISR will start processing samples */
        g_filter_initialized = true;
    }
}

bsp_error_t BSP_ADC1_GetFilteredValue(uint8_t channel, float32_t *value)
{
    bsp_error_t ret = BSP_OK;

    /* Validate parameters */
    if (channel >= BSP_ADC1_NUM_CHANNELS || value == NULL)
    {
        ret = BSP_INVALID_ARG;
    }
    /* Check if filter is initialized */
    else if (!g_filter_initialized)
    {
        ret = BSP_ERROR;
    }
    else
    {
        /* Instant response: just return the current filtered value */
        *value = g_filtered_values[channel];
    }

    return ret;
}

bsp_error_t BSP_ADC1_GetFilteredValuesAll(float32_t *values)
{
    bsp_error_t ret = BSP_OK;

    /* Validate parameters */
    if (values == NULL)
    {
        ret = BSP_INVALID_ARG;
    }
    /* Check if filter is initialized */
    else if (!g_filter_initialized)
    {
        ret = BSP_ERROR;
    }
    else
    {
        /* Instant response: copy all current filtered values */
        /* Disable interrupts briefly to get consistent snapshot */
        __disable_irq();
        for (uint8_t ch = 0; ch < BSP_ADC1_NUM_CHANNELS; ch++)
        {
            values[ch] = g_filtered_values[ch];
        }
        __enable_irq();
    }

    return ret;
}

bool BSP_ADC1_IsFilterSettled(void)
{
    return g_filter_initialized &&
           (g_filter_sample_count >= BSP_ADC1_FILTER_SETTLING_SAMPLES);
}

uint32_t BSP_ADC1_GetFilterSampleCount(void) { return g_filter_sample_count; }

bsp_error_t BSP_I2CDO_init()
{
    bsp_error_t ret = BSP_OK;

    MX_I2C3_Init();

    // Set initial state to all low and write to expanders
    ret = BSP_I2CDO_Write(0x0000U);

    return ret;
}

bsp_error_t BSP_I2CDO_Write(uint16_t value)
{
    HAL_StatusTypeDef status;
    uint8_t           output_byte;
    bsp_error_t       ret = BSP_OK;

    // Write lower 8 bits to PCF8574
    output_byte = (uint8_t)(value & 0xFFU);
    status      = HAL_I2C_Master_Transmit(&hi2c3, BSP_I2CDO_PCF8574_ADDR,
                                          &output_byte, 1, BSP_I2CDO_TIMEOUT);
    if (status != HAL_OK)
    {
        if (status == HAL_TIMEOUT)
        {
            ret = BSP_TIMEOUT;
        }
        else
        {
            ret = BSP_ERROR;
        }
    }

    // Write upper 8 bits to PCF8574A
    if (ret == BSP_OK)
    {
        output_byte = (uint8_t)((value >> 8U) & 0xFFU);
        status      = HAL_I2C_Master_Transmit(&hi2c3, BSP_I2CDO_PCF8574A_ADDR,
                                              &output_byte, 1, BSP_I2CDO_TIMEOUT);
        if (status != HAL_OK)
        {
            if (status == HAL_TIMEOUT)
            {
                ret = BSP_TIMEOUT;
            }
            else
            {
                ret = BSP_ERROR;
            }
        }
    }

    return ret;
}

bsp_error_t BSP_I2CDO_Read(uint16_t *value)
{
    HAL_StatusTypeDef status;
    uint8_t           read_byte_pcf8574;
    uint8_t           read_byte_pcf8574a;
    bsp_error_t       ret = BSP_OK;

    if (value == NULL)
    {
        return BSP_INVALID_ARG;
    }

    // Read from PCF8574
    status = HAL_I2C_Master_Receive(&hi2c3, BSP_I2CDO_PCF8574_ADDR,
                                    &read_byte_pcf8574, 1, BSP_I2CDO_TIMEOUT);
    if (status != HAL_OK)
    {
        if (status == HAL_TIMEOUT)
        {
            ret = BSP_TIMEOUT;
        }
        else
        {
            ret = BSP_ERROR;
        }
    }

    // Read from PCF8574A
    if (ret == BSP_OK)
    {
        status =
            HAL_I2C_Master_Receive(&hi2c3, BSP_I2CDO_PCF8574A_ADDR,
                                   &read_byte_pcf8574a, 1, BSP_I2CDO_TIMEOUT);
        if (status != HAL_OK)
        {
            if (status == HAL_TIMEOUT)
            {
                ret = BSP_TIMEOUT;
            }
            else
            {
                ret = BSP_ERROR;
            }
        }
    }

    if (ret == BSP_OK)
    {
        *value =
            (uint16_t)read_byte_pcf8574 | ((uint16_t)read_byte_pcf8574a << 8U);
    }

    return ret;
}

bsp_error_t BSP_GPIODI_Read(uint32_t channel, uint32_t *pVal)
{
    bsp_error_t retval;

    if ((channel <= BSP_GPIODI_INDEX_7) && (NULL != pVal))
    {
        GPIO_PinState pinValue;

        retval = BSP_OK;
        switch (channel)
        {
            case BSP_GPIODI_INDEX_0:
                pinValue = HAL_GPIO_ReadPin(DI0_GPIO_Port, DI0_Pin);
                break;
            case BSP_GPIODI_INDEX_1:
                pinValue = HAL_GPIO_ReadPin(DI1_GPIO_Port, DI1_Pin);
                break;
            case BSP_GPIODI_INDEX_2:
                pinValue = HAL_GPIO_ReadPin(DI2_GPIO_Port, DI2_Pin);
                break;
            case BSP_GPIODI_INDEX_3:
                pinValue = HAL_GPIO_ReadPin(DI3_GPIO_Port, DI3_Pin);
                break;
            case BSP_GPIODI_INDEX_4:
                pinValue = HAL_GPIO_ReadPin(DI4_GPIO_Port, DI4_Pin);
                break;
            case BSP_GPIODI_INDEX_5:
                pinValue = HAL_GPIO_ReadPin(DI5_GPIO_Port, DI5_Pin);
                break;
            case BSP_GPIODI_INDEX_6:
                pinValue = HAL_GPIO_ReadPin(DI6_GPIO_Port, DI6_Pin);
                break;
            case BSP_GPIODI_INDEX_7:
                pinValue = HAL_GPIO_ReadPin(DI7_GPIO_Port, DI7_Pin);
                break;
            default:
                pinValue = GPIO_PIN_RESET;
        }
        *pVal = pinValue;
    }
    else
    {
        retval = BSP_ERROR;
    }

    return retval;
}

uint8_t BSP_GetDeviceAddress(void)
{
    uint8_t address = 0U;

    /*
     * Read DEVADDR pins - they have internal pull-ups, so:
     * - Open/floating pin reads HIGH (GPIO_PIN_SET) -> bit = 0
     * - Grounded pin reads LOW (GPIO_PIN_RESET) -> bit = 1
     *
     * This allows setting address by grounding pins (active-low logic).
     */

    /* DEVADDR0 - Bit 0 */
    if (HAL_GPIO_ReadPin(DEVADDR0_GPIO_Port, DEVADDR0_Pin) == GPIO_PIN_RESET)
    {
        address |= (1U << 0U);
    }

    /* DEVADDR1 - Bit 1 */
    if (HAL_GPIO_ReadPin(DEVADDR1_GPIO_Port, DEVADDR1_Pin) == GPIO_PIN_RESET)
    {
        address |= (1U << 1U);
    }

    /* DEVADDR2 - Bit 2 */
    if (HAL_GPIO_ReadPin(DEVADDR2_GPIO_Port, DEVADDR2_Pin) == GPIO_PIN_RESET)
    {
        address |= (1U << 2U);
    }

    /* DEVADDR3 - Bit 3 */
    if (HAL_GPIO_ReadPin(DEVADDR3_GPIO_Port, DEVADDR3_Pin) == GPIO_PIN_RESET)
    {
        address |= (1U << 3U);
    }

    return address;
}
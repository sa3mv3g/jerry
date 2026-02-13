#ifndef BSP_H
#define BSP_H

#include <stdbool.h>
#include <stdint.h>

#include "arm_math_types.h"
#include "stm32h5xx_nucleo.h"

/**
 * @brief BSP Error Codes
 */
typedef int bsp_error_t;

#define BSP_OK          ((bsp_error_t)0) /*!< Operation successful */
#define BSP_ERROR       ((bsp_error_t)1) /*!< General/Unspecified error */
#define BSP_BUSY        ((bsp_error_t)2) /*!< Resource is busy */
#define BSP_TIMEOUT     ((bsp_error_t)3) /*!< Operation timed out */
#define BSP_INVALID_ARG ((bsp_error_t)4) /*!< Invalid argument provided */

/**
 * @defgroup BSP_I2C_Digital_Output_Masks I2C Digital Output Masks
 * @brief Bit masks for individual digital output channels controlled via I2C.
 * @{
 */
#define BSP_I2CDO_MASK_D0           (1U << 0U) /**< Mask for Digital Output 0 */
#define BSP_I2CDO_MASK_D1           (1U << 1U) /**< Mask for Digital Output 1 */
#define BSP_I2CDO_MASK_D2           (1U << 2U) /**< Mask for Digital Output 2 */
#define BSP_I2CDO_MASK_D3           (1U << 3U) /**< Mask for Digital Output 3 */
#define BSP_I2CDO_MASK_D4           (1U << 4U) /**< Mask for Digital Output 4 */
#define BSP_I2CDO_MASK_D5           (1U << 5U) /**< Mask for Digital Output 5 */
#define BSP_I2CDO_MASK_D6           (1U << 6U) /**< Mask for Digital Output 6 */
#define BSP_I2CDO_MASK_D7           (1U << 7U) /**< Mask for Digital Output 7 */
#define BSP_I2CDO_MASK_D8           (1U << 8U) /**< Mask for Digital Output 8 */
#define BSP_I2CDO_MASK_D9           (1U << 9U) /**< Mask for Digital Output 9 */
#define BSP_I2CDO_MASK_D10          (1U << 10U) /**< Mask for Digital Output 10 */
#define BSP_I2CDO_MASK_D11          (1U << 11U) /**< Mask for Digital Output 11 */
#define BSP_I2CDO_MASK_D12          (1U << 12U) /**< Mask for Digital Output 12 */
#define BSP_I2CDO_MASK_D13          (1U << 13U) /**< Mask for Digital Output 13 */
#define BSP_I2CDO_MASK_D14          (1U << 14U) /**< Mask for Digital Output 14 */
#define BSP_I2CDO_MASK_D15          (1U << 15U) /**< Mask for Digital Output 15 */
#define BSP_I2CDO_CONSTRUCT_MASK(x) (1U << x)
/** @} */

/**
 * @defgroup BSP_I2C_Digital_Output_Addresses I2C Digital Output Addresses
 * @brief I2C addresses and timeout for PCF8574 and PCF8574A expanders.
 * @{
 */
#define BSP_I2CDO_PCF8574_ADDR                                              \
    (0x20 << 1) /**< 7-bit I2C address of PCF8574 (0x20), shifted left by 1 \
                   for HAL functions. */
#define BSP_I2CDO_PCF8574A_ADDR                                              \
    (0x21 << 1) /**< 7-bit I2C address of PCF8574A (0x21), shifted left by 1 \
                   for HAL functions. */
#define BSP_I2CDO_TIMEOUT                               \
    100 /**< I2C communication timeout in milliseconds. \
         */
/** @} */

/**
 * @brief Number of ADC1 channels configured
 */
#define BSP_ADC1_NUM_CHANNELS 6U

/**
 * @brief Global configuration structure for BSP COM port initialization.
 *
 * This structure holds the configuration parameters (Baud Rate, Word Length,
 * Stop Bits, Parity, etc.) used to initialize the COM port (UART) functionality
 * provided by the Board Support Package.
 */
extern COM_InitTypeDef BspCOMInit;

/**
 * @brief Initializes the Board Support Package (BSP).
 *
 * This function initializes the underlying hardware resources, drivers, and
 * peripherals abstraction layer required by the application.
 *
 * @return bsp_error_t BSP_OK if initialization is successful, otherwise an
 * error code.
 */
bsp_error_t BSP_Init(void);

/**
 * @defgroup BSP_ADC1 ADC1 Functions
 * @brief ADC1 control and data acquisition functions
 * @{
 */

/**
 * @brief Starts ADC1 conversion with DMA.
 *
 * This function starts the ADC1 in continuous conversion mode with DMA
 * transfer. The ADC will continuously convert all configured channels
 * and store results in an internal buffer via DMA.
 *
 * @note Call this function once after BSP_Init() to start ADC conversions.
 * @note The ADC runs in circular DMA mode, so conversions continue
 *       indefinitely until BSP_ADC1_Stop() is called.
 *
 * @return bsp_error_t BSP_OK if started successfully, BSP_ERROR otherwise.
 */
bsp_error_t BSP_ADC1_Start(void);

/**
 * @brief Stops ADC1 conversion.
 *
 * This function stops the ADC1 conversion and DMA transfer.
 *
 * @return bsp_error_t BSP_OK if stopped successfully, BSP_ERROR otherwise.
 */
bsp_error_t BSP_ADC1_Stop(void);

/**
 * @brief Checks if ADC1 conversion sequence is complete.
 *
 * In circular DMA mode, this function checks if at least one complete
 * sequence of all channels has been converted since the last call to
 * BSP_ADC1_Start() or since the last time this function returned true.
 *
 * @return true if a complete conversion sequence is available, false otherwise.
 */
bool BSP_ADC1_IsConversionComplete(void);

/**
 * @brief Gets the ADC1 conversion results.
 *
 * This function returns a pointer to the internal buffer containing
 * the latest ADC conversion results for all channels.
 *
 * @param[out] results Pointer to store the address of the results buffer.
 *                     The buffer contains BSP_ADC1_NUM_CHANNELS values.
 *                     Each value is a 12-bit right-aligned ADC reading.
 *
 * @return bsp_error_t BSP_OK if results are valid, BSP_INVALID_ARG if
 *         results pointer is NULL, BSP_ERROR if ADC is not running.
 *
 * @note The returned pointer points to the DMA buffer which is continuously
 *       updated. For consistent readings, either:
 *       - Use BSP_ADC1_GetResultsCopy() for a snapshot, or
 *       - Disable interrupts briefly while reading all values.
 */
bsp_error_t BSP_ADC1_GetResults(const uint32_t **results);

/**
 * @brief Copies ADC1 conversion results to a user buffer.
 *
 * This function copies the current ADC conversion results to a user-provided
 * buffer. This provides a consistent snapshot of all channel values.
 *
 * @param[out] buffer Pointer to user buffer to store results.
 *                    Must have space for at least BSP_ADC1_NUM_CHANNELS values.
 *
 * @return bsp_error_t BSP_OK if copy successful, BSP_INVALID_ARG if buffer
 *         is NULL, BSP_ERROR if ADC is not running.
 */
bsp_error_t BSP_ADC1_GetResultsCopy(uint32_t *buffer);

/**
 * @brief Check if an ADC error has occurred.
 *
 * @return true if an error has occurred, false otherwise.
 */
bool BSP_ADC1_HasError(void);

/**
 * @brief Get the last ADC error code.
 *
 * @return HAL ADC error code from the last error.
 */
uint32_t BSP_ADC1_GetLastError(void);

/**
 * @brief Restart ADC1 after an error or stop.
 *
 * This function stops the ADC if running, clears error flags,
 * and restarts the ADC with DMA.
 *
 * @return bsp_error_t BSP_OK if restart successful, BSP_ERROR otherwise.
 */
bsp_error_t BSP_ADC1_Restart(void);

/**
 * @brief Check for ADC errors and restart if needed.
 *
 * This function checks if an ADC error has occurred or if the ADC
 * has stopped, and automatically restarts it.
 *
 * @return true if ADC was restarted, false if no restart was needed.
 */
bool BSP_ADC1_CheckAndRestart(void);

/** @} */ /* End of BSP_ADC1 group */

/**
 * @defgroup BSP_ADC1_Filtered Filtered ADC1 Functions (Continuous Mode)
 * @brief Continuously filtered ADC acquisition functions
 *
 * These functions provide filtered ADC readings using a 12-stage biquad
 * cascade filter (4th order Butterworth LPF + 10 notch filters for 50Hz
 * mains rejection). The filter runs continuously at 10kHz, processing
 * every ADC sample in the DMA ISR.
 *
 * **Key characteristics:**
 * - **Instant response**: GetFilteredValue() returns immediately
 * - **Continuous operation**: Filter always running, values always current
 * - **~2-5ms signal delay**: Actual group delay through filter chain
 * - **~102ms initial settling**: One-time after power-on/reset
 *
 * @{
 */

/** Number of samples required for filter settling (95% settling) */
#define BSP_ADC1_FILTER_SETTLING_SAMPLES 1024U

/**
 * @brief Initialize the ADC filter subsystem.
 *
 * This function initializes the digital filter for all ADC channels.
 * After initialization, the filter runs continuously, processing every
 * ADC sample in the DMA ISR.
 *
 * @note This is automatically called by BSP_Init().
 * @note After calling this, wait ~102ms for filter to settle before
 *       reading values (use BSP_ADC1_IsFilterSettled() to check).
 */
void BSP_ADC1_FilterInit(void);

/**
 * @brief Get a filtered ADC value for a single channel (instant response).
 *
 * Returns the current filtered value for the specified channel.
 * The filter runs continuously in the background, so this function
 * returns immediately with the latest filtered value.
 *
 * @param[in]  channel Channel index (0 to BSP_ADC1_NUM_CHANNELS-1).
 * @param[out] value   Pointer to store the filtered value (0.0 to 1.0
 * normalized).
 *
 * @return bsp_error_t BSP_OK if successful, BSP_INVALID_ARG if parameters
 *         are invalid, BSP_ERROR if filter is not initialized.
 *
 * @note This function returns instantly (no blocking).
 * @note The returned value is normalized to 0.0-1.0 range (0 = 0V, 1.0 = VREF).
 * @note Check BSP_ADC1_IsFilterSettled() to ensure filter has settled
 *       after power-on before trusting the values.
 */
bsp_error_t BSP_ADC1_GetFilteredValue(uint8_t channel, float32_t *value);

/**
 * @brief Get filtered ADC values for all channels (instant response).
 *
 * Returns the current filtered values for all channels as an atomic snapshot.
 *
 * @param[out] values Array to store filtered values for all channels.
 *                    Must have space for BSP_ADC1_NUM_CHANNELS float32_t
 * values.
 *
 * @return bsp_error_t BSP_OK if successful, BSP_INVALID_ARG if values is NULL,
 *         BSP_ERROR if filter is not initialized.
 *
 * @note This function returns instantly (no blocking).
 * @note Interrupts are briefly disabled to ensure consistent snapshot.
 */
bsp_error_t BSP_ADC1_GetFilteredValuesAll(float32_t *values);

/**
 * @brief Check if filter has settled after initialization.
 *
 * After power-on or filter reset, the filter needs ~1024 samples (~102ms)
 * to settle. This function returns true once enough samples have been
 * processed for the filter output to be valid.
 *
 * @return true if filter has settled and values are valid, false otherwise.
 */
bool BSP_ADC1_IsFilterSettled(void);

/**
 * @brief Get the current filter sample count.
 *
 * Returns the number of samples processed since filter initialization.
 * Useful for monitoring settling progress after power-on.
 *
 * @return Number of samples processed since initialization.
 */
uint32_t BSP_ADC1_GetFilterSampleCount(void);

/** @} */ /* End of BSP_ADC1_Filtered group */

/**
 * @brief Initializes the I2C Digital Output (PCF8574/PCF8574A) subsystem.
 *
 * This function initializes the I2C expanders to a known state, typically all
 * outputs set to low.
 *
 * @return bsp_error_t BSP_OK if initialization is successful, otherwise an
 * error code.
 */
bsp_error_t BSP_I2CDO_init();

/**
 * @brief Writes a 16-bit value to the I2C Digital Output expanders.
 *
 * The lower 8 bits of the value are sent to PCF8574, and the upper 8 bits
 * are sent to PCF8574A. This function includes checks for device readiness.
 *
 * @param value A 16-bit mask of ::BSP_I2C_Digital_Output_Masks indicating the
 * desired state of the digital outputs.
 * @return bsp_error_t BSP_OK if the write is successful, otherwise an error
 * code.
 */
bsp_error_t BSP_I2CDO_Write(uint16_t value);

/**
 * @brief Reads the current 16-bit state of the I2C Digital Output expanders.
 *
 * This function reads the state from both PCF8574 and PCF8574A and combines
 * them into a single 16-bit value. It includes checks for device readiness.
 *
 * @param value Pointer to a uint16_t where the read value will be stored.
 * @return bsp_error_t BSP_OK if the read is successful, otherwise an error
 * code.
 */
bsp_error_t BSP_I2CDO_Read(uint16_t *value);

#endif  // BSP_H
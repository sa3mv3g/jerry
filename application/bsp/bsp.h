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
 * @param[out] value   Pointer to store the filtered value (0.0 to 1.0 normalized).
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
 *                    Must have space for BSP_ADC1_NUM_CHANNELS float32_t values.
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

#endif  // BSP_H
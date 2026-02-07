#ifndef BSP_H
#define BSP_H

#include <stdbool.h>
#include <stdint.h>

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

#endif  // BSP_H
#ifndef BSP_H
#define BSP_H

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
 * @brief Global configuration structure for BSP COM port initialization.
 *
 * This structure holds the configuration parameters (Baud Rate, Word Length, Stop Bits, Parity, etc.)
 * used to initialize the COM port (UART) functionality provided by the Board Support Package.
 */
extern COM_InitTypeDef BspCOMInit;

/**
 * @brief Initializes the Board Support Package (BSP).
 *
 * This function initializes the underlying hardware resources, drivers, and
 * peripherals abstraction layer required by the application.
 *
 * @return bsp_error_t BSP_OK if initialization is successful, otherwise an error code.
 */
bsp_error_t BSP_Init();

#endif // BSP_H
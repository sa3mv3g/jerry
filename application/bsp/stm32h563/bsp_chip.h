#ifndef BSP_CHIP_H
#define BSP_CHIP_H

/* STM32H563 specific includes */
#include "stm32h5xx_nucleo.h"

/**
 * @brief Global configuration structure for BSP COM port initialization.
 *
 * This structure holds the configuration parameters (Baud Rate, Word Length,
 * Stop Bits, Parity, etc.) used to initialize the COM port (UART) functionality
 * provided by the Board Support Package.
 */
extern COM_InitTypeDef BspCOMInit;

#endif  /* BSP_CHIP_H */

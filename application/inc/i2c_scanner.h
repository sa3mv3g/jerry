/**
 * @file i2c_scanner.h
 * @brief I2C Bus Scanner Utility
 *
 * This utility scans the I2C bus for connected devices and prints their
 * addresses to the console. Useful for debugging and device discovery.
 *
 * @copyright Copyright (c) 2026
 * All rights reserved.
 */

#ifndef I2C_SCANNER_H
#define I2C_SCANNER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    /**
     * @brief Scan the I2C bus for connected devices
     *
     * This function scans all valid 7-bit I2C addresses (0x03 to 0x77)
     * and prints the addresses of devices that respond. The scan uses
     * the BSP I2C functions to probe each address.
     *
     * @note This function should be called after BSP_Init() to ensure
     *       the I2C peripheral is properly initialized.
     * @note The scan may take a few seconds to complete as it probes
     *       all 127 possible addresses.
     * @note Reserved addresses (0x00-0x02, 0x78-0x7F) are skipped.
     *
     * Example output:
     * @code
     * ========================================
     * I2C Bus Scanner
     * ========================================
     * Scanning I2C bus (0x03 to 0x77)...
     *
     * Device found at address 0x20 (PCF8574)
     * Device found at address 0x27 (LCD)
     * Device found at address 0x3C (OLED)
     *
     * Scan complete. Found 3 device(s).
     * ========================================
     * @endcode
     */
    void I2C_ScanBus(void);

#ifdef __cplusplus
}
#endif

#endif /* I2C_SCANNER_H */

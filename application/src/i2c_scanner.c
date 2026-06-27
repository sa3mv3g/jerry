/**
 * @file i2c_scanner.c
 * @brief I2C Bus Scanner Utility Implementation
 *
 * @copyright Copyright (c) 2026
 * All rights reserved.
 */

#include "i2c_scanner.h"

#include <stdio.h>

#include "bsp.h"
#include "bsp_i2c.h"
#include "log.h"

/**
 * @brief I2C address range definitions
 */
#define I2C_SCAN_START_ADDR 0x03U /**< First valid 7-bit I2C address */
#define I2C_SCAN_END_ADDR   0x77U /**< Last valid 7-bit I2C address */
#define I2C_SCAN_TIMEOUT_MS 10U   /**< Timeout for each address probe */

/**
 * @brief Common I2C device identifiers
 *
 * This structure maps known I2C addresses to common device types
 * to provide helpful hints during scanning.
 */
typedef struct
{
    uint8_t     address;
    const char *description;
} I2cDeviceInfo;

/**
 * @brief Known I2C device address mappings
 */
static const I2cDeviceInfo known_devices[] = {
    {0x20, "PCF8574 I/O Expander"},
    {0x21, "PCF8574A I/O Expander"},
    {0x27, "LCD with I2C Backpack"},
    {0x3C, "OLED Display (SSD1306)"},
    {0x3D, "OLED Display (SSD1306)"},
    {0x48, "ADS1115 ADC / TMP102 Temp Sensor"},
    {0x49, "ADS1115 ADC / TMP102 Temp Sensor"},
    {0x4A, "ADS1115 ADC / TMP102 Temp Sensor"},
    {0x4B, "ADS1115 ADC / TMP102 Temp Sensor"},
    {0x50, "EEPROM (24C series)"},
    {0x51, "EEPROM (24C series)"},
    {0x52, "EEPROM (24C series)"},
    {0x53, "EEPROM (24C series) / ADXL345 Accelerometer"},
    {0x57, "EEPROM (24C series)"},
    {0x68, "DS1307 RTC / MPU6050 IMU"},
    {0x69, "MPU6050 IMU"},
    {0x76, "BMP280 / BME280 Pressure Sensor"},
    {0x77, "BMP280 / BME280 Pressure Sensor"},
};

/**
 * @brief Get device description for a known I2C address
 *
 * @param address 7-bit I2C address
 * @return Pointer to description string, or NULL if unknown
 */
static const char *get_device_description(uint8_t address)
{
    for (size_t i = 0; i < sizeof(known_devices) / sizeof(known_devices[0]);
         i++)
    {
        if (known_devices[i].address == address)
        {
            return known_devices[i].description;
        }
    }
    return NULL;
}

void I2C_ScanBus(void)
{
    uint16_t    device_count = 0;
    bsp_error_t result;
    const char *device_desc;

    LOG_INF("\n");
    LOG_INF("========================================\n");
    LOG_INF("I2C Bus Scanner\n");
    LOG_INF("========================================\n");
    LOG_INF("Scanning I2C bus (0x%02X to 0x%02X)...\n", I2C_SCAN_START_ADDR,
            I2C_SCAN_END_ADDR);
    LOG_INF("\n");

    /* Scan all valid 7-bit I2C addresses */
    for (uint8_t addr = I2C_SCAN_START_ADDR; addr <= I2C_SCAN_END_ADDR; addr++)
    {
        /* Convert 7-bit address to 8-bit format (left-shifted) */
        uint8_t addr_8bit = addr << 1;

        /* Try to read from the device */
        result = BSP_I2C_LcdWrite(addr_8bit, NULL, 0, I2C_SCAN_TIMEOUT_MS);

        if (result == BSP_OK)
        {
            device_count++;
            device_desc = get_device_description(addr);

            if (device_desc != NULL)
            {
                LOG_INF("Device found at address 0x%02X (%s)\n", addr,
                        device_desc);
            }
            else
            {
                LOG_INF("Device found at address 0x%02X (Unknown device)\n",
                        addr);
            }
        }
    }

    LOG_INF("\n");
    LOG_INF("Scan complete. Found %u device(s).\n", device_count);
    LOG_INF("========================================\n");
    LOG_INF("\n");
}

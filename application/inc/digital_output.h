/**
 * @file digital_output.h
 * @brief Digital Output (DO) module public interface.
 *
 * This module provides a high-level API for controlling the 16 digital
 * outputs connected via I2C expanders (PCF8574/PCF8574A). It maintains
 * an internal shadow register for efficiency and consistency, and handles
 * updating the LCD display status of each channel.
 *
 * It abstracts the low-level BSP I2C Digital Output driver.
 *
 * @copyright Copyright (c) 2026
 */

#ifndef DIGITAL_OUTPUT_H
#define DIGITAL_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "bsp.h"

#define DIGITAL_OUTPUT_NUM_CHANNELS (16U)

/**
 * @brief Initialize the digital output module.
 *
 * Writes 0x0000 to the I2C expanders (all outputs off), sets the
 * internal shadow register to 0, and updates the LCD for all 16
 * channels to "off".
 *
 * Must be called after BSP_I2C_Init() / BSP_I2CDO_init().
 *
 * @return bsp_error_t BSP_OK on success, error code on I2C failure.
 */
bsp_error_t DigitalOutput_Init(void);

/**
 * @brief Set a single digital output channel.
 *
 * Modifies the shadow register, writes the full 16-bit value to
 * hardware via I2C, and updates the LCD on success.
 *
 * @param channel Channel index (0-15, use BSP_I2CDO_INDEX_Dx macros).
 * @param value   true = ON, false = OFF.
 * @return bsp_error_t BSP_OK on success, BSP_INVALID_ARG if channel
 *         is out of range, or I2C error code on failure.
 */
bsp_error_t DigitalOutput_SetChannel(uint16_t channel, bool value);

/**
 * @brief Get the cached state of a single digital output channel.
 *
 * Reads from the internal shadow register (no I2C transaction).
 *
 * @param channel Channel index (0-15).
 * @param value   Pointer to store the current state.
 * @return bsp_error_t BSP_OK on success, BSP_INVALID_ARG if channel
 *         is out of range or value is NULL.
 */
bsp_error_t DigitalOutput_GetChannel(uint16_t channel, bool *value);

/**
 * @brief Read the actual hardware state of a single channel via I2C.
 *
 * Performs a BSP_I2CDO_Read() and extracts the specified channel bit.
 * Does NOT update the shadow register — this is a diagnostic/
 * verification function only.
 *
 * @param channel Channel index (0-15).
 * @param value   Pointer to store the hardware state.
 * @return bsp_error_t BSP_OK on success, BSP_INVALID_ARG if channel
 *         is out of range or value is NULL, or I2C error code.
 */
bsp_error_t DigitalOutput_ReadHardware(uint16_t channel, bool *value);

/**
 * @brief Write all 16 digital outputs at once.
 *
 * Writes the full 16-bit mask to hardware. Diffs against the shadow
 * register and calls LcdManager_UpdateDigitalOutputStatus() for each
 * channel that changed. Updates the shadow on success.
 *
 * @param mask 16-bit output mask (bit 0 = channel 0, etc.).
 * @return bsp_error_t BSP_OK on success, or I2C error code on failure.
 */
bsp_error_t DigitalOutput_WriteAll(uint16_t mask);

#endif /* DIGITAL_OUTPUT_H */

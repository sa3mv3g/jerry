/**
 * @file test_modbus_callbacks.c
 * @brief Stub implementations of Modbus callbacks for unit testing
 *
 * These are minimal stub implementations that allow the modbus_core.c
 * to link successfully for unit testing purposes.
 *
 * @copyright Copyright (c) 2026
 */

#include "modbus_callbacks.h"

/* ==========================================================================
 * Stub Callback Implementations
 * ========================================================================== */

/**
 * @brief Stub implementation for reading coils
 */
modbus_exception_t modbus_cb_read_coils(uint16_t start_address,
                                        uint16_t quantity,
                                        uint8_t *coil_status) {
  (void)start_address;
  (void)quantity;
  (void)coil_status;
  return MODBUS_EXCEPTION_NONE;
}

/**
 * @brief Stub implementation for reading discrete inputs
 */
modbus_exception_t modbus_cb_read_discrete_inputs(uint16_t start_address,
                                                  uint16_t quantity,
                                                  uint8_t *input_status) {
  (void)start_address;
  (void)quantity;
  (void)input_status;
  return MODBUS_EXCEPTION_NONE;
}

/**
 * @brief Stub implementation for reading holding registers
 */
modbus_exception_t modbus_cb_read_holding_registers(uint16_t start_address,
                                                    uint16_t quantity,
                                                    uint16_t *register_values) {
  (void)start_address;
  (void)quantity;
  (void)register_values;
  return MODBUS_EXCEPTION_NONE;
}

/**
 * @brief Stub implementation for reading input registers
 */
modbus_exception_t modbus_cb_read_input_registers(uint16_t start_address,
                                                  uint16_t quantity,
                                                  uint16_t *register_values) {
  (void)start_address;
  (void)quantity;
  (void)register_values;
  return MODBUS_EXCEPTION_NONE;
}

/**
 * @brief Stub implementation for writing a single coil
 */
modbus_exception_t modbus_cb_write_single_coil(uint16_t address, bool value) {
  (void)address;
  (void)value;
  return MODBUS_EXCEPTION_NONE;
}

/**
 * @brief Stub implementation for writing a single register
 */
modbus_exception_t modbus_cb_write_single_register(uint16_t address,
                                                   uint16_t value) {
  (void)address;
  (void)value;
  return MODBUS_EXCEPTION_NONE;
}

/**
 * @brief Stub implementation for writing multiple coils
 */
modbus_exception_t modbus_cb_write_multiple_coils(uint16_t start_address,
                                                  uint16_t quantity,
                                                  const uint8_t *coil_values) {
  (void)start_address;
  (void)quantity;
  (void)coil_values;
  return MODBUS_EXCEPTION_NONE;
}

/**
 * @brief Stub implementation for writing multiple registers
 */
modbus_exception_t
modbus_cb_write_multiple_registers(uint16_t start_address, uint16_t quantity,
                                   const uint16_t *register_values) {
  (void)start_address;
  (void)quantity;
  (void)register_values;
  return MODBUS_EXCEPTION_NONE;
}

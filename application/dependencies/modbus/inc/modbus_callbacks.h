/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * Modbus Callback Declarations
 *
 * This file declares the callback functions that the user application must
 * implement to handle Modbus requests. The Modbus library calls these
 * functions when processing incoming requests.
 *
 * IMPORTANT: The user application MUST implement all required callbacks.
 * Optional callbacks can be left unimplemented (weak symbols provided).
 */

#ifndef MODBUS_CALLBACKS_H
#define MODBUS_CALLBACKS_H

#include "modbus_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* ==========================================================================
     * Coil Callbacks (FC01, FC05, FC15)
     *
     * Coils are single-bit read/write values, typically representing digital
     * outputs. Address range: 0x0000 - 0xFFFF (0 - 65535)
     * ==========================================================================
     */

    /**
     * @brief Read coils callback (FC01)
     *
     * Called when a Read Coils request is received. The implementation should
     * read the coil values starting at the specified address and pack them
     * into the output buffer (LSB first, bit 0 = first coil).
     *
     * @param[in]  start_address Starting coil address (0-based)
     * @param[in]  quantity      Number of coils to read (1-2000)
     * @param[out] coil_values   Output buffer for coil values (bit-packed)
     *                           Buffer size: (quantity + 7) / 8 bytes
     * @return MODBUS_EXCEPTION_NONE on success, or appropriate exception code
     *
     * @note The coil_values buffer is pre-zeroed before this callback is
     * called.
     */
    modbus_exception_t modbus_cb_read_coils(uint16_t start_address,
                                            uint16_t quantity,
                                            uint8_t *coil_values);

    /**
     * @brief Write single coil callback (FC05)
     *
     * Called when a Write Single Coil request is received. The implementation
     * should set the specified coil to the given value.
     *
     * @param[in] address Coil address (0-based)
     * @param[in] value   Coil value (true = ON/0xFF00, false = OFF/0x0000)
     * @return MODBUS_EXCEPTION_NONE on success, or appropriate exception code
     */
    modbus_exception_t modbus_cb_write_single_coil(uint16_t address,
                                                   bool     value);

    /**
     * @brief Write multiple coils callback (FC15)
     *
     * Called when a Write Multiple Coils request is received. The
     * implementation should set the coils starting at the specified address to
     * the given values.
     *
     * @param[in] start_address Starting coil address (0-based)
     * @param[in] quantity      Number of coils to write (1-1968)
     * @param[in] coil_values   Input buffer with coil values (bit-packed)
     *                          LSB first, bit 0 = first coil
     * @return MODBUS_EXCEPTION_NONE on success, or appropriate exception code
     */
    modbus_exception_t modbus_cb_write_multiple_coils(
        uint16_t start_address, uint16_t quantity, const uint8_t *coil_values);

    /* ==========================================================================
     * Discrete Input Callbacks (FC02)
     *
     * Discrete inputs are single-bit read-only values, typically representing
     * digital inputs. Address range: 0x0000 - 0xFFFF (0 - 65535)
     * ==========================================================================
     */

    /**
     * @brief Read discrete inputs callback (FC02)
     *
     * Called when a Read Discrete Inputs request is received. The
     * implementation should read the input values starting at the specified
     * address and pack them into the output buffer (LSB first, bit 0 = first
     * input).
     *
     * @param[in]  start_address Starting input address (0-based)
     * @param[in]  quantity      Number of inputs to read (1-2000)
     * @param[out] input_values  Output buffer for input values (bit-packed)
     *                           Buffer size: (quantity + 7) / 8 bytes
     * @return MODBUS_EXCEPTION_NONE on success, or appropriate exception code
     *
     * @note The input_values buffer is pre-zeroed before this callback is
     * called.
     */
    modbus_exception_t modbus_cb_read_discrete_inputs(uint16_t start_address,
                                                      uint16_t quantity,
                                                      uint8_t *input_values);

    /* ==========================================================================
     * Holding Register Callbacks (FC03, FC06, FC16)
     *
     * Holding registers are 16-bit read/write values, typically representing
     * configuration parameters or output values. Address range: 0x0000 - 0xFFFF
     * ==========================================================================
     */

    /**
     * @brief Read holding registers callback (FC03)
     *
     * Called when a Read Holding Registers request is received. The
     * implementation should read the register values starting at the specified
     * address.
     *
     * @param[in]  start_address   Starting register address (0-based)
     * @param[in]  quantity        Number of registers to read (1-125)
     * @param[out] register_values Output buffer for register values
     *                             Buffer size: quantity * sizeof(uint16_t)
     * @return MODBUS_EXCEPTION_NONE on success, or appropriate exception code
     *
     * @note Register values should be in native byte order; the library handles
     *       byte swapping for network transmission.
     */
    modbus_exception_t modbus_cb_read_holding_registers(
        uint16_t start_address, uint16_t quantity, uint16_t *register_values);

    /**
     * @brief Write single register callback (FC06)
     *
     * Called when a Write Single Register request is received. The
     * implementation should set the specified register to the given value.
     *
     * @param[in] address Register address (0-based)
     * @param[in] value   Register value (16-bit)
     * @return MODBUS_EXCEPTION_NONE on success, or appropriate exception code
     */
    modbus_exception_t modbus_cb_write_single_register(uint16_t address,
                                                       uint16_t value);

    /**
     * @brief Write multiple registers callback (FC16)
     *
     * Called when a Write Multiple Registers request is received. The
     * implementation should set the registers starting at the specified address
     * to the given values.
     *
     * @param[in] start_address   Starting register address (0-based)
     * @param[in] quantity        Number of registers to write (1-123)
     * @param[in] register_values Input buffer with register values
     * @return MODBUS_EXCEPTION_NONE on success, or appropriate exception code
     *
     * @note Register values are in native byte order.
     */
    modbus_exception_t modbus_cb_write_multiple_registers(
        uint16_t start_address, uint16_t quantity,
        const uint16_t *register_values);

    /* ==========================================================================
     * Input Register Callbacks (FC04)
     *
     * Input registers are 16-bit read-only values, typically representing
     * measured values or status information. Address range: 0x0000 - 0xFFFF
     * ==========================================================================
     */

    /**
     * @brief Read input registers callback (FC04)
     *
     * Called when a Read Input Registers request is received. The
     * implementation should read the register values starting at the specified
     * address.
     *
     * @param[in]  start_address   Starting register address (0-based)
     * @param[in]  quantity        Number of registers to read (1-125)
     * @param[out] register_values Output buffer for register values
     *                             Buffer size: quantity * sizeof(uint16_t)
     * @return MODBUS_EXCEPTION_NONE on success, or appropriate exception code
     *
     * @note Register values should be in native byte order.
     */
    modbus_exception_t modbus_cb_read_input_registers(
        uint16_t start_address, uint16_t quantity, uint16_t *register_values);

    /* ==========================================================================
     * Optional Event Callbacks
     *
     * These callbacks are optional and provide hooks for monitoring and
     * logging. Default weak implementations are provided that do nothing.
     * ==========================================================================
     */

    /**
     * @brief Request received callback (optional)
     *
     * Called when a valid Modbus request is received, before processing.
     * Can be used for logging or access control.
     *
     * @param[in] unit_id       Target unit ID
     * @param[in] function_code Function code of the request
     *
     * @note This is a weak symbol; implementation is optional.
     */
    void modbus_cb_request_received(uint8_t unit_id, uint8_t function_code);

    /**
     * @brief Response sent callback (optional)
     *
     * Called after a response has been sent (or exception generated).
     * Can be used for logging or statistics.
     *
     * @param[in] unit_id       Target unit ID
     * @param[in] function_code Function code of the request
     * @param[in] exception     Exception code (NONE if successful)
     *
     * @note This is a weak symbol; implementation is optional.
     */
    void modbus_cb_response_sent(uint8_t unit_id, uint8_t function_code,
                                 modbus_exception_t exception);

    /**
     * @brief Error callback (optional)
     *
     * Called when an error occurs during Modbus processing.
     * Can be used for error logging and diagnostics.
     *
     * @param[in] error Error code
     * @param[in] info  Additional error information (context-dependent)
     *
     * @note This is a weak symbol; implementation is optional.
     */
    void modbus_cb_error(modbus_error_t error, uint32_t info);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_CALLBACKS_H */

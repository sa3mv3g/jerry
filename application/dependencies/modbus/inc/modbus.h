/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * Modbus Library - Main Public API
 *
 * This is the main header file for the Modbus library. Include this file
 * to access all public Modbus functionality.
 *
 * Features:
 * - Modbus RTU (serial, CRC-16)
 * - Modbus ASCII (serial, LRC)
 * - Modbus TCP/IP (Ethernet)
 * - Slave (server) and Master (client) modes
 * - Callback-based architecture
 * - No dynamic memory allocation
 */

#ifndef MODBUS_H
#define MODBUS_H

#include "modbus_callbacks.h"
#include "modbus_config.h"
#include "modbus_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * Library Version
 * ========================================================================== */
#define MODBUS_VERSION_MAJOR  1
#define MODBUS_VERSION_MINOR  0
#define MODBUS_VERSION_PATCH  0
#define MODBUS_VERSION_STRING "1.0.0"

/* ==========================================================================
 * Context Management
 * ========================================================================== */

/**
 * @brief Get the size of a Modbus context structure
 *
 * Use this to allocate static storage for a Modbus context.
 *
 * @return Size in bytes required for a modbus_context_t
 *
 * @code
 * static uint8_t modbus_ctx_storage[modbus_context_size()];
 * modbus_context_t* ctx = (modbus_context_t*)modbus_ctx_storage;
 * @endcode
 */
size_t modbus_context_size(void);

/**
 * @brief Initialize a Modbus context
 *
 * Initializes the Modbus context with the specified configuration.
 * The context must be allocated by the caller (static allocation).
 *
 * @param[out] ctx    Pointer to context storage (must be at least
 *                    modbus_context_size() bytes)
 * @param[in]  config Configuration parameters
 * @return MODBUS_OK on success, error code on failure
 *
 * @note The context storage must remain valid for the lifetime of the
 *       Modbus instance.
 */
modbus_error_t modbus_init(modbus_context_t      *ctx,
                           const modbus_config_t *config);

/**
 * @brief Deinitialize a Modbus context
 *
 * Releases any resources associated with the context and closes
 * any open connections.
 *
 * @param[in] ctx Pointer to initialized context
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_deinit(modbus_context_t *ctx);

/**
 * @brief Get default configuration
 *
 * Fills the configuration structure with default values.
 *
 * @param[out] config Configuration structure to fill
 * @param[in]  protocol Protocol type (RTU, ASCII, or TCP)
 */
void modbus_get_default_config(modbus_config_t  *config,
                               modbus_protocol_t protocol);

/* ==========================================================================
 * Connection Management
 * ========================================================================== */

/**
 * @brief Connect to transport layer
 *
 * For serial (RTU/ASCII): Opens and configures the serial port
 * For TCP slave: Starts listening on the configured port
 * For TCP master: Connects to the specified server
 *
 * @param[in] ctx Pointer to initialized context
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_connect(modbus_context_t *ctx);

/**
 * @brief Disconnect from transport layer
 *
 * Closes the connection and releases transport resources.
 *
 * @param[in] ctx Pointer to initialized context
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_disconnect(modbus_context_t *ctx);

/**
 * @brief Check if connected
 *
 * @param[in] ctx Pointer to initialized context
 * @return true if connected, false otherwise
 */
bool modbus_is_connected(const modbus_context_t *ctx);

/* ==========================================================================
 * Slave Mode API
 * ========================================================================== */

#if MODBUS_ENABLE_SLAVE

/**
 * @brief Process incoming data (slave mode)
 *
 * Call this function when data is received from the transport layer.
 * The function will parse the frame, validate it, and invoke the
 * appropriate callback function.
 *
 * @param[in] ctx    Pointer to initialized context
 * @param[in] data   Received data buffer
 * @param[in] length Length of received data
 * @return MODBUS_OK on success, error code on failure
 *
 * @note For RTU, call this with complete frames (after inter-frame delay).
 *       For TCP, call this with complete MBAP + PDU data.
 */
modbus_error_t modbus_slave_process(modbus_context_t *ctx, const uint8_t *data,
                                    uint16_t length);

/**
 * @brief Poll for slave mode operations
 *
 * Call this function periodically to handle timeouts and other
 * time-based operations. Recommended call interval: 1-10ms.
 *
 * @param[in] ctx Pointer to initialized context
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_slave_poll(modbus_context_t *ctx);

#endif /* MODBUS_ENABLE_SLAVE */

/* ==========================================================================
 * Master Mode API
 * ========================================================================== */

#if MODBUS_ENABLE_MASTER

/**
 * @brief Read coils from slave (FC01)
 *
 * @param[in]  ctx           Pointer to initialized context
 * @param[in]  slave_addr    Slave address (1-247)
 * @param[in]  start_address Starting coil address
 * @param[in]  quantity      Number of coils to read (1-2000)
 * @param[out] coil_values   Buffer for coil values (bit-packed)
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_master_read_coils(modbus_context_t *ctx,
                                        uint8_t           slave_addr,
                                        uint16_t          start_address,
                                        uint16_t          quantity,
                                        uint8_t          *coil_values);

/**
 * @brief Read discrete inputs from slave (FC02)
 *
 * @param[in]  ctx           Pointer to initialized context
 * @param[in]  slave_addr    Slave address (1-247)
 * @param[in]  start_address Starting input address
 * @param[in]  quantity      Number of inputs to read (1-2000)
 * @param[out] input_values  Buffer for input values (bit-packed)
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_master_read_discrete_inputs(modbus_context_t *ctx,
                                                  uint8_t           slave_addr,
                                                  uint16_t start_address,
                                                  uint16_t quantity,
                                                  uint8_t *input_values);

/**
 * @brief Read holding registers from slave (FC03)
 *
 * @param[in]  ctx             Pointer to initialized context
 * @param[in]  slave_addr      Slave address (1-247)
 * @param[in]  start_address   Starting register address
 * @param[in]  quantity        Number of registers to read (1-125)
 * @param[out] register_values Buffer for register values
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_master_read_holding_registers(modbus_context_t *ctx,
                                                    uint8_t   slave_addr,
                                                    uint16_t  start_address,
                                                    uint16_t  quantity,
                                                    uint16_t *register_values);

/**
 * @brief Read input registers from slave (FC04)
 *
 * @param[in]  ctx             Pointer to initialized context
 * @param[in]  slave_addr      Slave address (1-247)
 * @param[in]  start_address   Starting register address
 * @param[in]  quantity        Number of registers to read (1-125)
 * @param[out] register_values Buffer for register values
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_master_read_input_registers(modbus_context_t *ctx,
                                                  uint8_t           slave_addr,
                                                  uint16_t  start_address,
                                                  uint16_t  quantity,
                                                  uint16_t *register_values);

/**
 * @brief Write single coil to slave (FC05)
 *
 * @param[in] ctx        Pointer to initialized context
 * @param[in] slave_addr Slave address (1-247)
 * @param[in] address    Coil address
 * @param[in] value      Coil value (true = ON, false = OFF)
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_master_write_single_coil(modbus_context_t *ctx,
                                               uint8_t           slave_addr,
                                               uint16_t address, bool value);

/**
 * @brief Write single register to slave (FC06)
 *
 * @param[in] ctx        Pointer to initialized context
 * @param[in] slave_addr Slave address (1-247)
 * @param[in] address    Register address
 * @param[in] value      Register value
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_master_write_single_register(modbus_context_t *ctx,
                                                   uint8_t           slave_addr,
                                                   uint16_t          address,
                                                   uint16_t          value);

/**
 * @brief Write multiple coils to slave (FC15)
 *
 * @param[in] ctx           Pointer to initialized context
 * @param[in] slave_addr    Slave address (1-247)
 * @param[in] start_address Starting coil address
 * @param[in] quantity      Number of coils to write (1-1968)
 * @param[in] coil_values   Coil values (bit-packed)
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_master_write_multiple_coils(modbus_context_t *ctx,
                                                  uint8_t           slave_addr,
                                                  uint16_t       start_address,
                                                  uint16_t       quantity,
                                                  const uint8_t *coil_values);

/**
 * @brief Write multiple registers to slave (FC16)
 *
 * @param[in] ctx             Pointer to initialized context
 * @param[in] slave_addr      Slave address (1-247)
 * @param[in] start_address   Starting register address
 * @param[in] quantity        Number of registers to write (1-123)
 * @param[in] register_values Register values
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_master_write_multiple_registers(
    modbus_context_t *ctx, uint8_t slave_addr, uint16_t start_address,
    uint16_t quantity, const uint16_t *register_values);

/**
 * @brief Poll for master mode operations
 *
 * Call this function periodically to handle response timeouts and
 * other time-based operations.
 *
 * @param[in] ctx Pointer to initialized context
 * @return MODBUS_OK on success, error code on failure
 */
modbus_error_t modbus_master_poll(modbus_context_t *ctx);

#endif /* MODBUS_ENABLE_MASTER */

/* ==========================================================================
 * Utility Functions
 * ========================================================================== */

/**
 * @brief Get error string
 *
 * Returns a human-readable string for the given error code.
 *
 * @param[in] error Error code
 * @return Pointer to static string describing the error
 */
const char *modbus_error_string(modbus_error_t error);

/**
 * @brief Get exception string
 *
 * Returns a human-readable string for the given exception code.
 *
 * @param[in] exception Exception code
 * @return Pointer to static string describing the exception
 */
const char *modbus_exception_string(modbus_exception_t exception);

/**
 * @brief Get last exception
 *
 * Returns the last Modbus exception that occurred.
 *
 * @param[in] ctx Pointer to initialized context
 * @return Last exception code
 */
modbus_exception_t modbus_get_last_exception(const modbus_context_t *ctx);

/**
 * @brief Get statistics
 *
 * Returns statistics about Modbus operations.
 *
 * @param[in]  ctx              Pointer to initialized context
 * @param[out] requests_count   Number of requests processed (can be NULL)
 * @param[out] responses_count  Number of responses sent (can be NULL)
 * @param[out] errors_count     Number of errors (can be NULL)
 * @param[out] exceptions_count Number of exceptions (can be NULL)
 */
void modbus_get_statistics(const modbus_context_t *ctx,
                           uint32_t *requests_count, uint32_t *responses_count,
                           uint32_t *errors_count, uint32_t *exceptions_count);

/**
 * @brief Reset statistics
 *
 * Resets all statistics counters to zero.
 *
 * @param[in] ctx Pointer to initialized context
 */
void modbus_reset_statistics(modbus_context_t *ctx);

/* ==========================================================================
 * CRC/LRC Utility Functions
 * ========================================================================== */

/**
 * @brief Calculate CRC-16 (Modbus)
 *
 * Calculates the CRC-16 checksum used in Modbus RTU.
 *
 * @param[in] data   Data buffer
 * @param[in] length Length of data
 * @return CRC-16 value (low byte first in frame)
 */
uint16_t modbus_crc16(const uint8_t *data, uint16_t length);

/**
 * @brief Calculate LRC (Longitudinal Redundancy Check)
 *
 * Calculates the LRC checksum used in Modbus ASCII.
 *
 * @param[in] data   Data buffer (binary, not ASCII encoded)
 * @param[in] length Length of data
 * @return LRC value
 */
uint8_t modbus_lrc(const uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_H */

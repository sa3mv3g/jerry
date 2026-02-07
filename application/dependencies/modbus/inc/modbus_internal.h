/**
 * @file modbus_internal.h
 * @brief Internal function declarations for Modbus library
 *
 * This header provides forward declarations for all internal functions
 * used across the Modbus library modules. This is required for MISRA C:2012
 * Rule 8.4 compliance (compatible declarations for external linkage).
 *
 * @note This header is for internal library use only. Application code
 *       should only include modbus.h.
 *
 * @copyright Copyright (c) 2026
 */

#ifndef MODBUS_INTERNAL_H
#define MODBUS_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "modbus_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* ==========================================================================
     * CRC-16 Functions (modbus_crc.c)
     * ==========================================================================
     */

    /**
     * @brief Calculate CRC-16 using lookup table
     * @param[in] data Data buffer
     * @param[in] length Length of data
     * @return CRC-16 value (low byte first, as per Modbus spec)
     */
    uint16_t modbus_crc16(const uint8_t *data, uint16_t length);

    /**
     * @brief Calculate CRC-16 bit-by-bit (for verification)
     * @param[in] data Data buffer
     * @param[in] length Length of data
     * @return CRC-16 value
     */
    uint16_t modbus_crc16_bitwise(const uint8_t *data, uint16_t length);

    /**
     * @brief Verify CRC-16 of a frame
     * @param[in] frame Frame data including CRC
     * @param[in] frame_length Total frame length including 2 CRC bytes
     * @return true if CRC is valid, false otherwise
     */
    bool modbus_crc16_verify(const uint8_t *frame, uint16_t frame_length);

    /**
     * @brief Append CRC-16 to a frame
     * @param[in,out] frame Frame buffer (must have 2 extra bytes)
     * @param[in] data_length Length of data (without CRC)
     * @return Total frame length including CRC (data_length + 2)
     */
    uint16_t modbus_crc16_append(uint8_t *frame, uint16_t data_length);

    /* ==========================================================================
     * LRC Functions (modbus_lrc.c)
     * ==========================================================================
     */

    /**
     * @brief Calculate LRC (Longitudinal Redundancy Check)
     * @param[in] data Data buffer
     * @param[in] length Length of data
     * @return LRC value
     */
    uint8_t modbus_lrc(const uint8_t *data, uint16_t length);

    /**
     * @brief Verify LRC of a message
     * @param[in] data Data buffer including LRC byte
     * @param[in] length Total length including LRC byte
     * @return true if LRC is valid, false otherwise
     */
    bool modbus_lrc_verify(const uint8_t *data, uint16_t length);

    /**
     * @brief Convert binary byte to two ASCII hex characters
     * @param[in] byte Binary byte to convert
     * @param[out] high_char Pointer to store high nibble ASCII character
     * @param[out] low_char Pointer to store low nibble ASCII character
     */
    void modbus_byte_to_ascii(uint8_t byte, char *high_char, char *low_char);

    /**
     * @brief Convert two ASCII hex characters to binary byte
     * @param[in] high_char High nibble ASCII character
     * @param[in] low_char Low nibble ASCII character
     * @param[out] byte Pointer to store the resulting byte
     * @return MODBUS_OK on success, MODBUS_ERROR_FRAME on invalid chars
     */
    modbus_error_t modbus_ascii_to_byte(char high_char, char low_char,
                                        uint8_t *byte);

    /**
     * @brief Convert binary buffer to ASCII hex string
     * @param[in] binary Pointer to binary data
     * @param[in] binary_len Length of binary data
     * @param[out] ascii Pointer to ASCII output buffer
     * @param[in] ascii_size Size of ASCII buffer
     * @return Number of ASCII characters written, or 0 on error
     */
    uint16_t modbus_binary_to_ascii(const uint8_t *binary, uint16_t binary_len,
                                    char *ascii, uint16_t ascii_size);

    /**
     * @brief Convert ASCII hex string to binary buffer
     * @param[in] ascii Pointer to ASCII hex string
     * @param[in] ascii_len Length of ASCII string (must be even)
     * @param[out] binary Pointer to binary output buffer
     * @param[in] binary_size Size of binary buffer
     * @return Number of binary bytes written, or 0 on error
     */
    uint16_t modbus_ascii_to_binary(const char *ascii, uint16_t ascii_len,
                                    uint8_t *binary, uint16_t binary_size);

    /* ==========================================================================
     * PDU Functions (modbus_pdu.c)
     * ==========================================================================
     */

    /* Request Encoding */
    modbus_error_t modbus_pdu_encode_read_coils(modbus_pdu_t *pdu,
                                                uint16_t      start_address,
                                                uint16_t      quantity);

    modbus_error_t modbus_pdu_encode_read_discrete_inputs(
        modbus_pdu_t *pdu, uint16_t start_address, uint16_t quantity);

    modbus_error_t modbus_pdu_encode_read_holding_registers(
        modbus_pdu_t *pdu, uint16_t start_address, uint16_t quantity);

    modbus_error_t modbus_pdu_encode_read_input_registers(
        modbus_pdu_t *pdu, uint16_t start_address, uint16_t quantity);

    modbus_error_t modbus_pdu_encode_write_single_coil(modbus_pdu_t *pdu,
                                                       uint16_t      address,
                                                       bool          value);

    modbus_error_t modbus_pdu_encode_write_single_register(modbus_pdu_t *pdu,
                                                           uint16_t address,
                                                           uint16_t value);

    modbus_error_t modbus_pdu_encode_write_multiple_coils(
        modbus_pdu_t *pdu, uint16_t start_address, uint16_t quantity,
        const uint8_t *values);

    modbus_error_t modbus_pdu_encode_write_multiple_registers(
        modbus_pdu_t *pdu, uint16_t start_address, uint16_t quantity,
        const uint16_t *values);

    /* Response Encoding */
    modbus_error_t modbus_pdu_encode_read_bits_response(
        modbus_pdu_t *pdu, uint8_t function_code, const uint8_t *coil_values,
        uint16_t quantity);

    modbus_error_t modbus_pdu_encode_read_registers_response(
        modbus_pdu_t *pdu, uint8_t function_code,
        const uint16_t *register_values, uint16_t quantity);

    modbus_error_t modbus_pdu_encode_write_single_response(
        modbus_pdu_t *pdu, uint8_t function_code, uint16_t address,
        uint16_t value);

    modbus_error_t modbus_pdu_encode_write_multiple_response(
        modbus_pdu_t *pdu, uint8_t function_code, uint16_t start_address,
        uint16_t quantity);

    modbus_error_t modbus_pdu_encode_exception(
        modbus_pdu_t *pdu, uint8_t function_code,
        modbus_exception_t exception_code);

    /* Request Decoding */
    modbus_error_t modbus_pdu_decode_read_bits_request(const modbus_pdu_t *pdu,
                                                       uint16_t *start_address,
                                                       uint16_t *quantity);

    modbus_error_t modbus_pdu_decode_read_registers_request(
        const modbus_pdu_t *pdu, uint16_t *start_address, uint16_t *quantity);

    modbus_error_t modbus_pdu_decode_write_single_coil_request(
        const modbus_pdu_t *pdu, uint16_t *address, bool *value);

    modbus_error_t modbus_pdu_decode_write_single_register_request(
        const modbus_pdu_t *pdu, uint16_t *address, uint16_t *value);

    modbus_error_t modbus_pdu_decode_write_multiple_coils_request(
        const modbus_pdu_t *pdu, uint16_t *start_address, uint16_t *quantity,
        const uint8_t **values);

    modbus_error_t modbus_pdu_decode_write_multiple_registers_request(
        const modbus_pdu_t *pdu, uint16_t *start_address, uint16_t *quantity,
        uint16_t *values, uint16_t max_values);

    /* PDU Utilities */
    bool modbus_pdu_is_exception(const modbus_pdu_t *pdu);

    modbus_error_t modbus_pdu_get_exception(const modbus_pdu_t *pdu,
                                            modbus_exception_t *exception_code);

    modbus_error_t modbus_pdu_serialize(const modbus_pdu_t *pdu,
                                        uint8_t *buffer, uint16_t buffer_size,
                                        uint16_t *length);

    modbus_error_t modbus_pdu_deserialize(modbus_pdu_t  *pdu,
                                          const uint8_t *buffer,
                                          uint16_t       length);

    /* ==========================================================================
     * RTU Functions (modbus_rtu.c)
     * ==========================================================================
     */

    modbus_error_t modbus_rtu_build_frame(const modbus_adu_t *adu,
                                          uint8_t *frame, uint16_t frame_size,
                                          uint16_t *frame_length);

    modbus_error_t modbus_rtu_parse_frame(const uint8_t *frame,
                                          uint16_t       frame_length,
                                          modbus_adu_t  *adu);

    uint32_t modbus_rtu_get_interframe_delay_us(uint32_t baudrate);

    uint32_t modbus_rtu_get_interchar_timeout_us(uint32_t baudrate);

    bool modbus_rtu_address_match(uint8_t frame_address, uint8_t slave_address);

    bool modbus_rtu_is_broadcast(uint8_t address);

    /* ==========================================================================
     * ASCII Functions (modbus_ascii.c)
     * ==========================================================================
     */

    modbus_error_t modbus_ascii_build_frame(const modbus_adu_t *adu,
                                            char *frame, uint16_t frame_size,
                                            uint16_t *frame_length);

    modbus_error_t modbus_ascii_parse_frame(const char   *frame,
                                            uint16_t      frame_length,
                                            modbus_adu_t *adu);

    bool modbus_ascii_address_match(uint8_t frame_address,
                                    uint8_t slave_address);

    bool modbus_ascii_is_broadcast(uint8_t address);

    /* ==========================================================================
     * TCP Functions (modbus_tcp.c)
     * ==========================================================================
     */

    modbus_error_t modbus_tcp_build_frame(const modbus_adu_t *adu,
                                          uint8_t *frame, uint16_t frame_size,
                                          uint16_t *frame_length);

    modbus_error_t modbus_tcp_parse_frame(const uint8_t *frame,
                                          uint16_t       frame_length,
                                          modbus_adu_t  *adu);

    uint16_t modbus_tcp_get_next_transaction_id(void);

    void modbus_tcp_reset_transaction_id(void);

    void modbus_tcp_set_transaction_id(uint16_t id);

    uint16_t modbus_tcp_get_length_field(const uint8_t *frame);

    uint16_t modbus_tcp_get_transaction_id(const uint8_t *frame);

    uint8_t modbus_tcp_get_unit_id(const uint8_t *frame);

    /* ==========================================================================
     * Core Functions (modbus_core.c)
     * ==========================================================================
     */

    modbus_state_t modbus_get_state(const modbus_context_t *ctx);

    modbus_error_t modbus_slave_process_pdu(modbus_context_t   *ctx,
                                            const modbus_pdu_t *request,
                                            modbus_pdu_t       *response);

    modbus_error_t modbus_slave_process_adu(modbus_context_t   *ctx,
                                            const modbus_adu_t *request,
                                            modbus_adu_t       *response,
                                            bool               *send_response);

    uint32_t modbus_get_requests_processed(const modbus_context_t *ctx);

    uint32_t modbus_get_errors_count(const modbus_context_t *ctx);

    uint32_t modbus_get_exceptions_sent(const modbus_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_INTERNAL_H */

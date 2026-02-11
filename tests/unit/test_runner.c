/**
 * @file test_runner.c
 * @brief Unity test runner for Modbus library unit tests
 *
 * This file runs all unit tests for the Modbus library modules.
 *
 * @copyright Copyright (c) 2026
 */

#include "unity.h"

/* ==========================================================================
 * External Test Function Declarations
 * ========================================================================== */

/* CRC-16 Tests */
extern void test_crc16_empty_data(void);
extern void test_crc16_single_byte(void);
extern void test_crc16_known_vector_modbus(void);
extern void test_crc16_known_vector_ascii(void);
extern void test_crc16_verify_valid(void);
extern void test_crc16_verify_invalid(void);
extern void test_crc16_null_pointer(void);
extern void test_crc16_large_buffer(void);

/* LRC Tests */
extern void test_lrc_empty_data(void);
extern void test_lrc_single_byte(void);
extern void test_lrc_known_vector(void);
extern void test_lrc_verify_valid(void);
extern void test_lrc_verify_invalid(void);
extern void test_byte_to_ascii(void);
extern void test_ascii_to_byte_valid(void);
extern void test_ascii_to_byte_invalid(void);
extern void test_binary_to_ascii(void);
extern void test_ascii_to_binary(void);

/* PDU Tests */
extern void test_pdu_read_coils_request(void);
extern void test_pdu_read_coils_response(void);
extern void test_pdu_read_discrete_inputs_request(void);
extern void test_pdu_read_holding_regs_request(void);
extern void test_pdu_read_holding_regs_response(void);
extern void test_pdu_read_input_regs_request(void);
extern void test_pdu_write_single_coil_on(void);
extern void test_pdu_write_single_coil_off(void);
extern void test_pdu_write_single_reg_request(void);
extern void test_pdu_write_multi_coils_request(void);
extern void test_pdu_write_multi_regs_request(void);
extern void test_pdu_exception_response(void);
extern void test_pdu_serialize_deserialize(void);

/* RTU Tests */
extern void test_rtu_build_frame_fc03(void);
extern void test_rtu_parse_frame_fc03(void);
extern void test_rtu_parse_frame_invalid_crc(void);
extern void test_rtu_parse_frame_too_short(void);
extern void test_rtu_interframe_delay_9600(void);
extern void test_rtu_interframe_delay_19200(void);
extern void test_rtu_interframe_delay_38400(void);
extern void test_rtu_interchar_timeout_9600(void);
extern void test_rtu_address_match_direct(void);
extern void test_rtu_address_match_broadcast(void);
extern void test_rtu_address_mismatch(void);

/* ASCII Tests */
extern void test_ascii_build_frame_fc03(void);
extern void test_ascii_parse_frame_fc03(void);
extern void test_ascii_parse_frame_invalid_lrc(void);
extern void test_ascii_parse_frame_no_start(void);
extern void test_ascii_parse_frame_no_crlf(void);
extern void test_ascii_address_match(void);

/* TCP Tests */
extern void test_tcp_build_frame_fc03(void);
extern void test_tcp_parse_frame_fc03(void);
extern void test_tcp_parse_frame_wrong_protocol(void);
extern void test_tcp_parse_frame_length_mismatch(void);
extern void test_tcp_parse_frame_too_short(void);
extern void test_tcp_transaction_id(void);

/* ==========================================================================
 * Unity Setup and Teardown
 * ========================================================================== */

void setUp(void)
{
    /* Called before each test */
}

void tearDown(void)
{
    /* Called after each test */
}

/* ==========================================================================
 * Main Test Runner
 * ========================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* ======================================================================
     * CRC-16 Module Tests
     * ====================================================================== */
    printf("\n=== CRC-16 Module Tests ===\n");
    RUN_TEST(test_crc16_empty_data);
    RUN_TEST(test_crc16_single_byte);
    RUN_TEST(test_crc16_known_vector_modbus);
    RUN_TEST(test_crc16_known_vector_ascii);
    RUN_TEST(test_crc16_verify_valid);
    RUN_TEST(test_crc16_verify_invalid);
    RUN_TEST(test_crc16_null_pointer);
    RUN_TEST(test_crc16_large_buffer);

    /* ======================================================================
     * LRC Module Tests
     * ====================================================================== */
    printf("\n=== LRC Module Tests ===\n");
    RUN_TEST(test_lrc_empty_data);
    RUN_TEST(test_lrc_single_byte);
    RUN_TEST(test_lrc_known_vector);
    RUN_TEST(test_lrc_verify_valid);
    RUN_TEST(test_lrc_verify_invalid);
    RUN_TEST(test_byte_to_ascii);
    RUN_TEST(test_ascii_to_byte_valid);
    RUN_TEST(test_ascii_to_byte_invalid);
    RUN_TEST(test_binary_to_ascii);
    RUN_TEST(test_ascii_to_binary);

    /* ======================================================================
     * PDU Module Tests
     * ====================================================================== */
    printf("\n=== PDU Module Tests ===\n");
    RUN_TEST(test_pdu_read_coils_request);
    RUN_TEST(test_pdu_read_coils_response);
    RUN_TEST(test_pdu_read_discrete_inputs_request);
    RUN_TEST(test_pdu_read_holding_regs_request);
    RUN_TEST(test_pdu_read_holding_regs_response);
    RUN_TEST(test_pdu_read_input_regs_request);
    RUN_TEST(test_pdu_write_single_coil_on);
    RUN_TEST(test_pdu_write_single_coil_off);
    RUN_TEST(test_pdu_write_single_reg_request);
    RUN_TEST(test_pdu_write_multi_coils_request);
    RUN_TEST(test_pdu_write_multi_regs_request);
    RUN_TEST(test_pdu_exception_response);
    RUN_TEST(test_pdu_serialize_deserialize);

    /* ======================================================================
     * RTU Module Tests
     * ====================================================================== */
    printf("\n=== RTU Module Tests ===\n");
    RUN_TEST(test_rtu_build_frame_fc03);
    RUN_TEST(test_rtu_parse_frame_fc03);
    RUN_TEST(test_rtu_parse_frame_invalid_crc);
    RUN_TEST(test_rtu_parse_frame_too_short);
    RUN_TEST(test_rtu_interframe_delay_9600);
    RUN_TEST(test_rtu_interframe_delay_19200);
    RUN_TEST(test_rtu_interframe_delay_38400);
    RUN_TEST(test_rtu_interchar_timeout_9600);
    RUN_TEST(test_rtu_address_match_direct);
    RUN_TEST(test_rtu_address_match_broadcast);
    RUN_TEST(test_rtu_address_mismatch);

    /* ======================================================================
     * ASCII Module Tests
     * ====================================================================== */
    printf("\n=== ASCII Module Tests ===\n");
    RUN_TEST(test_ascii_build_frame_fc03);
    RUN_TEST(test_ascii_parse_frame_fc03);
    RUN_TEST(test_ascii_parse_frame_invalid_lrc);
    RUN_TEST(test_ascii_parse_frame_no_start);
    RUN_TEST(test_ascii_parse_frame_no_crlf);
    RUN_TEST(test_ascii_address_match);

    /* ======================================================================
     * TCP Module Tests
     * ====================================================================== */
    printf("\n=== TCP Module Tests ===\n");
    RUN_TEST(test_tcp_build_frame_fc03);
    RUN_TEST(test_tcp_parse_frame_fc03);
    RUN_TEST(test_tcp_parse_frame_wrong_protocol);
    RUN_TEST(test_tcp_parse_frame_length_mismatch);
    RUN_TEST(test_tcp_parse_frame_too_short);
    RUN_TEST(test_tcp_transaction_id);

    return UNITY_END();
}

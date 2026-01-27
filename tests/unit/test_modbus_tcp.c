/**
 * @file test_modbus_tcp.c
 * @brief Unity unit tests for Modbus TCP module
 *
 * Tests the TCP frame building and parsing functions including
 * MBAP header handling.
 *
 * @copyright Copyright (c) 2026
 */

#include "unity.h"
#include "modbus_types.h"
#include <string.h>

/* ==========================================================================
 * External Function Declarations
 * ========================================================================== */

extern modbus_error_t modbus_tcp_build_frame(const modbus_adu_t* adu,
                                              uint8_t* frame,
                                              uint16_t frame_size,
                                              uint16_t* frame_length);
extern modbus_error_t modbus_tcp_parse_frame(const uint8_t* frame,
                                              uint16_t frame_length,
                                              modbus_adu_t* adu);

/* ==========================================================================
 * Test Cases - Frame Building
 * ========================================================================== */

/**
 * @brief Test TCP frame building for FC03 request
 */
void test_tcp_build_frame_fc03(void)
{
    modbus_adu_t adu;
    uint8_t frame[256];
    uint16_t frame_length;
    modbus_error_t err;

    /* Build ADU: Read Holding Registers */
    adu.transaction_id = 0x0001;
    adu.protocol_id = 0x0000;  /* Modbus protocol */
    adu.unit_id = 0x01;
    adu.pdu.function_code = MODBUS_FC_READ_HOLDING_REGISTERS;
    adu.pdu.data[0] = 0x00;  /* Start address high */
    adu.pdu.data[1] = 0x00;  /* Start address low */
    adu.pdu.data[2] = 0x00;  /* Quantity high */
    adu.pdu.data[3] = 0x0A;  /* Quantity low (10) */
    adu.pdu.data_length = 4;

    err = modbus_tcp_build_frame(&adu, frame, sizeof(frame), &frame_length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(12, frame_length);  /* MBAP (7) + FC (1) + Data (4) */

    /* Verify MBAP header */
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[0]);  /* Transaction ID high */
    TEST_ASSERT_EQUAL_HEX8(0x01, frame[1]);  /* Transaction ID low */
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[2]);  /* Protocol ID high */
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[3]);  /* Protocol ID low */
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[4]);  /* Length high */
    TEST_ASSERT_EQUAL_HEX8(0x06, frame[5]);  /* Length low (Unit ID + PDU) */
    TEST_ASSERT_EQUAL_HEX8(0x01, frame[6]);  /* Unit ID */

    /* Verify PDU */
    TEST_ASSERT_EQUAL_HEX8(0x03, frame[7]);  /* FC03 */
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[8]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[9]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[10]);
    TEST_ASSERT_EQUAL_HEX8(0x0A, frame[11]);
}

/**
 * @brief Test TCP frame parsing for FC03 request
 */
void test_tcp_parse_frame_fc03(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* Complete TCP frame */
    uint8_t frame[] = {
        0x00, 0x01,  /* Transaction ID */
        0x00, 0x00,  /* Protocol ID */
        0x00, 0x06,  /* Length */
        0x01,        /* Unit ID */
        0x03,        /* FC03 */
        0x00, 0x00,  /* Start address */
        0x00, 0x0A   /* Quantity */
    };

    err = modbus_tcp_parse_frame(frame, sizeof(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL_HEX16(0x0001, adu.transaction_id);
    TEST_ASSERT_EQUAL_HEX16(0x0000, adu.protocol_id);
    TEST_ASSERT_EQUAL_HEX8(0x01, adu.unit_id);
    TEST_ASSERT_EQUAL_HEX8(0x03, adu.pdu.function_code);
    TEST_ASSERT_EQUAL(4, adu.pdu.data_length);
}

/**
 * @brief Test TCP frame parsing with wrong protocol ID
 */
void test_tcp_parse_frame_wrong_protocol(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* Frame with wrong protocol ID (0x0001 instead of 0x0000) */
    uint8_t frame[] = {
        0x00, 0x01,  /* Transaction ID */
        0x00, 0x01,  /* Wrong Protocol ID */
        0x00, 0x06,  /* Length */
        0x01,        /* Unit ID */
        0x03,        /* FC03 */
        0x00, 0x00,
        0x00, 0x0A
    };

    err = modbus_tcp_parse_frame(frame, sizeof(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_FRAME, err);
}

/**
 * @brief Test TCP frame parsing with length mismatch
 */
void test_tcp_parse_frame_length_mismatch(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* Frame with wrong length field (0x0010 instead of 0x0006) */
    uint8_t frame[] = {
        0x00, 0x01,  /* Transaction ID */
        0x00, 0x00,  /* Protocol ID */
        0x00, 0x10,  /* Wrong Length */
        0x01,        /* Unit ID */
        0x03,        /* FC03 */
        0x00, 0x00,
        0x00, 0x0A
    };

    err = modbus_tcp_parse_frame(frame, sizeof(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_FRAME, err);
}

/**
 * @brief Test TCP frame parsing with frame too short
 */
void test_tcp_parse_frame_too_short(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* Frame too short (< 8 bytes minimum) */
    uint8_t frame[] = {0x00, 0x01, 0x00, 0x00, 0x00};

    err = modbus_tcp_parse_frame(frame, sizeof(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_FRAME, err);
}

/**
 * @brief Test TCP transaction ID handling
 */
void test_tcp_transaction_id(void)
{
    modbus_adu_t adu_out, adu_in;
    uint8_t frame[256];
    uint16_t frame_length;
    modbus_error_t err;

    /* Build frame with specific transaction ID */
    adu_out.transaction_id = 0x1234;
    adu_out.protocol_id = 0x0000;
    adu_out.unit_id = 0x01;
    adu_out.pdu.function_code = MODBUS_FC_READ_COILS;
    adu_out.pdu.data[0] = 0x00;
    adu_out.pdu.data[1] = 0x00;
    adu_out.pdu.data[2] = 0x00;
    adu_out.pdu.data[3] = 0x0A;
    adu_out.pdu.data_length = 4;

    err = modbus_tcp_build_frame(&adu_out, frame, sizeof(frame), &frame_length);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);

    /* Verify transaction ID in frame */
    TEST_ASSERT_EQUAL_HEX8(0x12, frame[0]);
    TEST_ASSERT_EQUAL_HEX8(0x34, frame[1]);

    /* Parse and verify */
    err = modbus_tcp_parse_frame(frame, frame_length, &adu_in);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL_HEX16(0x1234, adu_in.transaction_id);
}

/* ==========================================================================
 * Test Cases - Error Handling
 * ========================================================================== */

/**
 * @brief Test TCP build frame with NULL ADU
 */
void test_tcp_build_frame_null_adu(void)
{
    uint8_t frame[256];
    uint16_t frame_length;
    modbus_error_t err;

    err = modbus_tcp_build_frame(NULL, frame, sizeof(frame), &frame_length);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test TCP build frame with NULL buffer
 */
void test_tcp_build_frame_null_buffer(void)
{
    modbus_adu_t adu;
    uint16_t frame_length;
    modbus_error_t err;

    adu.transaction_id = 0x0001;
    adu.protocol_id = 0x0000;
    adu.unit_id = 0x01;
    adu.pdu.function_code = MODBUS_FC_READ_COILS;
    adu.pdu.data_length = 4;

    err = modbus_tcp_build_frame(&adu, NULL, 256, &frame_length);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test TCP build frame with buffer too small
 */
void test_tcp_build_frame_buffer_small(void)
{
    modbus_adu_t adu;
    uint8_t frame[5];  /* Too small */
    uint16_t frame_length;
    modbus_error_t err;

    adu.transaction_id = 0x0001;
    adu.protocol_id = 0x0000;
    adu.unit_id = 0x01;
    adu.pdu.function_code = MODBUS_FC_READ_COILS;
    adu.pdu.data[0] = 0x00;
    adu.pdu.data[1] = 0x00;
    adu.pdu.data[2] = 0x00;
    adu.pdu.data[3] = 0x0A;
    adu.pdu.data_length = 4;

    err = modbus_tcp_build_frame(&adu, frame, sizeof(frame), &frame_length);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_BUFFER_OVERFLOW, err);
}

/**
 * @brief Test TCP parse frame with NULL frame
 */
void test_tcp_parse_frame_null_frame(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    err = modbus_tcp_parse_frame(NULL, 12, &adu);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test TCP parse frame with NULL ADU
 */
void test_tcp_parse_frame_null_adu(void)
{
    uint8_t frame[] = {
        0x00, 0x01, 0x00, 0x00, 0x00, 0x06,
        0x01, 0x03, 0x00, 0x00, 0x00, 0x0A
    };
    modbus_error_t err;

    err = modbus_tcp_parse_frame(frame, sizeof(frame), NULL);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test TCP frame round trip
 */
void test_tcp_frame_round_trip(void)
{
    modbus_adu_t adu_out, adu_in;
    uint8_t frame[256];
    uint16_t frame_length;
    modbus_error_t err;

    /* Build original ADU */
    adu_out.transaction_id = 0xABCD;
    adu_out.protocol_id = 0x0000;
    adu_out.unit_id = 0x05;
    adu_out.pdu.function_code = MODBUS_FC_WRITE_SINGLE_REGISTER;
    adu_out.pdu.data[0] = 0x00;
    adu_out.pdu.data[1] = 0x01;
    adu_out.pdu.data[2] = 0x00;
    adu_out.pdu.data[3] = 0x03;
    adu_out.pdu.data_length = 4;

    /* Build frame */
    err = modbus_tcp_build_frame(&adu_out, frame, sizeof(frame), &frame_length);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);

    /* Parse frame */
    err = modbus_tcp_parse_frame(frame, frame_length, &adu_in);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);

    /* Verify */
    TEST_ASSERT_EQUAL_HEX16(adu_out.transaction_id, adu_in.transaction_id);
    TEST_ASSERT_EQUAL_HEX16(adu_out.protocol_id, adu_in.protocol_id);
    TEST_ASSERT_EQUAL_HEX8(adu_out.unit_id, adu_in.unit_id);
    TEST_ASSERT_EQUAL_HEX8(adu_out.pdu.function_code, adu_in.pdu.function_code);
    TEST_ASSERT_EQUAL(adu_out.pdu.data_length, adu_in.pdu.data_length);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(adu_out.pdu.data, adu_in.pdu.data, adu_out.pdu.data_length);
}

/**
 * @brief Test TCP frame with response data
 */
void test_tcp_parse_response_frame(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* FC03 Response: Read Holding Registers */
    uint8_t frame[] = {
        0x00, 0x01,  /* Transaction ID */
        0x00, 0x00,  /* Protocol ID */
        0x00, 0x07,  /* Length (1 + 1 + 1 + 4) */
        0x01,        /* Unit ID */
        0x03,        /* FC03 */
        0x04,        /* Byte count */
        0x00, 0x01,  /* Register 1 */
        0x00, 0x02   /* Register 2 */
    };

    err = modbus_tcp_parse_frame(frame, sizeof(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL_HEX8(0x03, adu.pdu.function_code);
    TEST_ASSERT_EQUAL(5, adu.pdu.data_length);  /* byte count + 4 data bytes */
    TEST_ASSERT_EQUAL_HEX8(0x04, adu.pdu.data[0]);  /* Byte count */
}

/**
 * @brief Test TCP frame with exception response
 */
void test_tcp_parse_exception_frame(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* Exception Response: FC03 + 0x80, Exception 02 */
    uint8_t frame[] = {
        0x00, 0x01,  /* Transaction ID */
        0x00, 0x00,  /* Protocol ID */
        0x00, 0x03,  /* Length */
        0x01,        /* Unit ID */
        0x83,        /* FC03 + 0x80 */
        0x02         /* Exception code */
    };

    err = modbus_tcp_parse_frame(frame, sizeof(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL_HEX8(0x83, adu.pdu.function_code);
    TEST_ASSERT_EQUAL(1, adu.pdu.data_length);
    TEST_ASSERT_EQUAL_HEX8(0x02, adu.pdu.data[0]);
}

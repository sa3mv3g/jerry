/**
 * @file test_modbus_ascii.c
 * @brief Unity unit tests for Modbus ASCII module
 *
 * Tests the ASCII frame building, parsing, and state machine functions.
 *
 * @copyright Copyright (c) 2026
 */

#include "unity.h"
#include "modbus_types.h"
#include <string.h>

/* ==========================================================================
 * External Function Declarations
 * ========================================================================== */

extern modbus_error_t modbus_ascii_build_frame(const modbus_adu_t* adu,
                                                char* frame,
                                                uint16_t frame_size,
                                                uint16_t* frame_length);
extern modbus_error_t modbus_ascii_parse_frame(const char* frame,
                                                uint16_t frame_length,
                                                modbus_adu_t* adu);
extern bool modbus_ascii_address_match(uint8_t frame_address, uint8_t slave_address);
extern bool modbus_ascii_is_broadcast(uint8_t address);

/* ==========================================================================
 * Test Cases - Frame Building
 * ========================================================================== */

/**
 * @brief Test ASCII frame building for FC03 request
 */
void test_ascii_build_frame_fc03(void)
{
    modbus_adu_t adu;
    char frame[256];
    uint16_t frame_length;
    modbus_error_t err;

    /* Build ADU: Read Holding Registers */
    adu.unit_id = 0x01;
    adu.pdu.function_code = MODBUS_FC_READ_HOLDING_REGISTERS;
    adu.pdu.data[0] = 0x00;  /* Start address high */
    adu.pdu.data[1] = 0x00;  /* Start address low */
    adu.pdu.data[2] = 0x00;  /* Quantity high */
    adu.pdu.data[3] = 0x0A;  /* Quantity low (10) */
    adu.pdu.data_length = 4;

    err = modbus_ascii_build_frame(&adu, frame, sizeof(frame), &frame_length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_GREATER_THAN(0, frame_length);

    /* Frame should start with ':' */
    TEST_ASSERT_EQUAL_CHAR(':', frame[0]);

    /* Frame should end with CR LF */
    TEST_ASSERT_EQUAL_CHAR('\r', frame[frame_length - 2]);
    TEST_ASSERT_EQUAL_CHAR('\n', frame[frame_length - 1]);
}

/**
 * @brief Test ASCII frame parsing for FC03 request
 */
void test_ascii_parse_frame_fc03(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* ASCII frame: :0103000000F2<CR><LF> */
    /* Address=01, FC=03, Start=0000, Qty=000A, LRC=F2 */
    const char frame[] = ":0103000000000AF2\r\n";

    err = modbus_ascii_parse_frame(frame, (uint16_t)strlen(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL_HEX8(0x01, adu.unit_id);
    TEST_ASSERT_EQUAL_HEX8(0x03, adu.pdu.function_code);
}

/**
 * @brief Test ASCII frame parsing with invalid LRC
 */
void test_ascii_parse_frame_invalid_lrc(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* Frame with wrong LRC (FF instead of F2) */
    const char frame[] = ":0103000000000AFF\r\n";

    err = modbus_ascii_parse_frame(frame, (uint16_t)strlen(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_CRC, err);  /* LRC error uses CRC error code */
}

/**
 * @brief Test ASCII frame parsing without start character
 */
void test_ascii_parse_frame_no_start(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* Frame without ':' start character */
    const char frame[] = "0103000000000AF2\r\n";

    err = modbus_ascii_parse_frame(frame, (uint16_t)strlen(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_FRAME, err);
}

/**
 * @brief Test ASCII frame parsing without CR LF
 */
void test_ascii_parse_frame_no_crlf(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* Frame without CR LF ending */
    const char frame[] = ":0103000000000AF2";

    err = modbus_ascii_parse_frame(frame, (uint16_t)strlen(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_FRAME, err);
}

/* ==========================================================================
 * Test Cases - Address Matching
 * ========================================================================== */

/**
 * @brief Test ASCII address matching
 */
void test_ascii_address_match(void)
{
    /* Direct match */
    TEST_ASSERT_TRUE(modbus_ascii_address_match(0x01, 0x01));

    /* Broadcast match */
    TEST_ASSERT_TRUE(modbus_ascii_address_match(0x00, 0x01));

    /* Mismatch */
    TEST_ASSERT_FALSE(modbus_ascii_address_match(0x02, 0x01));
}

/**
 * @brief Test ASCII broadcast detection
 */
void test_ascii_is_broadcast(void)
{
    TEST_ASSERT_TRUE(modbus_ascii_is_broadcast(0x00));
    TEST_ASSERT_FALSE(modbus_ascii_is_broadcast(0x01));
}

/* ==========================================================================
 * Test Cases - Error Handling
 * ========================================================================== */

/**
 * @brief Test ASCII build frame with NULL ADU
 */
void test_ascii_build_frame_null_adu(void)
{
    char frame[256];
    uint16_t frame_length;
    modbus_error_t err;

    err = modbus_ascii_build_frame(NULL, frame, sizeof(frame), &frame_length);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test ASCII build frame with NULL buffer
 */
void test_ascii_build_frame_null_buffer(void)
{
    modbus_adu_t adu;
    uint16_t frame_length;
    modbus_error_t err;

    adu.unit_id = 0x01;
    adu.pdu.function_code = MODBUS_FC_READ_COILS;
    adu.pdu.data_length = 4;

    err = modbus_ascii_build_frame(&adu, NULL, 256, &frame_length);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test ASCII parse frame with NULL frame
 */
void test_ascii_parse_frame_null_frame(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    err = modbus_ascii_parse_frame(NULL, 20, &adu);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test ASCII parse frame with NULL ADU
 */
void test_ascii_parse_frame_null_adu(void)
{
    const char frame[] = ":0103000000000AF2\r\n";
    modbus_error_t err;

    err = modbus_ascii_parse_frame(frame, (uint16_t)strlen(frame), NULL);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test ASCII parse frame too short
 */
void test_ascii_parse_frame_too_short(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* Minimum frame: : + addr(2) + fc(2) + lrc(2) + CR + LF = 9 chars */
    const char frame[] = ":01\r\n";  /* Too short */

    err = modbus_ascii_parse_frame(frame, (uint16_t)strlen(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_FRAME, err);
}

/**
 * @brief Test ASCII frame round trip
 */
void test_ascii_frame_round_trip(void)
{
    modbus_adu_t adu_out, adu_in;
    char frame[256];
    uint16_t frame_length;
    modbus_error_t err;

    /* Build original ADU */
    adu_out.unit_id = 0x01;
    adu_out.pdu.function_code = MODBUS_FC_READ_HOLDING_REGISTERS;
    adu_out.pdu.data[0] = 0x00;
    adu_out.pdu.data[1] = 0x6B;
    adu_out.pdu.data[2] = 0x00;
    adu_out.pdu.data[3] = 0x03;
    adu_out.pdu.data_length = 4;

    /* Build frame */
    err = modbus_ascii_build_frame(&adu_out, frame, sizeof(frame), &frame_length);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);

    /* Parse frame */
    err = modbus_ascii_parse_frame(frame, frame_length, &adu_in);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);

    /* Verify */
    TEST_ASSERT_EQUAL_HEX8(adu_out.unit_id, adu_in.unit_id);
    TEST_ASSERT_EQUAL_HEX8(adu_out.pdu.function_code, adu_in.pdu.function_code);
    TEST_ASSERT_EQUAL(adu_out.pdu.data_length, adu_in.pdu.data_length);
}

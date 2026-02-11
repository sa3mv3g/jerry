/**
 * @file test_modbus_rtu.c
 * @brief Unity unit tests for Modbus RTU module
 *
 * Tests the RTU frame building, parsing, and timing functions.
 *
 * @copyright Copyright (c) 2026
 */

#include "unity.h"
#include "modbus_types.h"
#include <string.h>

/* ==========================================================================
 * External Function Declarations
 * ========================================================================== */

extern modbus_error_t modbus_rtu_build_frame(const modbus_adu_t* adu,
                                              uint8_t* frame,
                                              uint16_t frame_size,
                                              uint16_t* frame_length);
extern modbus_error_t modbus_rtu_parse_frame(const uint8_t* frame,
                                              uint16_t frame_length,
                                              modbus_adu_t* adu);
extern uint32_t modbus_rtu_get_interframe_delay_us(uint32_t baudrate);
extern uint32_t modbus_rtu_get_interchar_timeout_us(uint32_t baudrate);
extern bool modbus_rtu_address_match(uint8_t frame_address, uint8_t slave_address);
extern bool modbus_rtu_is_broadcast(uint8_t address);

/* ==========================================================================
 * Test Cases - Frame Building
 * ========================================================================== */

/**
 * @brief Test RTU frame building for FC03 request
 */
void test_rtu_build_frame_fc03(void)
{
    modbus_adu_t adu;
    uint8_t frame[256];
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

    err = modbus_rtu_build_frame(&adu, frame, sizeof(frame), &frame_length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(8, frame_length);  /* Address + FC + 4 data + 2 CRC */
    TEST_ASSERT_EQUAL_HEX8(0x01, frame[0]);  /* Address */
    TEST_ASSERT_EQUAL_HEX8(0x03, frame[1]);  /* FC03 */
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[4]);
    TEST_ASSERT_EQUAL_HEX8(0x0A, frame[5]);
    /* CRC should be 0xCDC5 (transmitted as low byte 0xC5, high byte 0xCD) */
    TEST_ASSERT_EQUAL_HEX8(0xC5, frame[6]);
    TEST_ASSERT_EQUAL_HEX8(0xCD, frame[7]);
}

/**
 * @brief Test RTU frame parsing for FC03 response
 */
void test_rtu_parse_frame_fc03(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* Complete RTU frame with valid CRC (0xCDC5 transmitted as 0xC5, 0xCD) */
    uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD};

    err = modbus_rtu_parse_frame(frame, sizeof(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL_HEX8(0x01, adu.unit_id);
    TEST_ASSERT_EQUAL_HEX8(0x03, adu.pdu.function_code);
    TEST_ASSERT_EQUAL(4, adu.pdu.data_length);
}

/**
 * @brief Test RTU frame parsing with invalid CRC
 */
void test_rtu_parse_frame_invalid_crc(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* Frame with wrong CRC */
    uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xFF, 0xFF};

    err = modbus_rtu_parse_frame(frame, sizeof(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_CRC, err);
}

/**
 * @brief Test RTU frame parsing with frame too short
 */
void test_rtu_parse_frame_too_short(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    /* Frame too short (< 4 bytes) */
    uint8_t frame[] = {0x01, 0x03};

    err = modbus_rtu_parse_frame(frame, sizeof(frame), &adu);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_FRAME, err);
}

/* ==========================================================================
 * Test Cases - Timing Calculations
 * ========================================================================== */

/**
 * @brief Test inter-frame delay at 9600 baud
 *
 * t3.5 = 3.5 * 11 * 1000000 / 9600 = 4010 µs
 */
void test_rtu_interframe_delay_9600(void)
{
    uint32_t delay = modbus_rtu_get_interframe_delay_us(9600);

    /* Should be approximately 4010 µs */
    TEST_ASSERT_UINT32_WITHIN(100, 4010, delay);
}

/**
 * @brief Test inter-frame delay at 19200 baud
 *
 * t3.5 = 3.5 * 11 * 1000000 / 19200 = 2005 µs
 */
void test_rtu_interframe_delay_19200(void)
{
    uint32_t delay = modbus_rtu_get_interframe_delay_us(19200);

    /* Should be approximately 2005 µs */
    TEST_ASSERT_UINT32_WITHIN(100, 2005, delay);
}

/**
 * @brief Test inter-frame delay at 38400 baud
 *
 * For baud rates > 19200, fixed 1750 µs is used
 */
void test_rtu_interframe_delay_38400(void)
{
    uint32_t delay = modbus_rtu_get_interframe_delay_us(38400);

    /* Should be fixed 1750 µs */
    TEST_ASSERT_EQUAL_UINT32(1750, delay);
}

/**
 * @brief Test inter-character timeout at 9600 baud
 *
 * t1.5 = 1.5 * 11 * 1000000 / 9600 = 1719 µs
 */
void test_rtu_interchar_timeout_9600(void)
{
    uint32_t timeout = modbus_rtu_get_interchar_timeout_us(9600);

    /* Should be approximately 1719 µs */
    TEST_ASSERT_UINT32_WITHIN(100, 1719, timeout);
}

/**
 * @brief Test inter-frame delay with zero baud rate
 */
void test_rtu_interframe_delay_zero(void)
{
    uint32_t delay = modbus_rtu_get_interframe_delay_us(0);

    /* Should return default 1750 µs */
    TEST_ASSERT_EQUAL_UINT32(1750, delay);
}

/* ==========================================================================
 * Test Cases - Address Matching
 * ========================================================================== */

/**
 * @brief Test direct address match
 */
void test_rtu_address_match_direct(void)
{
    bool result = modbus_rtu_address_match(0x01, 0x01);

    TEST_ASSERT_TRUE(result);
}

/**
 * @brief Test broadcast address match
 */
void test_rtu_address_match_broadcast(void)
{
    /* Broadcast address (0) should match any slave */
    bool result = modbus_rtu_address_match(0x00, 0x01);

    TEST_ASSERT_TRUE(result);
}

/**
 * @brief Test address mismatch
 */
void test_rtu_address_mismatch(void)
{
    bool result = modbus_rtu_address_match(0x02, 0x01);

    TEST_ASSERT_FALSE(result);
}

/**
 * @brief Test broadcast detection
 */
void test_rtu_is_broadcast(void)
{
    TEST_ASSERT_TRUE(modbus_rtu_is_broadcast(0x00));
    TEST_ASSERT_FALSE(modbus_rtu_is_broadcast(0x01));
    TEST_ASSERT_FALSE(modbus_rtu_is_broadcast(0xFF));
}

/* ==========================================================================
 * Test Cases - Error Handling
 * ========================================================================== */

/**
 * @brief Test RTU build frame with NULL ADU
 */
void test_rtu_build_frame_null_adu(void)
{
    uint8_t frame[256];
    uint16_t frame_length;
    modbus_error_t err;

    err = modbus_rtu_build_frame(NULL, frame, sizeof(frame), &frame_length);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test RTU build frame with NULL buffer
 */
void test_rtu_build_frame_null_buffer(void)
{
    modbus_adu_t adu;
    uint16_t frame_length;
    modbus_error_t err;

    adu.unit_id = 0x01;
    adu.pdu.function_code = MODBUS_FC_READ_COILS;
    adu.pdu.data_length = 4;

    err = modbus_rtu_build_frame(&adu, NULL, 256, &frame_length);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test RTU build frame with buffer too small
 */
void test_rtu_build_frame_buffer_small(void)
{
    modbus_adu_t adu;
    uint8_t frame[4];  /* Too small */
    uint16_t frame_length;
    modbus_error_t err;

    adu.unit_id = 0x01;
    adu.pdu.function_code = MODBUS_FC_READ_COILS;
    adu.pdu.data[0] = 0x00;
    adu.pdu.data[1] = 0x00;
    adu.pdu.data[2] = 0x00;
    adu.pdu.data[3] = 0x0A;
    adu.pdu.data_length = 4;

    err = modbus_rtu_build_frame(&adu, frame, sizeof(frame), &frame_length);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_BUFFER_OVERFLOW, err);
}

/**
 * @brief Test RTU parse frame with NULL frame
 */
void test_rtu_parse_frame_null_frame(void)
{
    modbus_adu_t adu;
    modbus_error_t err;

    err = modbus_rtu_parse_frame(NULL, 8, &adu);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test RTU parse frame with NULL ADU
 */
void test_rtu_parse_frame_null_adu(void)
{
    uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD};
    modbus_error_t err;

    err = modbus_rtu_parse_frame(frame, sizeof(frame), NULL);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

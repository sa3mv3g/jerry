/**
 * @file test_modbus_crc.c
 * @brief Unity unit tests for Modbus CRC-16 module
 *
 * Tests the CRC-16 calculation and verification functions used in
 * Modbus RTU protocol.
 *
 * @copyright Copyright (c) 2026
 */

#include "unity.h"
#include "modbus_types.h"
#include <string.h>

/* ==========================================================================
 * External Function Declarations
 * ========================================================================== */

extern uint16_t modbus_crc16(const uint8_t* data, uint16_t length);
extern bool modbus_crc16_verify(const uint8_t* data, uint16_t length);

/* ==========================================================================
 * Test Cases
 * ========================================================================== */

/**
 * @brief Test CRC-16 with empty data
 *
 * Empty data should return initial CRC value (0xFFFF)
 */
void test_crc16_empty_data(void)
{
    uint8_t data[] = {0};
    uint16_t crc = modbus_crc16(data, 0);

    /* Empty data returns initial CRC value */
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc);
}

/**
 * @brief Test CRC-16 with single byte
 *
 * Known test vector: single byte 0x01
 */
void test_crc16_single_byte(void)
{
    uint8_t data[] = {0x01};
    uint16_t crc = modbus_crc16(data, 1);

    /* CRC of single byte 0x01 */
    /* Calculated using standard Modbus CRC-16 polynomial */
    TEST_ASSERT_EQUAL_HEX16(0x807E, crc);
}

/**
 * @brief Test CRC-16 with known Modbus test vector
 *
 * Standard Modbus example: Read Holding Registers request
 * Address=0x01, FC=0x03, Start=0x0000, Quantity=0x000A
 */
void test_crc16_known_vector_modbus(void)
{
    /* Modbus RTU frame without CRC: Read 10 holding registers from address 0 */
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint16_t crc = modbus_crc16(data, sizeof(data));

    /* Expected CRC: 0xCDC5 (transmitted as low byte 0xCD, high byte 0xC5) */
    TEST_ASSERT_EQUAL_HEX16(0xCDC5, crc);
}

/**
 * @brief Test CRC-16 with ASCII string "123456789"
 *
 * This is a standard CRC-16 test vector
 */
void test_crc16_known_vector_ascii(void)
{
    uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint16_t crc = modbus_crc16(data, sizeof(data));

    /* CRC-16/MODBUS of "123456789" = 0x4B37 */
    TEST_ASSERT_EQUAL_HEX16(0x4B37, crc);
}

/**
 * @brief Test CRC-16 verification with valid frame
 *
 * Complete RTU frame with correct CRC should verify successfully
 */
void test_crc16_verify_valid(void)
{
    /* Complete Modbus RTU frame with CRC */
    /* Read 10 holding registers from address 0, CRC = 0xCDC5 (transmitted as 0xC5, 0xCD) */
    uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD};

    bool result = modbus_crc16_verify(frame, sizeof(frame));

    TEST_ASSERT_TRUE(result);
}

/**
 * @brief Test CRC-16 verification with invalid frame
 *
 * Frame with incorrect CRC should fail verification
 */
void test_crc16_verify_invalid(void)
{
    /* Frame with wrong CRC (0xFFFF instead of 0xC5CD) */
    uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xFF, 0xFF};

    bool result = modbus_crc16_verify(frame, sizeof(frame));

    TEST_ASSERT_FALSE(result);
}

/**
 * @brief Test CRC-16 with NULL pointer
 *
 * NULL data pointer should return initial CRC value
 */
void test_crc16_null_pointer(void)
{
    uint16_t crc = modbus_crc16(NULL, 10);

    /* NULL pointer returns initial CRC value */
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc);
}

/**
 * @brief Test CRC-16 with large buffer
 *
 * Test with maximum PDU size (256 bytes)
 */
void test_crc16_large_buffer(void)
{
    uint8_t data[256];

    /* Fill with pattern */
    for (int i = 0; i < 256; i++)
    {
        data[i] = (uint8_t)(i & 0xFF);
    }

    uint16_t crc = modbus_crc16(data, sizeof(data));

    /* Just verify it doesn't crash and returns a value */
    /* The actual CRC value depends on the data pattern */
    TEST_ASSERT_NOT_EQUAL(0xFFFF, crc);  /* Should be different from initial */
}

/**
 * @brief Test CRC-16 with Write Single Coil frame
 *
 * FC05: Write Single Coil ON at address 0x00AC
 */
void test_crc16_write_single_coil(void)
{
    /* Write Single Coil: Address=1, FC=05, Coil=0x00AC, Value=0xFF00 */
    uint8_t data[] = {0x01, 0x05, 0x00, 0xAC, 0xFF, 0x00};
    uint16_t crc = modbus_crc16(data, sizeof(data));

    /* Verify CRC is calculated (actual value depends on implementation) */
    TEST_ASSERT_NOT_EQUAL(0xFFFF, crc);

    /* Verify the frame with CRC appended */
    uint8_t frame[8];
    memcpy(frame, data, 6);
    frame[6] = (uint8_t)(crc & 0xFF);        /* CRC low byte */
    frame[7] = (uint8_t)((crc >> 8) & 0xFF); /* CRC high byte */

    TEST_ASSERT_TRUE(modbus_crc16_verify(frame, sizeof(frame)));
}

/**
 * @brief Test CRC-16 with Write Multiple Registers frame
 *
 * FC16: Write Multiple Registers
 */
void test_crc16_write_multiple_registers(void)
{
    /* Write Multiple Registers: Address=1, FC=16, Start=0x0001, Qty=2, Bytes=4, Data */
    uint8_t data[] = {0x01, 0x10, 0x00, 0x01, 0x00, 0x02, 0x04,
                      0x00, 0x0A, 0x01, 0x02};
    uint16_t crc = modbus_crc16(data, sizeof(data));

    /* Verify CRC is calculated */
    TEST_ASSERT_NOT_EQUAL(0xFFFF, crc);

    /* Verify the frame with CRC appended */
    uint8_t frame[13];
    memcpy(frame, data, 11);
    frame[11] = (uint8_t)(crc & 0xFF);
    frame[12] = (uint8_t)((crc >> 8) & 0xFF);

    TEST_ASSERT_TRUE(modbus_crc16_verify(frame, sizeof(frame)));
}

/**
 * @brief Test CRC-16 consistency
 *
 * Same data should always produce same CRC
 */
void test_crc16_consistency(void)
{
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};

    uint16_t crc1 = modbus_crc16(data, sizeof(data));
    uint16_t crc2 = modbus_crc16(data, sizeof(data));
    uint16_t crc3 = modbus_crc16(data, sizeof(data));

    TEST_ASSERT_EQUAL_HEX16(crc1, crc2);
    TEST_ASSERT_EQUAL_HEX16(crc2, crc3);
}

/**
 * @brief Test CRC-16 with response frame
 *
 * FC03 Response: Read Holding Registers response
 */
void test_crc16_response_frame(void)
{
    /* Response: Address=1, FC=03, ByteCount=4, Data=0x0001, 0x0002 */
    uint8_t data[] = {0x01, 0x03, 0x04, 0x00, 0x01, 0x00, 0x02};
    uint16_t crc = modbus_crc16(data, sizeof(data));

    /* Build complete frame */
    uint8_t frame[9];
    memcpy(frame, data, 7);
    frame[7] = (uint8_t)(crc & 0xFF);
    frame[8] = (uint8_t)((crc >> 8) & 0xFF);

    /* Verify */
    TEST_ASSERT_TRUE(modbus_crc16_verify(frame, sizeof(frame)));
}

/**
 * @brief Test CRC-16 with exception response
 *
 * Exception response frame
 */
void test_crc16_exception_frame(void)
{
    /* Exception: Address=1, FC=0x83 (FC03+0x80), Exception=0x02 */
    uint8_t data[] = {0x01, 0x83, 0x02};
    uint16_t crc = modbus_crc16(data, sizeof(data));

    /* Build complete frame */
    uint8_t frame[5];
    memcpy(frame, data, 3);
    frame[3] = (uint8_t)(crc & 0xFF);
    frame[4] = (uint8_t)((crc >> 8) & 0xFF);

    /* Verify */
    TEST_ASSERT_TRUE(modbus_crc16_verify(frame, sizeof(frame)));
}

/**
 * @brief Test CRC-16 verify with minimum frame size
 *
 * Minimum RTU frame: Address + FC + CRC = 4 bytes
 */
void test_crc16_verify_minimum_frame(void)
{
    /* Minimum frame: just address and function code */
    uint8_t data[] = {0x01, 0x03};
    uint16_t crc = modbus_crc16(data, sizeof(data));

    uint8_t frame[4];
    frame[0] = 0x01;
    frame[1] = 0x03;
    frame[2] = (uint8_t)(crc & 0xFF);
    frame[3] = (uint8_t)((crc >> 8) & 0xFF);

    TEST_ASSERT_TRUE(modbus_crc16_verify(frame, sizeof(frame)));
}

/**
 * @brief Test CRC-16 verify with frame too short
 *
 * Frame shorter than minimum should fail
 */
void test_crc16_verify_frame_too_short(void)
{
    uint8_t frame[] = {0x01, 0x03};  /* Only 2 bytes, need at least 4 */

    bool result = modbus_crc16_verify(frame, sizeof(frame));

    /* Should fail because frame is too short for CRC verification */
    TEST_ASSERT_FALSE(result);
}

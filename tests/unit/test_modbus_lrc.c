/**
 * @file test_modbus_lrc.c
 * @brief Unity unit tests for Modbus LRC module
 *
 * Tests the LRC calculation, verification, and ASCII conversion functions
 * used in Modbus ASCII protocol.
 *
 * @copyright Copyright (c) 2026
 */

#include "unity.h"
#include "modbus_types.h"
#include <string.h>

/* ==========================================================================
 * External Function Declarations
 * ========================================================================== */

extern uint8_t modbus_lrc(const uint8_t* data, uint16_t length);
extern bool modbus_lrc_verify(const uint8_t* data, uint16_t length);
extern void modbus_byte_to_ascii(uint8_t byte, char* high_char, char* low_char);
extern modbus_error_t modbus_ascii_to_byte(char high_char, char low_char, uint8_t* byte);
extern uint16_t modbus_binary_to_ascii(const uint8_t* binary, uint16_t binary_len,
                                        char* ascii, uint16_t ascii_size);
extern uint16_t modbus_ascii_to_binary(const char* ascii, uint16_t ascii_len,
                                        uint8_t* binary, uint16_t binary_size);

/* ==========================================================================
 * Test Cases - LRC Calculation
 * ========================================================================== */

/**
 * @brief Test LRC with empty data
 *
 * Empty data should return 0x00 (two's complement of 0)
 */
void test_lrc_empty_data(void)
{
    uint8_t data[] = {0};
    uint8_t lrc = modbus_lrc(data, 0);

    /* LRC of empty data is 0 */
    TEST_ASSERT_EQUAL_HEX8(0x00, lrc);
}

/**
 * @brief Test LRC with single byte
 *
 * LRC of single byte is two's complement
 */
void test_lrc_single_byte(void)
{
    uint8_t data[] = {0x01};
    uint8_t lrc = modbus_lrc(data, 1);

    /* Two's complement of 0x01 is 0xFF */
    TEST_ASSERT_EQUAL_HEX8(0xFF, lrc);
}

/**
 * @brief Test LRC with known Modbus test vector
 *
 * Standard Modbus ASCII example
 */
void test_lrc_known_vector(void)
{
    /* Read Holding Registers: Address=1, FC=03, Start=0x0000, Qty=0x000A */
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint8_t lrc = modbus_lrc(data, sizeof(data));

    /* Sum = 0x01 + 0x03 + 0x00 + 0x00 + 0x00 + 0x0A = 0x0E */
    /* Two's complement of 0x0E = 0xF2 */
    TEST_ASSERT_EQUAL_HEX8(0xF2, lrc);
}

/**
 * @brief Test LRC verification with valid data
 *
 * Data with correct LRC appended should verify successfully
 */
void test_lrc_verify_valid(void)
{
    /* Data with LRC appended */
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xF2};

    bool result = modbus_lrc_verify(data, sizeof(data));

    TEST_ASSERT_TRUE(result);
}

/**
 * @brief Test LRC verification with invalid data
 *
 * Data with wrong LRC should fail verification
 */
void test_lrc_verify_invalid(void)
{
    /* Data with wrong LRC (0xFF instead of 0xF2) */
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xFF};

    bool result = modbus_lrc_verify(data, sizeof(data));

    TEST_ASSERT_FALSE(result);
}

/* ==========================================================================
 * Test Cases - ASCII Conversion
 * ========================================================================== */

/**
 * @brief Test byte to ASCII conversion
 *
 * Convert various byte values to ASCII hex characters
 */
void test_byte_to_ascii(void)
{
    char high, low;

    /* Test 0xAB -> 'A', 'B' */
    modbus_byte_to_ascii(0xAB, &high, &low);
    TEST_ASSERT_EQUAL_CHAR('A', high);
    TEST_ASSERT_EQUAL_CHAR('B', low);

    /* Test 0x00 -> '0', '0' */
    modbus_byte_to_ascii(0x00, &high, &low);
    TEST_ASSERT_EQUAL_CHAR('0', high);
    TEST_ASSERT_EQUAL_CHAR('0', low);

    /* Test 0xFF -> 'F', 'F' */
    modbus_byte_to_ascii(0xFF, &high, &low);
    TEST_ASSERT_EQUAL_CHAR('F', high);
    TEST_ASSERT_EQUAL_CHAR('F', low);

    /* Test 0x5C -> '5', 'C' */
    modbus_byte_to_ascii(0x5C, &high, &low);
    TEST_ASSERT_EQUAL_CHAR('5', high);
    TEST_ASSERT_EQUAL_CHAR('C', low);

    /* Test 0x9A -> '9', 'A' */
    modbus_byte_to_ascii(0x9A, &high, &low);
    TEST_ASSERT_EQUAL_CHAR('9', high);
    TEST_ASSERT_EQUAL_CHAR('A', low);
}

/**
 * @brief Test valid ASCII to byte conversion
 *
 * Convert valid ASCII hex characters to byte
 */
void test_ascii_to_byte_valid(void)
{
    uint8_t byte;
    modbus_error_t err;

    /* Test 'A', 'B' -> 0xAB */
    err = modbus_ascii_to_byte('A', 'B', &byte);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL_HEX8(0xAB, byte);

    /* Test '0', '0' -> 0x00 */
    err = modbus_ascii_to_byte('0', '0', &byte);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL_HEX8(0x00, byte);

    /* Test 'F', 'F' -> 0xFF */
    err = modbus_ascii_to_byte('F', 'F', &byte);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL_HEX8(0xFF, byte);

    /* Test lowercase 'a', 'b' -> 0xAB */
    err = modbus_ascii_to_byte('a', 'b', &byte);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL_HEX8(0xAB, byte);

    /* Test mixed case 'A', 'b' -> 0xAB */
    err = modbus_ascii_to_byte('A', 'b', &byte);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL_HEX8(0xAB, byte);
}

/**
 * @brief Test invalid ASCII to byte conversion
 *
 * Invalid hex characters should return error
 */
void test_ascii_to_byte_invalid(void)
{
    uint8_t byte;
    modbus_error_t err;

    /* Test 'G', 'H' -> invalid */
    err = modbus_ascii_to_byte('G', 'H', &byte);
    TEST_ASSERT_NOT_EQUAL(MODBUS_OK, err);

    /* Test 'X', 'Y' -> invalid */
    err = modbus_ascii_to_byte('X', 'Y', &byte);
    TEST_ASSERT_NOT_EQUAL(MODBUS_OK, err);

    /* Test ' ', '0' -> invalid (space) */
    err = modbus_ascii_to_byte(' ', '0', &byte);
    TEST_ASSERT_NOT_EQUAL(MODBUS_OK, err);
}

/**
 * @brief Test binary to ASCII conversion
 *
 * Convert binary array to ASCII hex string
 */
void test_binary_to_ascii(void)
{
    uint8_t binary[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    char ascii[20];
    uint16_t len;

    len = modbus_binary_to_ascii(binary, sizeof(binary), ascii, sizeof(ascii));

    /* Should return 12 characters (6 bytes * 2) */
    TEST_ASSERT_EQUAL(12, len);

    /* Verify content */
    TEST_ASSERT_EQUAL_STRING_LEN("0103000000", ascii, 10);
}

/**
 * @brief Test ASCII to binary conversion
 *
 * Convert ASCII hex string to binary array
 */
void test_ascii_to_binary(void)
{
    const char* ascii = "0103000000";
    uint8_t binary[10];
    uint16_t len;

    len = modbus_ascii_to_binary(ascii, 10, binary, sizeof(binary));

    /* Should return 5 bytes */
    TEST_ASSERT_EQUAL(5, len);

    /* Verify content */
    TEST_ASSERT_EQUAL_HEX8(0x01, binary[0]);
    TEST_ASSERT_EQUAL_HEX8(0x03, binary[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, binary[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, binary[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00, binary[4]);
}

/**
 * @brief Test binary to ASCII with buffer too small
 *
 * Should return 0 if output buffer is too small
 */
void test_binary_to_ascii_buffer_small(void)
{
    uint8_t binary[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    char ascii[5];  /* Too small for 12 characters */
    uint16_t len;

    len = modbus_binary_to_ascii(binary, sizeof(binary), ascii, sizeof(ascii));

    /* Should return 0 (buffer too small) */
    TEST_ASSERT_EQUAL(0, len);
}

/**
 * @brief Test ASCII to binary with odd length
 *
 * Odd number of ASCII characters should be handled
 */
void test_ascii_to_binary_odd_length(void)
{
    const char* ascii = "01030";  /* 5 characters (odd) */
    uint8_t binary[10];
    uint16_t len;

    len = modbus_ascii_to_binary(ascii, 5, binary, sizeof(binary));

    /* Should return 2 bytes (only complete pairs) */
    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL_HEX8(0x01, binary[0]);
    TEST_ASSERT_EQUAL_HEX8(0x03, binary[1]);
}

/**
 * @brief Test LRC with Write Single Coil data
 */
void test_lrc_write_single_coil(void)
{
    /* Write Single Coil: Address=1, FC=05, Coil=0x00AC, Value=0xFF00 */
    uint8_t data[] = {0x01, 0x05, 0x00, 0xAC, 0xFF, 0x00};
    uint8_t lrc = modbus_lrc(data, sizeof(data));

    /* Verify LRC is calculated */
    /* Sum = 0x01 + 0x05 + 0x00 + 0xAC + 0xFF + 0x00 = 0x1AB */
    /* Low byte = 0xAB, Two's complement = 0x55 */
    TEST_ASSERT_EQUAL_HEX8(0x55, lrc);

    /* Verify with LRC appended */
    uint8_t data_with_lrc[] = {0x01, 0x05, 0x00, 0xAC, 0xFF, 0x00, 0x55};
    TEST_ASSERT_TRUE(modbus_lrc_verify(data_with_lrc, sizeof(data_with_lrc)));
}

/**
 * @brief Test LRC with all zeros
 */
void test_lrc_all_zeros(void)
{
    uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
    uint8_t lrc = modbus_lrc(data, sizeof(data));

    /* Sum = 0, Two's complement of 0 = 0 */
    TEST_ASSERT_EQUAL_HEX8(0x00, lrc);
}

/**
 * @brief Test LRC with all 0xFF
 */
void test_lrc_all_ff(void)
{
    uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t lrc = modbus_lrc(data, sizeof(data));

    /* Sum = 0x3FC, Low byte = 0xFC, Two's complement = 0x04 */
    TEST_ASSERT_EQUAL_HEX8(0x04, lrc);
}

/**
 * @brief Test round-trip conversion
 *
 * Binary -> ASCII -> Binary should produce same result
 */
void test_conversion_round_trip(void)
{
    uint8_t original[] = {0x01, 0x03, 0xAB, 0xCD, 0xEF};
    char ascii[20];
    uint8_t result[10];
    uint16_t ascii_len, binary_len;

    /* Convert to ASCII */
    ascii_len = modbus_binary_to_ascii(original, sizeof(original), ascii, sizeof(ascii));
    TEST_ASSERT_EQUAL(10, ascii_len);

    /* Convert back to binary */
    binary_len = modbus_ascii_to_binary(ascii, ascii_len, result, sizeof(result));
    TEST_ASSERT_EQUAL(5, binary_len);

    /* Verify match */
    TEST_ASSERT_EQUAL_HEX8_ARRAY(original, result, 5);
}

/**
 * @brief Test NULL pointer handling in LRC
 */
void test_lrc_null_pointer(void)
{
    uint8_t lrc = modbus_lrc(NULL, 10);

    /* NULL pointer should return 0 */
    TEST_ASSERT_EQUAL_HEX8(0x00, lrc);
}

/**
 * @brief Test LRC verify with NULL pointer
 */
void test_lrc_verify_null_pointer(void)
{
    bool result = modbus_lrc_verify(NULL, 10);

    /* NULL pointer should return false */
    TEST_ASSERT_FALSE(result);
}

/**
 * @brief Test LRC verify with minimum length
 */
void test_lrc_verify_minimum_length(void)
{
    /* Minimum: 1 byte data + 1 byte LRC = 2 bytes */
    uint8_t data[] = {0x01, 0xFF};  /* 0x01 with LRC 0xFF */

    bool result = modbus_lrc_verify(data, sizeof(data));

    TEST_ASSERT_TRUE(result);
}

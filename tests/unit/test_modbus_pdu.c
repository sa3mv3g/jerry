/**
 * @file test_modbus_pdu.c
 * @brief Unity unit tests for Modbus PDU module
 *
 * Tests the PDU encoding and decoding functions for all supported
 * Modbus function codes.
 *
 * @copyright Copyright (c) 2026
 */

#include "unity.h"
#include "modbus_types.h"
#include <string.h>

/* ==========================================================================
 * External Function Declarations
 * ========================================================================== */

extern modbus_error_t modbus_pdu_serialize(const modbus_pdu_t* pdu,
                                            uint8_t* buffer,
                                            uint16_t buffer_size,
                                            uint16_t* length);
extern modbus_error_t modbus_pdu_deserialize(modbus_pdu_t* pdu,
                                              const uint8_t* buffer,
                                              uint16_t length);

/* ==========================================================================
 * Test Cases - Read Coils (FC01)
 * ========================================================================== */

/**
 * @brief Test PDU encoding for Read Coils request
 */
void test_pdu_read_coils_request(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build Read Coils request: Start=0x0000, Quantity=10 */
    pdu.function_code = MODBUS_FC_READ_COILS;
    pdu.data[0] = 0x00;  /* Start address high */
    pdu.data[1] = 0x00;  /* Start address low */
    pdu.data[2] = 0x00;  /* Quantity high */
    pdu.data[3] = 0x0A;  /* Quantity low (10) */
    pdu.data_length = 4;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(5, length);  /* FC + 4 data bytes */
    TEST_ASSERT_EQUAL_HEX8(0x01, buffer[0]);  /* FC01 */
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[3]);
    TEST_ASSERT_EQUAL_HEX8(0x0A, buffer[4]);
}

/**
 * @brief Test PDU encoding for Read Coils response
 */
void test_pdu_read_coils_response(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build Read Coils response: ByteCount=2, Data=0xCD, 0x01 */
    pdu.function_code = MODBUS_FC_READ_COILS;
    pdu.data[0] = 0x02;  /* Byte count */
    pdu.data[1] = 0xCD;  /* Coil status */
    pdu.data[2] = 0x01;  /* Coil status */
    pdu.data_length = 3;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(4, length);
    TEST_ASSERT_EQUAL_HEX8(0x01, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCD, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0x01, buffer[3]);
}

/* ==========================================================================
 * Test Cases - Read Discrete Inputs (FC02)
 * ========================================================================== */

/**
 * @brief Test PDU encoding for Read Discrete Inputs request
 */
void test_pdu_read_discrete_inputs_request(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build Read Discrete Inputs request: Start=0x00C4, Quantity=22 */
    pdu.function_code = MODBUS_FC_READ_DISCRETE_INPUTS;
    pdu.data[0] = 0x00;  /* Start address high */
    pdu.data[1] = 0xC4;  /* Start address low */
    pdu.data[2] = 0x00;  /* Quantity high */
    pdu.data[3] = 0x16;  /* Quantity low (22) */
    pdu.data_length = 4;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(5, length);
    TEST_ASSERT_EQUAL_HEX8(0x02, buffer[0]);  /* FC02 */
}

/* ==========================================================================
 * Test Cases - Read Holding Registers (FC03)
 * ========================================================================== */

/**
 * @brief Test PDU encoding for Read Holding Registers request
 */
void test_pdu_read_holding_regs_request(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build Read Holding Registers request: Start=0x006B, Quantity=3 */
    pdu.function_code = MODBUS_FC_READ_HOLDING_REGISTERS;
    pdu.data[0] = 0x00;  /* Start address high */
    pdu.data[1] = 0x6B;  /* Start address low */
    pdu.data[2] = 0x00;  /* Quantity high */
    pdu.data[3] = 0x03;  /* Quantity low */
    pdu.data_length = 4;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(5, length);
    TEST_ASSERT_EQUAL_HEX8(0x03, buffer[0]);  /* FC03 */
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0x6B, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[3]);
    TEST_ASSERT_EQUAL_HEX8(0x03, buffer[4]);
}

/**
 * @brief Test PDU encoding for Read Holding Registers response
 */
void test_pdu_read_holding_regs_response(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build response: ByteCount=6, Data=0x022B, 0x0000, 0x0064 */
    pdu.function_code = MODBUS_FC_READ_HOLDING_REGISTERS;
    pdu.data[0] = 0x06;  /* Byte count */
    pdu.data[1] = 0x02;  /* Register 1 high */
    pdu.data[2] = 0x2B;  /* Register 1 low */
    pdu.data[3] = 0x00;  /* Register 2 high */
    pdu.data[4] = 0x00;  /* Register 2 low */
    pdu.data[5] = 0x00;  /* Register 3 high */
    pdu.data[6] = 0x64;  /* Register 3 low */
    pdu.data_length = 7;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(8, length);
    TEST_ASSERT_EQUAL_HEX8(0x03, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x06, buffer[1]);
}

/* ==========================================================================
 * Test Cases - Read Input Registers (FC04)
 * ========================================================================== */

/**
 * @brief Test PDU encoding for Read Input Registers request
 */
void test_pdu_read_input_regs_request(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build Read Input Registers request: Start=0x0008, Quantity=1 */
    pdu.function_code = MODBUS_FC_READ_INPUT_REGISTERS;
    pdu.data[0] = 0x00;
    pdu.data[1] = 0x08;
    pdu.data[2] = 0x00;
    pdu.data[3] = 0x01;
    pdu.data_length = 4;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(5, length);
    TEST_ASSERT_EQUAL_HEX8(0x04, buffer[0]);  /* FC04 */
}

/* ==========================================================================
 * Test Cases - Write Single Coil (FC05)
 * ========================================================================== */

/**
 * @brief Test PDU encoding for Write Single Coil ON
 */
void test_pdu_write_single_coil_on(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build Write Single Coil ON: Address=0x00AC, Value=0xFF00 */
    pdu.function_code = MODBUS_FC_WRITE_SINGLE_COIL;
    pdu.data[0] = 0x00;  /* Address high */
    pdu.data[1] = 0xAC;  /* Address low */
    pdu.data[2] = 0xFF;  /* Value high (ON) */
    pdu.data[3] = 0x00;  /* Value low */
    pdu.data_length = 4;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(5, length);
    TEST_ASSERT_EQUAL_HEX8(0x05, buffer[0]);  /* FC05 */
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0xAC, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, buffer[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[4]);
}

/**
 * @brief Test PDU encoding for Write Single Coil OFF
 */
void test_pdu_write_single_coil_off(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build Write Single Coil OFF: Address=0x00AC, Value=0x0000 */
    pdu.function_code = MODBUS_FC_WRITE_SINGLE_COIL;
    pdu.data[0] = 0x00;
    pdu.data[1] = 0xAC;
    pdu.data[2] = 0x00;  /* Value high (OFF) */
    pdu.data[3] = 0x00;  /* Value low */
    pdu.data_length = 4;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[4]);
}

/* ==========================================================================
 * Test Cases - Write Single Register (FC06)
 * ========================================================================== */

/**
 * @brief Test PDU encoding for Write Single Register request
 */
void test_pdu_write_single_reg_request(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build Write Single Register: Address=0x0001, Value=0x0003 */
    pdu.function_code = MODBUS_FC_WRITE_SINGLE_REGISTER;
    pdu.data[0] = 0x00;  /* Address high */
    pdu.data[1] = 0x01;  /* Address low */
    pdu.data[2] = 0x00;  /* Value high */
    pdu.data[3] = 0x03;  /* Value low */
    pdu.data_length = 4;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(5, length);
    TEST_ASSERT_EQUAL_HEX8(0x06, buffer[0]);  /* FC06 */
}

/* ==========================================================================
 * Test Cases - Write Multiple Coils (FC15)
 * ========================================================================== */

/**
 * @brief Test PDU encoding for Write Multiple Coils request
 */
void test_pdu_write_multi_coils_request(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build Write Multiple Coils: Start=0x0013, Quantity=10, ByteCount=2 */
    pdu.function_code = MODBUS_FC_WRITE_MULTIPLE_COILS;
    pdu.data[0] = 0x00;  /* Start address high */
    pdu.data[1] = 0x13;  /* Start address low */
    pdu.data[2] = 0x00;  /* Quantity high */
    pdu.data[3] = 0x0A;  /* Quantity low (10) */
    pdu.data[4] = 0x02;  /* Byte count */
    pdu.data[5] = 0xCD;  /* Coil values */
    pdu.data[6] = 0x01;  /* Coil values */
    pdu.data_length = 7;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(8, length);
    TEST_ASSERT_EQUAL_HEX8(0x0F, buffer[0]);  /* FC15 = 0x0F */
}

/* ==========================================================================
 * Test Cases - Write Multiple Registers (FC16)
 * ========================================================================== */

/**
 * @brief Test PDU encoding for Write Multiple Registers request
 */
void test_pdu_write_multi_regs_request(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build Write Multiple Registers: Start=0x0001, Quantity=2, ByteCount=4 */
    pdu.function_code = MODBUS_FC_WRITE_MULTIPLE_REGISTERS;
    pdu.data[0] = 0x00;  /* Start address high */
    pdu.data[1] = 0x01;  /* Start address low */
    pdu.data[2] = 0x00;  /* Quantity high */
    pdu.data[3] = 0x02;  /* Quantity low */
    pdu.data[4] = 0x04;  /* Byte count */
    pdu.data[5] = 0x00;  /* Register 1 high */
    pdu.data[6] = 0x0A;  /* Register 1 low */
    pdu.data[7] = 0x01;  /* Register 2 high */
    pdu.data[8] = 0x02;  /* Register 2 low */
    pdu.data_length = 9;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(10, length);
    TEST_ASSERT_EQUAL_HEX8(0x10, buffer[0]);  /* FC16 = 0x10 */
}

/* ==========================================================================
 * Test Cases - Exception Response
 * ========================================================================== */

/**
 * @brief Test PDU encoding for exception response
 */
void test_pdu_exception_response(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build exception response: FC03 + 0x80, Exception=0x02 */
    pdu.function_code = 0x83;  /* FC03 + 0x80 */
    pdu.data[0] = 0x02;  /* Illegal Data Address */
    pdu.data_length = 1;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_OK, err);
    TEST_ASSERT_EQUAL(2, length);
    TEST_ASSERT_EQUAL_HEX8(0x83, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02, buffer[1]);
}

/* ==========================================================================
 * Test Cases - Serialize/Deserialize Round Trip
 * ========================================================================== */

/**
 * @brief Test PDU serialize and deserialize round trip
 */
void test_pdu_serialize_deserialize(void)
{
    modbus_pdu_t pdu_out, pdu_in;
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    /* Build original PDU */
    pdu_out.function_code = MODBUS_FC_READ_HOLDING_REGISTERS;
    pdu_out.data[0] = 0x00;
    pdu_out.data[1] = 0x6B;
    pdu_out.data[2] = 0x00;
    pdu_out.data[3] = 0x03;
    pdu_out.data_length = 4;

    /* Serialize */
    err = modbus_pdu_serialize(&pdu_out, buffer, sizeof(buffer), &length);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);

    /* Deserialize */
    err = modbus_pdu_deserialize(&pdu_in, buffer, length);
    TEST_ASSERT_EQUAL(MODBUS_OK, err);

    /* Verify */
    TEST_ASSERT_EQUAL(pdu_out.function_code, pdu_in.function_code);
    TEST_ASSERT_EQUAL(pdu_out.data_length, pdu_in.data_length);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(pdu_out.data, pdu_in.data, pdu_out.data_length);
}

/**
 * @brief Test PDU serialize with NULL pointer
 */
void test_pdu_serialize_null_pdu(void)
{
    uint8_t buffer[256];
    uint16_t length;
    modbus_error_t err;

    err = modbus_pdu_serialize(NULL, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test PDU serialize with NULL buffer
 */
void test_pdu_serialize_null_buffer(void)
{
    modbus_pdu_t pdu;
    uint16_t length;
    modbus_error_t err;

    pdu.function_code = MODBUS_FC_READ_COILS;
    pdu.data_length = 4;

    err = modbus_pdu_serialize(&pdu, NULL, 256, &length);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test PDU serialize with buffer too small
 */
void test_pdu_serialize_buffer_small(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[2];  /* Too small */
    uint16_t length;
    modbus_error_t err;

    pdu.function_code = MODBUS_FC_READ_COILS;
    pdu.data[0] = 0x00;
    pdu.data[1] = 0x00;
    pdu.data[2] = 0x00;
    pdu.data[3] = 0x0A;
    pdu.data_length = 4;

    err = modbus_pdu_serialize(&pdu, buffer, sizeof(buffer), &length);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_BUFFER_OVERFLOW, err);
}

/**
 * @brief Test PDU deserialize with NULL pointer
 */
void test_pdu_deserialize_null_pdu(void)
{
    uint8_t buffer[] = {0x03, 0x00, 0x00, 0x00, 0x0A};
    modbus_error_t err;

    err = modbus_pdu_deserialize(NULL, buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL(MODBUS_ERROR_INVALID_PARAM, err);
}

/**
 * @brief Test PDU deserialize with empty buffer
 */
void test_pdu_deserialize_empty(void)
{
    modbus_pdu_t pdu;
    uint8_t buffer[] = {0};
    modbus_error_t err;

    err = modbus_pdu_deserialize(&pdu, buffer, 0);

    TEST_ASSERT_EQUAL(MODBUS_ERROR_FRAME, err);
}

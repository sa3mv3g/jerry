/**
 * @file modbus_pdu.c
 * @brief Modbus Protocol Data Unit (PDU) encoding and decoding
 *
 * This module handles the encoding and decoding of Modbus PDUs.
 * The PDU is protocol-independent and contains the function code and data.
 *
 * @copyright Copyright (c) 2026
 */

#include <string.h>

#include "modbus_config.h"
#include "modbus_internal.h"
#include "modbus_types.h"

/* ==========================================================================
 * Private Constants
 * ========================================================================== */

/** Coil ON value in Modbus protocol */
#define MODBUS_COIL_ON 0xFF00U

/** Coil OFF value in Modbus protocol */
#define MODBUS_COIL_OFF 0x0000U

/* ==========================================================================
 * Private Helper Functions
 * ========================================================================== */

/**
 * @brief Write a 16-bit value to buffer in big-endian format
 *
 * @param[out] buffer Pointer to buffer
 * @param[in] value Value to write
 */
static void pdu_write_uint16_be(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)(value >> 8U);
    buffer[1] = (uint8_t)(value & 0xFFU);
}

/**
 * @brief Read a 16-bit value from buffer in big-endian format
 *
 * @param[in] buffer Pointer to buffer
 * @return uint16_t Value read from buffer
 */
static uint16_t pdu_read_uint16_be(const uint8_t *buffer)
{
    return (uint16_t)(((uint16_t)buffer[0] << 8U) | (uint16_t)buffer[1]);
}

/* ==========================================================================
 * PDU Encoding Functions - Request Building
 * ========================================================================== */

/**
 * @brief Encode a Read Coils (FC01) request PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] start_address Starting address (0-65535)
 * @param[in] quantity Number of coils to read (1-2000)
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_read_coils(modbus_pdu_t *pdu,
                                            uint16_t      start_address,
                                            uint16_t      quantity)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (pdu != NULL)
    {
        if ((quantity > 0U) && (quantity <= MODBUS_MAX_READ_COILS))
        {
            pdu->function_code = MODBUS_FC_READ_COILS;
            pdu_write_uint16_be(&pdu->data[0], start_address);
            pdu_write_uint16_be(&pdu->data[2], quantity);
            pdu->data_length = 4U;
            result           = MODBUS_OK;
        }
    }

    return result;
}

/**
 * @brief Encode a Read Discrete Inputs (FC02) request PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] start_address Starting address (0-65535)
 * @param[in] quantity Number of inputs to read (1-2000)
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_read_discrete_inputs(modbus_pdu_t *pdu,
                                                      uint16_t start_address,
                                                      uint16_t quantity)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (pdu != NULL)
    {
        if ((quantity > 0U) && (quantity <= MODBUS_MAX_READ_DISCRETE))
        {
            pdu->function_code = MODBUS_FC_READ_DISCRETE_INPUTS;
            pdu_write_uint16_be(&pdu->data[0], start_address);
            pdu_write_uint16_be(&pdu->data[2], quantity);
            pdu->data_length = 4U;
            result           = MODBUS_OK;
        }
    }

    return result;
}

/**
 * @brief Encode a Read Holding Registers (FC03) request PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] start_address Starting address (0-65535)
 * @param[in] quantity Number of registers to read (1-125)
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_read_holding_registers(modbus_pdu_t *pdu,
                                                        uint16_t start_address,
                                                        uint16_t quantity)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (pdu != NULL)
    {
        if ((quantity > 0U) && (quantity <= MODBUS_MAX_READ_REGISTERS))
        {
            pdu->function_code = MODBUS_FC_READ_HOLDING_REGISTERS;
            pdu_write_uint16_be(&pdu->data[0], start_address);
            pdu_write_uint16_be(&pdu->data[2], quantity);
            pdu->data_length = 4U;
            result           = MODBUS_OK;
        }
    }

    return result;
}

/**
 * @brief Encode a Read Input Registers (FC04) request PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] start_address Starting address (0-65535)
 * @param[in] quantity Number of registers to read (1-125)
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_read_input_registers(modbus_pdu_t *pdu,
                                                      uint16_t start_address,
                                                      uint16_t quantity)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (pdu != NULL)
    {
        if ((quantity > 0U) && (quantity <= MODBUS_MAX_READ_REGISTERS))
        {
            pdu->function_code = MODBUS_FC_READ_INPUT_REGISTERS;
            pdu_write_uint16_be(&pdu->data[0], start_address);
            pdu_write_uint16_be(&pdu->data[2], quantity);
            pdu->data_length = 4U;
            result           = MODBUS_OK;
        }
    }

    return result;
}

/**
 * @brief Encode a Write Single Coil (FC05) request PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] address Coil address (0-65535)
 * @param[in] value Coil value (true = ON, false = OFF)
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_write_single_coil(modbus_pdu_t *pdu,
                                                   uint16_t address, bool value)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (pdu != NULL)
    {
        pdu->function_code = MODBUS_FC_WRITE_SINGLE_COIL;
        pdu_write_uint16_be(&pdu->data[0], address);
        pdu_write_uint16_be(&pdu->data[2],
                            value ? MODBUS_COIL_ON : MODBUS_COIL_OFF);
        pdu->data_length = 4U;
        result           = MODBUS_OK;
    }

    return result;
}

/**
 * @brief Encode a Write Single Register (FC06) request PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] address Register address (0-65535)
 * @param[in] value Register value
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_write_single_register(modbus_pdu_t *pdu,
                                                       uint16_t      address,
                                                       uint16_t      value)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (pdu != NULL)
    {
        pdu->function_code = MODBUS_FC_WRITE_SINGLE_REGISTER;
        pdu_write_uint16_be(&pdu->data[0], address);
        pdu_write_uint16_be(&pdu->data[2], value);
        pdu->data_length = 4U;
        result           = MODBUS_OK;
    }

    return result;
}

/**
 * @brief Encode a Write Multiple Coils (FC15) request PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] start_address Starting address (0-65535)
 * @param[in] quantity Number of coils to write (1-1968)
 * @param[in] values Pointer to coil values (bit-packed, LSB first)
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_write_multiple_coils(modbus_pdu_t *pdu,
                                                      uint16_t start_address,
                                                      uint16_t quantity,
                                                      const uint8_t *values)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (values != NULL))
    {
        if ((quantity > 0U) && (quantity <= MODBUS_MAX_WRITE_COILS))
        {
            /* Calculate byte count: (quantity + 7) / 8 */
            uint8_t byte_count = (uint8_t)((quantity + 7U) / 8U);

            /* Check if data fits in PDU */
            if ((5U + byte_count) <= (MODBUS_MAX_PDU_SIZE - 1U))
            {
                pdu->function_code = MODBUS_FC_WRITE_MULTIPLE_COILS;
                pdu_write_uint16_be(&pdu->data[0], start_address);
                pdu_write_uint16_be(&pdu->data[2], quantity);
                pdu->data[4] = byte_count;
                (void)memcpy(&pdu->data[5], values, byte_count);
                pdu->data_length = 5U + (uint16_t)byte_count;
                result           = MODBUS_OK;
            }
            else
            {
                result = MODBUS_ERROR_BUFFER_OVERFLOW;
            }
        }
    }

    return result;
}

/**
 * @brief Encode a Write Multiple Registers (FC16) request PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] start_address Starting address (0-65535)
 * @param[in] quantity Number of registers to write (1-123)
 * @param[in] values Pointer to register values
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_write_multiple_registers(
    modbus_pdu_t *pdu, uint16_t start_address, uint16_t quantity,
    const uint16_t *values)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (values != NULL))
    {
        if ((quantity > 0U) && (quantity <= MODBUS_MAX_WRITE_REGISTERS))
        {
            uint8_t byte_count = (uint8_t)(quantity * 2U);

            /* Check if data fits in PDU */
            if ((5U + byte_count) <= (MODBUS_MAX_PDU_SIZE - 1U))
            {
                pdu->function_code = MODBUS_FC_WRITE_MULTIPLE_REGISTERS;
                pdu_write_uint16_be(&pdu->data[0], start_address);
                pdu_write_uint16_be(&pdu->data[2], quantity);
                pdu->data[4] = byte_count;

                /* Copy register values in big-endian format */
                for (uint16_t i = 0U; i < quantity; i++)
                {
                    pdu_write_uint16_be(&pdu->data[5U + (i * 2U)], values[i]);
                }
                pdu->data_length = 5U + (uint16_t)byte_count;
                result           = MODBUS_OK;
            }
            else
            {
                result = MODBUS_ERROR_BUFFER_OVERFLOW;
            }
        }
    }

    return result;
}

/* ==========================================================================
 * PDU Encoding Functions - Response Building
 * ========================================================================== */

/**
 * @brief Encode a Read Coils/Discrete Inputs response PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] function_code Function code (FC01 or FC02)
 * @param[in] coil_values Pointer to coil values (bit-packed)
 * @param[in] quantity Number of coils/inputs
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_read_bits_response(modbus_pdu_t *pdu,
                                                    uint8_t       function_code,
                                                    const uint8_t *coil_values,
                                                    uint16_t       quantity)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (coil_values != NULL))
    {
        uint8_t byte_count = (uint8_t)((quantity + 7U) / 8U);

        if ((1U + byte_count) <= (MODBUS_MAX_PDU_SIZE - 1U))
        {
            pdu->function_code = function_code;
            pdu->data[0]       = byte_count;
            (void)memcpy(&pdu->data[1], coil_values, byte_count);
            pdu->data_length = 1U + (uint16_t)byte_count;
            result           = MODBUS_OK;
        }
        else
        {
            result = MODBUS_ERROR_BUFFER_OVERFLOW;
        }
    }

    return result;
}

/**
 * @brief Encode a Read Registers response PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] function_code Function code (FC03 or FC04)
 * @param[in] register_values Pointer to register values
 * @param[in] quantity Number of registers
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_read_registers_response(
    modbus_pdu_t *pdu, uint8_t function_code, const uint16_t *register_values,
    uint16_t quantity)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (register_values != NULL))
    {
        uint8_t byte_count = (uint8_t)(quantity * 2U);

        if ((1U + byte_count) <= (MODBUS_MAX_PDU_SIZE - 1U))
        {
            pdu->function_code = function_code;
            pdu->data[0]       = byte_count;

            for (uint16_t i = 0U; i < quantity; i++)
            {
                pdu_write_uint16_be(&pdu->data[1U + (i * 2U)],
                                    register_values[i]);
            }
            pdu->data_length = 1U + (uint16_t)byte_count;
            result           = MODBUS_OK;
        }
        else
        {
            result = MODBUS_ERROR_BUFFER_OVERFLOW;
        }
    }

    return result;
}

/**
 * @brief Encode a Write Single Coil/Register response PDU (echo of request)
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] function_code Function code (FC05 or FC06)
 * @param[in] address Address written
 * @param[in] value Value written
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_write_single_response(modbus_pdu_t *pdu,
                                                       uint8_t  function_code,
                                                       uint16_t address,
                                                       uint16_t value)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (pdu != NULL)
    {
        pdu->function_code = function_code;
        pdu_write_uint16_be(&pdu->data[0], address);
        pdu_write_uint16_be(&pdu->data[2], value);
        pdu->data_length = 4U;
        result           = MODBUS_OK;
    }

    return result;
}

/**
 * @brief Encode a Write Multiple Coils/Registers response PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] function_code Function code (FC15 or FC16)
 * @param[in] start_address Starting address
 * @param[in] quantity Number of items written
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_write_multiple_response(modbus_pdu_t *pdu,
                                                         uint8_t  function_code,
                                                         uint16_t start_address,
                                                         uint16_t quantity)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (pdu != NULL)
    {
        pdu->function_code = function_code;
        pdu_write_uint16_be(&pdu->data[0], start_address);
        pdu_write_uint16_be(&pdu->data[2], quantity);
        pdu->data_length = 4U;
        result           = MODBUS_OK;
    }

    return result;
}

/**
 * @brief Encode an exception response PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] function_code Original function code
 * @param[in] exception_code Exception code
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_encode_exception(modbus_pdu_t      *pdu,
                                           uint8_t            function_code,
                                           modbus_exception_t exception_code)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (pdu != NULL)
    {
        /* Exception response has function code with MSB set */
        pdu->function_code = function_code | 0x80U;
        pdu->data[0]       = (uint8_t)exception_code;
        pdu->data_length   = 1U;
        result             = MODBUS_OK;
    }

    return result;
}

/* ==========================================================================
 * PDU Decoding Functions
 * ========================================================================== */

/**
 * @brief Decode a Read Coils/Discrete Inputs request
 *
 * @param[in] pdu Pointer to PDU structure
 * @param[out] start_address Pointer to store starting address
 * @param[out] quantity Pointer to store quantity
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_decode_read_bits_request(const modbus_pdu_t *pdu,
                                                   uint16_t *start_address,
                                                   uint16_t *quantity)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (start_address != NULL) && (quantity != NULL))
    {
        if (pdu->data_length >= 4U)
        {
            *start_address = pdu_read_uint16_be(&pdu->data[0]);
            *quantity      = pdu_read_uint16_be(&pdu->data[2]);
            result         = MODBUS_OK;
        }
        else
        {
            result = MODBUS_ERROR_FRAME;
        }
    }

    return result;
}

/**
 * @brief Decode a Read Registers request
 *
 * @param[in] pdu Pointer to PDU structure
 * @param[out] start_address Pointer to store starting address
 * @param[out] quantity Pointer to store quantity
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_decode_read_registers_request(const modbus_pdu_t *pdu,
                                                        uint16_t *start_address,
                                                        uint16_t *quantity)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (start_address != NULL) && (quantity != NULL))
    {
        if (pdu->data_length >= 4U)
        {
            *start_address = pdu_read_uint16_be(&pdu->data[0]);
            *quantity      = pdu_read_uint16_be(&pdu->data[2]);
            result         = MODBUS_OK;
        }
        else
        {
            result = MODBUS_ERROR_FRAME;
        }
    }

    return result;
}

/**
 * @brief Decode a Write Single Coil request
 *
 * @param[in] pdu Pointer to PDU structure
 * @param[out] address Pointer to store address
 * @param[out] value Pointer to store value (true = ON, false = OFF)
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_decode_write_single_coil_request(
    const modbus_pdu_t *pdu, uint16_t *address, bool *value)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (address != NULL) && (value != NULL))
    {
        if (pdu->data_length >= 4U)
        {
            *address            = pdu_read_uint16_be(&pdu->data[0]);
            uint16_t coil_value = pdu_read_uint16_be(&pdu->data[2]);

            /* Validate coil value - must be 0xFF00 or 0x0000 */
            if ((coil_value == MODBUS_COIL_ON) ||
                (coil_value == MODBUS_COIL_OFF))
            {
                *value = (coil_value == MODBUS_COIL_ON);
                result = MODBUS_OK;
            }
            else
            {
                result = MODBUS_ERROR_FRAME;
            }
        }
        else
        {
            result = MODBUS_ERROR_FRAME;
        }
    }

    return result;
}

/**
 * @brief Decode a Write Single Register request
 *
 * @param[in] pdu Pointer to PDU structure
 * @param[out] address Pointer to store address
 * @param[out] value Pointer to store value
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_decode_write_single_register_request(
    const modbus_pdu_t *pdu, uint16_t *address, uint16_t *value)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (address != NULL) && (value != NULL))
    {
        if (pdu->data_length >= 4U)
        {
            *address = pdu_read_uint16_be(&pdu->data[0]);
            *value   = pdu_read_uint16_be(&pdu->data[2]);
            result   = MODBUS_OK;
        }
        else
        {
            result = MODBUS_ERROR_FRAME;
        }
    }

    return result;
}

/**
 * @brief Decode a Write Multiple Coils request
 *
 * @param[in] pdu Pointer to PDU structure
 * @param[out] start_address Pointer to store starting address
 * @param[out] quantity Pointer to store quantity
 * @param[out] values Pointer to pointer to store coil values location
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_decode_write_multiple_coils_request(
    const modbus_pdu_t *pdu, uint16_t *start_address, uint16_t *quantity,
    const uint8_t **values)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (start_address != NULL) && (quantity != NULL) &&
        (values != NULL))
    {
        if (pdu->data_length >= 5U)
        {
            *start_address     = pdu_read_uint16_be(&pdu->data[0]);
            *quantity          = pdu_read_uint16_be(&pdu->data[2]);
            uint8_t byte_count = pdu->data[4];

            /* Validate byte count */
            uint8_t expected_bytes = (uint8_t)((*quantity + 7U) / 8U);
            if (byte_count == expected_bytes)
            {
                if (pdu->data_length >= (5U + byte_count))
                {
                    *values = &pdu->data[5];
                    result  = MODBUS_OK;
                }
                else
                {
                    result = MODBUS_ERROR_FRAME;
                }
            }
            else
            {
                result = MODBUS_ERROR_FRAME;
            }
        }
        else
        {
            result = MODBUS_ERROR_FRAME;
        }
    }

    return result;
}

/**
 * @brief Decode a Write Multiple Registers request
 *
 * @param[in] pdu Pointer to PDU structure
 * @param[out] start_address Pointer to store starting address
 * @param[out] quantity Pointer to store quantity
 * @param[out] values Pointer to buffer to store register values
 * @param[in] max_values Maximum number of values buffer can hold
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_decode_write_multiple_registers_request(
    const modbus_pdu_t *pdu, uint16_t *start_address, uint16_t *quantity,
    uint16_t *values, uint16_t max_values)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (start_address != NULL) && (quantity != NULL) &&
        (values != NULL))
    {
        if (pdu->data_length >= 5U)
        {
            *start_address     = pdu_read_uint16_be(&pdu->data[0]);
            *quantity          = pdu_read_uint16_be(&pdu->data[2]);
            uint8_t byte_count = pdu->data[4];

            /* Validate byte count */
            if (byte_count == (*quantity * 2U))
            {
                if (*quantity <= max_values)
                {
                    if (pdu->data_length >= (5U + byte_count))
                    {
                        /* Extract register values */
                        for (uint16_t i = 0U; i < *quantity; i++)
                        {
                            values[i] =
                                pdu_read_uint16_be(&pdu->data[5U + (i * 2U)]);
                        }
                        result = MODBUS_OK;
                    }
                    else
                    {
                        result = MODBUS_ERROR_FRAME;
                    }
                }
                else
                {
                    result = MODBUS_ERROR_BUFFER_OVERFLOW;
                }
            }
            else
            {
                result = MODBUS_ERROR_FRAME;
            }
        }
        else
        {
            result = MODBUS_ERROR_FRAME;
        }
    }

    return result;
}

/**
 * @brief Check if PDU is an exception response
 *
 * @param[in] pdu Pointer to PDU structure
 * @return bool true if exception response, false otherwise
 */
bool modbus_pdu_is_exception(const modbus_pdu_t *pdu)
{
    bool result = false;

    if (pdu != NULL)
    {
        result = ((pdu->function_code & 0x80U) != 0U);
    }

    return result;
}

/**
 * @brief Get exception code from exception response PDU
 *
 * @param[in] pdu Pointer to PDU structure
 * @param[out] exception_code Pointer to store exception code
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_get_exception(const modbus_pdu_t *pdu,
                                        modbus_exception_t *exception_code)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (exception_code != NULL))
    {
        if (modbus_pdu_is_exception(pdu))
        {
            if (pdu->data_length >= 1U)
            {
                *exception_code = (modbus_exception_t)pdu->data[0];
                result          = MODBUS_OK;
            }
            else
            {
                result = MODBUS_ERROR_FRAME;
            }
        }
        else
        {
            result = MODBUS_ERROR_FRAME;
        }
    }

    return result;
}

/**
 * @brief Serialize PDU to raw byte buffer
 *
 * @param[in] pdu Pointer to PDU structure
 * @param[out] buffer Output buffer
 * @param[in] buffer_size Size of output buffer
 * @param[out] length Pointer to store actual length written
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_serialize(const modbus_pdu_t *pdu, uint8_t *buffer,
                                    uint16_t buffer_size, uint16_t *length)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (buffer != NULL) && (length != NULL))
    {
        uint16_t total_length =
            1U + pdu->data_length; /* function code + data */

        if (buffer_size >= total_length)
        {
            buffer[0] = pdu->function_code;
            if (pdu->data_length > 0U)
            {
                (void)memcpy(&buffer[1], pdu->data, pdu->data_length);
            }
            *length = total_length;
            result  = MODBUS_OK;
        }
        else
        {
            result = MODBUS_ERROR_BUFFER_OVERFLOW;
        }
    }

    return result;
}

/**
 * @brief Deserialize raw byte buffer to PDU
 *
 * @param[out] pdu Pointer to PDU structure to fill
 * @param[in] buffer Input buffer
 * @param[in] length Length of input data
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_pdu_deserialize(modbus_pdu_t *pdu, const uint8_t *buffer,
                                      uint16_t length)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((pdu != NULL) && (buffer != NULL))
    {
        if (length >= 1U)
        {
            if (length <= MODBUS_MAX_PDU_SIZE)
            {
                pdu->function_code = buffer[0];
                pdu->data_length   = (uint16_t)(length - 1U);

                if (pdu->data_length > 0U)
                {
                    (void)memcpy(pdu->data, &buffer[1], pdu->data_length);
                }
                result = MODBUS_OK;
            }
            else
            {
                result = MODBUS_ERROR_BUFFER_OVERFLOW;
            }
        }
        else
        {
            result = MODBUS_ERROR_FRAME;
        }
    }

    return result;
}

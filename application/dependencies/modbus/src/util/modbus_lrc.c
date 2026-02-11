/**
 * @file modbus_lrc.c
 * @brief Modbus LRC (Longitudinal Redundancy Check) calculation for ASCII
 * protocol
 *
 * This module provides LRC calculation functions used by the Modbus ASCII
 * protocol. LRC is calculated as the two's complement of the 8-bit sum of all
 * bytes.
 *
 * @copyright Copyright (c) 2026
 */

#include <stddef.h>
#include <stdint.h>

#include "modbus.h"
#include "modbus_internal.h"
#include "modbus_types.h"

/**
 * @brief Calculate LRC (Longitudinal Redundancy Check) for Modbus ASCII
 *
 * The LRC is calculated as the two's complement of the 8-bit sum of all bytes
 * in the message (excluding the start ':' and end CR LF characters).
 *
 * @param[in] data Pointer to the data buffer (binary data, not ASCII encoded)
 * @param[in] length Number of bytes in the data buffer
 * @return uint8_t Calculated LRC value
 *
 * @note The LRC is calculated on the binary data before ASCII encoding.
 *       For a message with address 0x01 and function 0x03, the LRC is
 *       calculated on bytes [0x01, 0x03, ...], not on the ASCII characters.
 */
uint8_t modbus_lrc(const uint8_t *data, uint16_t length)
{
    uint8_t        lrc = 0U;
    const uint8_t *ptr = data;
    uint16_t       len = length;

    if ((ptr != NULL) && (len > 0U))
    {
        /* Sum all bytes */
        while (len > 0U)
        {
            lrc += *ptr;
            ptr++;
            len--;
        }

        /* Return two's complement */
        lrc = (uint8_t)(-(int8_t)lrc);
    }

    return lrc;
}

/**
 * @brief Verify LRC of a received Modbus ASCII message
 *
 * @param[in] data Pointer to the data buffer including the LRC byte
 * @param[in] length Total length including the LRC byte
 * @return bool true if LRC is valid, false otherwise
 */
bool modbus_lrc_verify(const uint8_t *data, uint16_t length)
{
    bool result = false;

    if ((data != NULL) && (length >= 2U))
    {
        /* Calculate sum of all bytes including LRC - should be 0 */
        uint8_t sum = 0U;
        for (uint16_t i = 0U; i < length; i++)
        {
            sum += data[i];
        }

        result = (sum == 0U);
    }

    return result;
}

/**
 * @brief Convert a nibble (4 bits) to ASCII hex character
 *
 * @param[in] nibble Value 0-15 to convert
 * @return char ASCII character '0'-'9' or 'A'-'F'
 */
static char nibble_to_ascii(uint8_t nibble)
{
    static const char hex_chars[] = "0123456789ABCDEF";
    return hex_chars[nibble & 0x0FU];
}

/**
 * @brief Convert ASCII hex character to nibble value
 *
 * @param[in] c ASCII character '0'-'9', 'A'-'F', or 'a'-'f'
 * @return int8_t Value 0-15, or -1 if invalid character
 */
static int8_t ascii_to_nibble(char c)
{
    int8_t result = -1;

    if ((c >= '0') && (c <= '9'))
    {
        result = (int8_t)(c - '0');
    }
    else if ((c >= 'A') && (c <= 'F'))
    {
        result = (int8_t)(c - 'A' + 10);
    }
    else if ((c >= 'a') && (c <= 'f'))
    {
        result = (int8_t)(c - 'a' + 10);
    }
    else
    {
        /* result already -1 */
    }

    return result;
}

/**
 * @brief Convert binary byte to two ASCII hex characters
 *
 * @param[in] byte Binary byte to convert
 * @param[out] high_char Pointer to store high nibble ASCII character
 * @param[out] low_char Pointer to store low nibble ASCII character
 */
void modbus_byte_to_ascii(uint8_t byte, char *high_char, char *low_char)
{
    if ((high_char != NULL) && (low_char != NULL))
    {
        *high_char = nibble_to_ascii((byte >> 4U) & 0x0FU);
        *low_char  = nibble_to_ascii(byte & 0x0FU);
    }
}

/**
 * @brief Convert two ASCII hex characters to binary byte
 *
 * @param[in] high_char High nibble ASCII character
 * @param[in] low_char Low nibble ASCII character
 * @param[out] byte Pointer to store the resulting byte
 * @return modbus_error_t MODBUS_OK on success, MODBUS_ERROR_FRAME on invalid
 * chars
 */
modbus_error_t modbus_ascii_to_byte(char high_char, char low_char,
                                    uint8_t *byte)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (byte != NULL)
    {
        int8_t high_nibble = ascii_to_nibble(high_char);
        int8_t low_nibble  = ascii_to_nibble(low_char);

        if ((high_nibble >= 0) && (low_nibble >= 0))
        {
            *byte =
                (uint8_t)(((uint8_t)high_nibble << 4U) | (uint8_t)low_nibble);
            result = MODBUS_OK;
        }
        else
        {
            result = MODBUS_ERROR_FRAME;
        }
    }

    return result;
}

/**
 * @brief Convert binary buffer to ASCII hex string
 *
 * @param[in] binary Pointer to binary data
 * @param[in] binary_len Length of binary data
 * @param[out] ascii Pointer to ASCII output buffer (must be at least
 * 2*binary_len bytes)
 * @param[in] ascii_size Size of ASCII buffer
 * @return uint16_t Number of ASCII characters written, or 0 on error
 */
uint16_t modbus_binary_to_ascii(const uint8_t *binary, uint16_t binary_len,
                                char *ascii, uint16_t ascii_size)
{
    uint16_t result = 0U;

    if ((binary != NULL) && (ascii != NULL) && (binary_len > 0U))
    {
        /* Check if output buffer is large enough */
        uint16_t ascii_len = binary_len * 2U;
        if (ascii_size >= ascii_len)
        {
            for (uint16_t i = 0U; i < binary_len; i++)
            {
                modbus_byte_to_ascii(binary[i], &ascii[i * 2U],
                                     &ascii[(i * 2U) + 1U]);
            }
            result = ascii_len;
        }
    }

    return result;
}

/**
 * @brief Convert ASCII hex string to binary buffer
 *
 * @param[in] ascii Pointer to ASCII hex string
 * @param[in] ascii_len Length of ASCII string (must be even)
 * @param[out] binary Pointer to binary output buffer
 * @param[in] binary_size Size of binary buffer
 * @return uint16_t Number of binary bytes written, or 0 on error
 */
uint16_t modbus_ascii_to_binary(const char *ascii, uint16_t ascii_len,
                                uint8_t *binary, uint16_t binary_size)
{
    uint16_t result        = 0U;
    bool     conversion_ok = true;

    if ((ascii != NULL) && (binary != NULL) && (ascii_len > 0U) &&
        ((ascii_len % 2U) == 0U))
    {
        uint16_t binary_len = ascii_len / 2U;
        if (binary_size >= binary_len)
        {
            for (uint16_t i = 0U; (i < binary_len) && conversion_ok; i++)
            {
                modbus_error_t err = modbus_ascii_to_byte(
                    ascii[i * 2U], ascii[(i * 2U) + 1U], &binary[i]);
                if (err != MODBUS_OK)
                {
                    conversion_ok = false;
                }
            }
            if (conversion_ok)
            {
                result = binary_len;
            }
        }
    }

    return result;
}

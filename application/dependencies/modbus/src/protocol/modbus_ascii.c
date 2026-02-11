/**
 * @file modbus_ascii.c
 * @brief Modbus ASCII protocol framing implementation
 *
 * This module handles the ASCII-specific framing including:
 * - LRC calculation and verification
 * - ASCII encoding/decoding (binary to hex)
 * - Frame delimiters (: start, CR LF end)
 *
 * ASCII Frame Format:
 * [:] [Address (2 chars)] [Function (2 chars)] [Data (0-504 chars)] [LRC (2
 * chars)] [CR] [LF]
 *
 * @copyright Copyright (c) 2026
 */

#include <string.h>

#include "modbus_internal.h"
#include "modbus_types.h"

/* ==========================================================================
 * Private Constants
 * ========================================================================== */

/** ASCII frame start character */
#define MODBUS_ASCII_START_CHAR ':'

/** ASCII frame end character 1 (CR) */
#define MODBUS_ASCII_END_CR '\r'

/** ASCII frame end character 2 (LF) */
#define MODBUS_ASCII_END_LF '\n'

/** Minimum ASCII frame length: : + addr(2) + func(2) + lrc(2) + CR + LF = 9 */
#define MODBUS_ASCII_MIN_FRAME_LEN 9U

/** Maximum ASCII frame length: : + (1+253)*2 + 2 + CR + LF = 513 */
#define MODBUS_ASCII_MAX_FRAME_LEN 513U

/** Maximum binary data length (address + PDU) */
#define MODBUS_ASCII_MAX_BINARY_LEN 254U

/** Broadcast address */
#define MODBUS_ASCII_BROADCAST_ADDR 0U

/* ==========================================================================
 * ASCII Frame Building Functions
 * ========================================================================== */

/**
 * @brief Build an ASCII frame from ADU
 *
 * Constructs a complete ASCII frame including start char, hex-encoded data,
 * LRC, and end characters.
 *
 * @param[in] adu Pointer to ADU structure
 * @param[out] frame Output buffer for the frame (as char array)
 * @param[in] frame_size Size of output buffer
 * @param[out] frame_length Pointer to store actual frame length
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_ascii_build_frame(const modbus_adu_t *adu, char *frame,
                                        uint16_t  frame_size,
                                        uint16_t *frame_length)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((adu != NULL) && (frame != NULL) && (frame_length != NULL))
    {
        uint8_t binary_buffer[MODBUS_ASCII_MAX_BINARY_LEN];

        /* Build binary data: address + PDU */
        binary_buffer[0] = adu->unit_id;

        uint16_t       pdu_len;
        modbus_error_t err =
            modbus_pdu_serialize(&adu->pdu, &binary_buffer[1],
                                 MODBUS_ASCII_MAX_BINARY_LEN - 1U, &pdu_len);
        if (err == MODBUS_OK)
        {
            uint16_t binary_len = 1U + pdu_len; /* address + PDU */

            /* Calculate LRC on binary data */
            uint8_t lrc = modbus_lrc(binary_buffer, binary_len);

            /* Append LRC to binary data */
            binary_buffer[binary_len] = lrc;
            binary_len++;

            /* Calculate required frame size: : + hex_data + CR + LF */
            if (frame_size >= (1U + (binary_len * 2U) + 2U))
            {
                /* Build frame: Start character */
                frame[0] = MODBUS_ASCII_START_CHAR;

                /* Build frame: Hex-encoded data (address + PDU + LRC) */
                uint16_t ascii_len =
                    modbus_binary_to_ascii(binary_buffer, binary_len, &frame[1],
                                           (uint16_t)(frame_size - 3U));
                if (ascii_len > 0U)
                {
                    /* Build frame: End characters */
                    frame[1U + ascii_len]      = MODBUS_ASCII_END_CR;
                    frame[1U + ascii_len + 1U] = MODBUS_ASCII_END_LF;

                    *frame_length = 1U + ascii_len + 2U;
                    result        = MODBUS_OK;
                }
                else
                {
                    result = MODBUS_ERROR_BUFFER_OVERFLOW;
                }
            }
            else
            {
                result = MODBUS_ERROR_BUFFER_OVERFLOW;
            }
        }
        else
        {
            result = err;
        }
    }

    return result;
}

/**
 * @brief Parse an ASCII frame into ADU
 *
 * Parses a received ASCII frame, verifies LRC, and extracts the ADU.
 *
 * @param[in] frame Input frame buffer (as char array)
 * @param[in] frame_length Length of frame data
 * @param[out] adu Pointer to ADU structure to fill
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_ascii_parse_frame(const char   *frame,
                                        uint16_t      frame_length,
                                        modbus_adu_t *adu)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((frame != NULL) && (adu != NULL))
    {
        /* Validate minimum frame length */
        if (frame_length < MODBUS_ASCII_MIN_FRAME_LEN)
        {
            result = MODBUS_ERROR_FRAME;
        }
        else if (frame[0] != MODBUS_ASCII_START_CHAR)
        {
            /* Validate start character */
            result = MODBUS_ERROR_FRAME;
        }
        else if ((frame[frame_length - 2U] != MODBUS_ASCII_END_CR) ||
                 (frame[frame_length - 1U] != MODBUS_ASCII_END_LF))
        {
            /* Validate end characters */
            result = MODBUS_ERROR_FRAME;
        }
        else
        {
            /* Calculate hex data length (excluding : and CR LF) */
            uint16_t hex_len = (uint16_t)(frame_length - 3U);

            /* Hex length must be even */
            if ((hex_len % 2U) != 0U)
            {
                result = MODBUS_ERROR_FRAME;
            }
            else
            {
                uint8_t binary_buffer[MODBUS_ASCII_MAX_BINARY_LEN +
                                      1U]; /* +1 for LRC */

                /* Convert hex to binary */
                uint16_t binary_len = modbus_ascii_to_binary(
                    &frame[1], hex_len, binary_buffer, sizeof(binary_buffer));
                if (binary_len == 0U)
                {
                    result = MODBUS_ERROR_FRAME;
                }
                else if (!modbus_lrc_verify(binary_buffer, binary_len))
                {
                    /* Verify LRC (binary data includes LRC byte) */
                    result =
                        MODBUS_ERROR_CRC; /* Using CRC error for LRC failure */
                }
                else
                {
                    /* Extract address (first byte) */
                    adu->unit_id = binary_buffer[0];

                    /* Extract PDU (excluding address and LRC) */
                    modbus_error_t err =
                        modbus_pdu_deserialize(&adu->pdu, &binary_buffer[1],
                                               (uint16_t)(binary_len - 2U));
                    if (err == MODBUS_OK)
                    {
                        /* Clear TCP-specific fields */
                        adu->transaction_id = 0U;
                        adu->protocol_id    = 0U;
                        result              = MODBUS_OK;
                    }
                    else
                    {
                        result = err;
                    }
                }
            }
        }
    }

    return result;
}

/* ==========================================================================
 * ASCII Frame Receiver State Machine
 * ========================================================================== */

/**
 * @brief ASCII receiver states
 */
typedef enum
{
    ASCII_RX_STATE_IDLE = 0,    /**< Waiting for start character ':' */
    ASCII_RX_STATE_RECEIVING,   /**< Receiving hex data */
    ASCII_RX_STATE_CR_RECEIVED, /**< CR received, waiting for LF */
    ASCII_RX_STATE_COMPLETE,    /**< Frame reception complete */
    ASCII_RX_STATE_ERROR        /**< Reception error */
} modbus_ascii_rx_state_t;

/**
 * @brief ASCII receiver context
 */
typedef struct
{
    modbus_ascii_rx_state_t state;               /**< Current receiver state */
    char     buffer[MODBUS_ASCII_MAX_FRAME_LEN]; /**< Receive buffer */
    uint16_t index;                              /**< Current buffer index */
    uint32_t start_time; /**< Timestamp of frame start */
    uint32_t timeout_ms; /**< Frame timeout in milliseconds */
} modbus_ascii_rx_context_t;

/**
 * @brief Initialize ASCII receiver context
 *
 * @param[out] ctx Pointer to receiver context
 * @param[in] timeout_ms Frame timeout in milliseconds
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_ascii_rx_init(modbus_ascii_rx_context_t *ctx,
                                    uint32_t                   timeout_ms)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (ctx != NULL)
    {
        ctx->state      = ASCII_RX_STATE_IDLE;
        ctx->index      = 0U;
        ctx->start_time = 0U;
        ctx->timeout_ms = timeout_ms;
        result          = MODBUS_OK;
    }

    return result;
}

/**
 * @brief Reset ASCII receiver to idle state
 *
 * @param[in,out] ctx Pointer to receiver context
 */
void modbus_ascii_rx_reset(modbus_ascii_rx_context_t *ctx)
{
    if (ctx != NULL)
    {
        ctx->state = ASCII_RX_STATE_IDLE;
        ctx->index = 0U;
    }
}

/**
 * @brief Process received character in ASCII receiver
 *
 * @param[in,out] ctx Pointer to receiver context
 * @param[in] c Received character
 * @param[in] current_time_ms Current timestamp in milliseconds
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_ascii_rx_process_char(modbus_ascii_rx_context_t *ctx,
                                            char c, uint32_t current_time_ms)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (ctx != NULL)
    {
        result = MODBUS_OK;

        switch (ctx->state)
        {
            case ASCII_RX_STATE_IDLE:
                if (c == MODBUS_ASCII_START_CHAR)
                {
                    /* Start of new frame */
                    ctx->buffer[0]  = c;
                    ctx->index      = 1U;
                    ctx->start_time = current_time_ms;
                    ctx->state      = ASCII_RX_STATE_RECEIVING;
                }
                /* Ignore other characters in idle state */
                break;

            case ASCII_RX_STATE_RECEIVING:
                if (c == MODBUS_ASCII_START_CHAR)
                {
                    /* New frame start - restart reception */
                    ctx->buffer[0]  = c;
                    ctx->index      = 1U;
                    ctx->start_time = current_time_ms;
                }
                else if (c == MODBUS_ASCII_END_CR)
                {
                    /* CR received */
                    if (ctx->index < MODBUS_ASCII_MAX_FRAME_LEN)
                    {
                        ctx->buffer[ctx->index] = c;
                        ctx->index++;
                        ctx->state = ASCII_RX_STATE_CR_RECEIVED;
                    }
                    else
                    {
                        ctx->state = ASCII_RX_STATE_ERROR;
                        result     = MODBUS_ERROR_BUFFER_OVERFLOW;
                    }
                }
                else
                {
                    /* Regular character */
                    if (ctx->index < MODBUS_ASCII_MAX_FRAME_LEN)
                    {
                        ctx->buffer[ctx->index] = c;
                        ctx->index++;
                    }
                    else
                    {
                        ctx->state = ASCII_RX_STATE_ERROR;
                        result     = MODBUS_ERROR_BUFFER_OVERFLOW;
                    }
                }
                break;

            case ASCII_RX_STATE_CR_RECEIVED:
                if (c == MODBUS_ASCII_END_LF)
                {
                    /* Frame complete */
                    if (ctx->index < MODBUS_ASCII_MAX_FRAME_LEN)
                    {
                        ctx->buffer[ctx->index] = c;
                        ctx->index++;
                        ctx->state = ASCII_RX_STATE_COMPLETE;
                    }
                    else
                    {
                        ctx->state = ASCII_RX_STATE_ERROR;
                        result     = MODBUS_ERROR_BUFFER_OVERFLOW;
                    }
                }
                else if (c == MODBUS_ASCII_START_CHAR)
                {
                    /* New frame start - restart reception */
                    ctx->buffer[0]  = c;
                    ctx->index      = 1U;
                    ctx->start_time = current_time_ms;
                    ctx->state      = ASCII_RX_STATE_RECEIVING;
                }
                else
                {
                    /* Invalid character after CR */
                    ctx->state = ASCII_RX_STATE_ERROR;
                    result     = MODBUS_ERROR_FRAME;
                }
                break;

            case ASCII_RX_STATE_COMPLETE:
            case ASCII_RX_STATE_ERROR:
                /* Ignore characters in these states - need explicit reset */
                break;

            default:
                ctx->state = ASCII_RX_STATE_ERROR;
                result     = MODBUS_ERROR_INVALID_STATE;
                break;
        }
    }

    return result;
}

/**
 * @brief Check if frame reception is complete
 *
 * @param[in] ctx Pointer to receiver context
 * @return bool true if frame is complete, false otherwise
 */
bool modbus_ascii_rx_is_complete(const modbus_ascii_rx_context_t *ctx)
{
    bool result = false;

    if (ctx != NULL)
    {
        result = (ctx->state == ASCII_RX_STATE_COMPLETE);
    }

    return result;
}

/**
 * @brief Check if frame reception has timed out
 *
 * @param[in] ctx Pointer to receiver context
 * @param[in] current_time_ms Current timestamp in milliseconds
 * @return bool true if timed out, false otherwise
 */
bool modbus_ascii_rx_is_timeout(const modbus_ascii_rx_context_t *ctx,
                                uint32_t current_time_ms)
{
    bool result = false;

    if (ctx != NULL)
    {
        if ((ctx->state == ASCII_RX_STATE_RECEIVING) ||
            (ctx->state == ASCII_RX_STATE_CR_RECEIVED))
        {
            uint32_t elapsed = current_time_ms - ctx->start_time;
            result           = (elapsed >= ctx->timeout_ms);
        }
    }

    return result;
}

/**
 * @brief Get received frame data
 *
 * @param[in] ctx Pointer to receiver context
 * @param[out] frame Pointer to pointer to store frame data location
 * @param[out] length Pointer to store frame length
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_ascii_rx_get_frame(const modbus_ascii_rx_context_t *ctx,
                                         const char **frame, uint16_t *length)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((ctx != NULL) && (frame != NULL) && (length != NULL))
    {
        if (ctx->state == ASCII_RX_STATE_COMPLETE)
        {
            *frame  = ctx->buffer;
            *length = ctx->index;
            result  = MODBUS_OK;
        }
        else
        {
            result = MODBUS_ERROR_INVALID_STATE;
        }
    }

    return result;
}

/* ==========================================================================
 * ASCII Address Validation
 * ========================================================================== */

/**
 * @brief Check if address matches or is broadcast
 *
 * @param[in] frame_address Address from received frame
 * @param[in] slave_address Configured slave address
 * @return bool true if address matches or is broadcast
 */
bool modbus_ascii_address_match(uint8_t frame_address, uint8_t slave_address)
{
    bool result;

    /* Broadcast address matches all slaves */
    if (frame_address == MODBUS_ASCII_BROADCAST_ADDR)
    {
        result = true;
    }
    else
    {
        /* Direct address match */
        result = (frame_address == slave_address);
    }

    return result;
}

/**
 * @brief Check if address is broadcast
 *
 * @param[in] address Address to check
 * @return bool true if broadcast address
 */
bool modbus_ascii_is_broadcast(uint8_t address)
{
    return (address == MODBUS_ASCII_BROADCAST_ADDR);
}

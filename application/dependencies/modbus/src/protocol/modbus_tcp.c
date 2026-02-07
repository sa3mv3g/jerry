/**
 * @file modbus_tcp.c
 * @brief Modbus TCP/IP protocol framing implementation
 *
 * This module handles the TCP-specific framing including:
 * - MBAP (Modbus Application Protocol) header handling
 * - Transaction ID management
 * - ADU construction and parsing
 *
 * TCP Frame Format (MBAP Header + PDU):
 * [Transaction ID (2)] [Protocol ID (2)] [Length (2)] [Unit ID (1)] [PDU
 * (1-253)]
 *
 * @copyright Copyright (c) 2026
 */

#include <string.h>

#include "modbus_internal.h"
#include "modbus_types.h"

/* ==========================================================================
 * Private Constants
 * ========================================================================== */

/** MBAP header size: Transaction ID (2) + Protocol ID (2) + Length (2) + Unit
 * ID (1) */
#define MODBUS_TCP_MBAP_SIZE 7U

/** Minimum TCP frame size: MBAP header + function code */
#define MODBUS_TCP_MIN_FRAME_SIZE 8U

/** Maximum TCP frame size: MBAP header + max PDU */
#define MODBUS_TCP_MAX_FRAME_SIZE 260U

/** Modbus protocol identifier (always 0 for Modbus) */
#define MODBUS_TCP_PROTOCOL_ID 0U

/* MBAP header field offsets */
#define MBAP_OFFSET_TRANSACTION_ID 0U
#define MBAP_OFFSET_PROTOCOL_ID    2U
#define MBAP_OFFSET_LENGTH         4U
#define MBAP_OFFSET_UNIT_ID        6U
#define MBAP_OFFSET_PDU            7U

/* ==========================================================================
 * Private Helper Functions
 * ========================================================================== */

/**
 * @brief Write a 16-bit value to buffer in big-endian format
 *
 * @param[out] buffer Pointer to buffer
 * @param[in] value Value to write
 */
static void tcp_write_uint16_be(uint8_t *buffer, uint16_t value)
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
static uint16_t tcp_read_uint16_be(const uint8_t *buffer)
{
    return (uint16_t)(((uint16_t)buffer[0] << 8U) | (uint16_t)buffer[1]);
}

/* ==========================================================================
 * TCP Frame Building Functions
 * ========================================================================== */

/**
 * @brief Build a TCP frame from ADU
 *
 * Constructs a complete TCP frame including MBAP header and PDU.
 *
 * @param[in] adu Pointer to ADU structure
 * @param[out] frame Output buffer for the frame
 * @param[in] frame_size Size of output buffer
 * @param[out] frame_length Pointer to store actual frame length
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_tcp_build_frame(const modbus_adu_t *adu, uint8_t *frame,
                                      uint16_t  frame_size,
                                      uint16_t *frame_length)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((adu != NULL) && (frame != NULL) && (frame_length != NULL))
    {
        /* Calculate PDU length */
        uint16_t pdu_length =
            1U + adu->pdu.data_length; /* function code + data */

        /* Calculate total frame length: MBAP header + PDU */
        uint16_t total_length = MODBUS_TCP_MBAP_SIZE + pdu_length;

        if (frame_size >= total_length)
        {
            /* Build MBAP header */
            tcp_write_uint16_be(&frame[MBAP_OFFSET_TRANSACTION_ID],
                                adu->transaction_id);
            tcp_write_uint16_be(&frame[MBAP_OFFSET_PROTOCOL_ID],
                                MODBUS_TCP_PROTOCOL_ID);
            tcp_write_uint16_be(&frame[MBAP_OFFSET_LENGTH],
                                1U + pdu_length); /* Unit ID + PDU */
            frame[MBAP_OFFSET_UNIT_ID] = adu->unit_id;

            /* Serialize PDU */
            modbus_error_t err = modbus_pdu_serialize(
                &adu->pdu, &frame[MBAP_OFFSET_PDU],
                (uint16_t)(frame_size - MODBUS_TCP_MBAP_SIZE), &pdu_length);
            if (err == MODBUS_OK)
            {
                *frame_length = total_length;
                result        = MODBUS_OK;
            }
            else
            {
                result = err;
            }
        }
        else
        {
            result = MODBUS_ERROR_BUFFER_OVERFLOW;
        }
    }

    return result;
}

/**
 * @brief Parse a TCP frame into ADU
 *
 * Parses a received TCP frame and extracts the ADU.
 *
 * @param[in] frame Input frame buffer
 * @param[in] frame_length Length of frame data
 * @param[out] adu Pointer to ADU structure to fill
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_tcp_parse_frame(const uint8_t *frame,
                                      uint16_t frame_length, modbus_adu_t *adu)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((frame != NULL) && (adu != NULL))
    {
        /* Validate minimum frame length */
        if (frame_length < MODBUS_TCP_MIN_FRAME_SIZE)
        {
            result = MODBUS_ERROR_FRAME;
        }
        else if (frame_length > MODBUS_TCP_MAX_FRAME_SIZE)
        {
            /* Validate maximum frame length */
            result = MODBUS_ERROR_FRAME;
        }
        else
        {
            /* Validate protocol ID (must be 0 for Modbus) */
            uint16_t protocol_id =
                tcp_read_uint16_be(&frame[MBAP_OFFSET_PROTOCOL_ID]);
            if (protocol_id != MODBUS_TCP_PROTOCOL_ID)
            {
                result = MODBUS_ERROR_FRAME;
            }
            else
            {
                /* Validate length field */
                uint16_t length_field =
                    tcp_read_uint16_be(&frame[MBAP_OFFSET_LENGTH]);
                if ((MODBUS_TCP_MBAP_SIZE - 1U + length_field) != frame_length)
                {
                    /* Length field doesn't match actual frame length */
                    result = MODBUS_ERROR_FRAME;
                }
                else
                {
                    /* Extract MBAP header fields */
                    adu->transaction_id =
                        tcp_read_uint16_be(&frame[MBAP_OFFSET_TRANSACTION_ID]);
                    adu->protocol_id = protocol_id;
                    adu->unit_id     = frame[MBAP_OFFSET_UNIT_ID];

                    /* Extract PDU */
                    uint16_t pdu_length =
                        (uint16_t)(length_field -
                                   1U); /* Subtract Unit ID byte */
                    modbus_error_t err = modbus_pdu_deserialize(
                        &adu->pdu, &frame[MBAP_OFFSET_PDU], pdu_length);
                    if (err == MODBUS_OK)
                    {
                        result = MODBUS_OK;
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
 * TCP Transaction ID Management
 * ========================================================================== */

/** Current transaction ID counter */
static uint16_t s_transaction_id = 0U;

/**
 * @brief Get next transaction ID
 *
 * @return uint16_t Next transaction ID
 */
uint16_t modbus_tcp_get_next_transaction_id(void)
{
    uint16_t id = s_transaction_id;
    s_transaction_id++;
    return id;
}

/**
 * @brief Reset transaction ID counter
 */
void modbus_tcp_reset_transaction_id(void) { s_transaction_id = 0U; }

/**
 * @brief Set transaction ID counter
 *
 * @param[in] id New transaction ID value
 */
void modbus_tcp_set_transaction_id(uint16_t id) { s_transaction_id = id; }

/* ==========================================================================
 * TCP Frame Receiver State Machine
 * ========================================================================== */

/**
 * @brief TCP receiver states
 */
typedef enum
{
    TCP_RX_STATE_HEADER = 0, /**< Receiving MBAP header */
    TCP_RX_STATE_PDU,        /**< Receiving PDU data */
    TCP_RX_STATE_COMPLETE,   /**< Frame reception complete */
    TCP_RX_STATE_ERROR       /**< Reception error */
} modbus_tcp_rx_state_t;

/**
 * @brief TCP receiver context
 */
typedef struct
{
    modbus_tcp_rx_state_t state;                /**< Current receiver state */
    uint8_t  buffer[MODBUS_TCP_MAX_FRAME_SIZE]; /**< Receive buffer */
    uint16_t index;                             /**< Current buffer index */
    uint16_t expected_length; /**< Expected total frame length */
    uint32_t start_time;      /**< Timestamp of frame start */
    uint32_t timeout_ms;      /**< Frame timeout in milliseconds */
} modbus_tcp_rx_context_t;

/**
 * @brief Initialize TCP receiver context
 *
 * @param[out] ctx Pointer to receiver context
 * @param[in] timeout_ms Frame timeout in milliseconds
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_tcp_rx_init(modbus_tcp_rx_context_t *ctx,
                                  uint32_t                 timeout_ms)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (ctx != NULL)
    {
        ctx->state           = TCP_RX_STATE_HEADER;
        ctx->index           = 0U;
        ctx->expected_length = MODBUS_TCP_MBAP_SIZE;
        ctx->start_time      = 0U;
        ctx->timeout_ms      = timeout_ms;
        result               = MODBUS_OK;
    }

    return result;
}

/**
 * @brief Reset TCP receiver to initial state
 *
 * @param[in,out] ctx Pointer to receiver context
 */
void modbus_tcp_rx_reset(modbus_tcp_rx_context_t *ctx)
{
    if (ctx != NULL)
    {
        ctx->state           = TCP_RX_STATE_HEADER;
        ctx->index           = 0U;
        ctx->expected_length = MODBUS_TCP_MBAP_SIZE;
    }
}

/**
 * @brief Process received data in TCP receiver
 *
 * @param[in,out] ctx Pointer to receiver context
 * @param[in] data Pointer to received data
 * @param[in] length Length of received data
 * @param[in] current_time_ms Current timestamp in milliseconds
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_tcp_rx_process_data(modbus_tcp_rx_context_t *ctx,
                                          const uint8_t *data, uint16_t length,
                                          uint32_t current_time_ms)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((ctx != NULL) && (data != NULL))
    {
        if ((ctx->state == TCP_RX_STATE_COMPLETE) ||
            (ctx->state == TCP_RX_STATE_ERROR))
        {
            result = MODBUS_ERROR_INVALID_STATE;
        }
        else
        {
            result          = MODBUS_OK;
            bool processing = true;

            /* Record start time on first byte */
            if (ctx->index == 0U)
            {
                ctx->start_time = current_time_ms;
            }

            uint16_t offset = 0U;
            while ((offset < length) && processing && (result == MODBUS_OK))
            {
                uint16_t remaining =
                    (uint16_t)(ctx->expected_length - ctx->index);
                uint16_t bytes_to_copy =
                    ((uint16_t)(length - offset) < remaining)
                        ? (uint16_t)(length - offset)
                        : remaining;

                /* Check buffer overflow */
                if ((ctx->index + bytes_to_copy) > MODBUS_TCP_MAX_FRAME_SIZE)
                {
                    ctx->state = TCP_RX_STATE_ERROR;
                    result     = MODBUS_ERROR_BUFFER_OVERFLOW;
                }
                else
                {
                    /* Copy data to buffer */
                    (void)memcpy(&ctx->buffer[ctx->index], &data[offset],
                                 bytes_to_copy);
                    ctx->index += bytes_to_copy;
                    offset += bytes_to_copy;

                    /* Check if current phase is complete */
                    if (ctx->index >= ctx->expected_length)
                    {
                        if (ctx->state == TCP_RX_STATE_HEADER)
                        {
                            /* Header complete - validate and get PDU length */
                            uint16_t protocol_id = tcp_read_uint16_be(
                                &ctx->buffer[MBAP_OFFSET_PROTOCOL_ID]);
                            uint16_t length_field = tcp_read_uint16_be(
                                &ctx->buffer[MBAP_OFFSET_LENGTH]);

                            /* Validate protocol ID */
                            if (protocol_id != MODBUS_TCP_PROTOCOL_ID)
                            {
                                ctx->state = TCP_RX_STATE_ERROR;
                                result     = MODBUS_ERROR_FRAME;
                            }
                            else if ((length_field < 2U) ||
                                     (length_field > 254U))
                            {
                                /* Validate length field */
                                ctx->state = TCP_RX_STATE_ERROR;
                                result     = MODBUS_ERROR_FRAME;
                            }
                            else
                            {
                                /* Calculate expected total length */
                                ctx->expected_length =
                                    MODBUS_TCP_MBAP_SIZE - 1U + length_field;

                                if (ctx->expected_length >
                                    MODBUS_TCP_MAX_FRAME_SIZE)
                                {
                                    ctx->state = TCP_RX_STATE_ERROR;
                                    result     = MODBUS_ERROR_FRAME;
                                }
                                else
                                {
                                    ctx->state = TCP_RX_STATE_PDU;
                                }
                            }
                        }
                        else if (ctx->state == TCP_RX_STATE_PDU)
                        {
                            /* Frame complete */
                            ctx->state = TCP_RX_STATE_COMPLETE;
                            processing = false;
                        }
                        else
                        {
                            /* MISRA C:2012 Rule 15.7 - All if...else if
                             * constructs shall be terminated with an else
                             * statement. This else handles unexpected states
                             * (COMPLETE or ERROR) which should not occur here
                             * due to the check at the beginning of the
                             * function. */
                        }
                    }
                }
            }
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
bool modbus_tcp_rx_is_complete(const modbus_tcp_rx_context_t *ctx)
{
    bool result = false;

    if (ctx != NULL)
    {
        result = (ctx->state == TCP_RX_STATE_COMPLETE);
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
bool modbus_tcp_rx_is_timeout(const modbus_tcp_rx_context_t *ctx,
                              uint32_t                       current_time_ms)
{
    bool result = false;

    if (ctx != NULL)
    {
        if ((ctx->state != TCP_RX_STATE_COMPLETE) &&
            (ctx->state != TCP_RX_STATE_ERROR) && (ctx->index > 0U))
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
modbus_error_t modbus_tcp_rx_get_frame(const modbus_tcp_rx_context_t *ctx,
                                       const uint8_t **frame, uint16_t *length)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((ctx != NULL) && (frame != NULL) && (length != NULL))
    {
        if (ctx->state == TCP_RX_STATE_COMPLETE)
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
 * TCP Frame Utility Functions
 * ========================================================================== */

/**
 * @brief Get MBAP header length field from frame
 *
 * @param[in] frame Pointer to frame buffer (must have at least 6 bytes)
 * @return uint16_t Length field value
 */
uint16_t modbus_tcp_get_length_field(const uint8_t *frame)
{
    uint16_t result = 0U;

    if (frame != NULL)
    {
        result = tcp_read_uint16_be(&frame[MBAP_OFFSET_LENGTH]);
    }

    return result;
}

/**
 * @brief Get transaction ID from frame
 *
 * @param[in] frame Pointer to frame buffer (must have at least 2 bytes)
 * @return uint16_t Transaction ID
 */
uint16_t modbus_tcp_get_transaction_id(const uint8_t *frame)
{
    uint16_t result = 0U;

    if (frame != NULL)
    {
        result = tcp_read_uint16_be(&frame[MBAP_OFFSET_TRANSACTION_ID]);
    }

    return result;
}

/**
 * @brief Get unit ID from frame
 *
 * @param[in] frame Pointer to frame buffer (must have at least 7 bytes)
 * @return uint8_t Unit ID
 */
uint8_t modbus_tcp_get_unit_id(const uint8_t *frame)
{
    uint8_t result = 0U;

    if (frame != NULL)
    {
        result = frame[MBAP_OFFSET_UNIT_ID];
    }

    return result;
}

/**
 * @file modbus_rtu.c
 * @brief Modbus RTU protocol framing implementation
 *
 * This module handles the RTU-specific framing including:
 * - CRC-16 calculation and verification
 * - Frame timing (3.5 character inter-frame delay)
 * - ADU construction and parsing
 *
 * RTU Frame Format:
 * [Address (1 byte)] [Function Code (1 byte)] [Data (0-252 bytes)] [CRC (2
 * bytes)]
 *
 * @copyright Copyright (c) 2026
 */

#include <string.h>

#include "modbus_internal.h"
#include "modbus_types.h"

/* ==========================================================================
 * Private Constants
 * ========================================================================== */

/** RTU frame minimum size: address (1) + function (1) + CRC (2) */
#define MODBUS_RTU_MIN_FRAME_SIZE 4U

/** RTU frame maximum size: address (1) + PDU (253) + CRC (2) */
#define MODBUS_RTU_MAX_FRAME_SIZE 256U

/** CRC size in bytes */
#define MODBUS_RTU_CRC_SIZE 2U

/** Address field size */
#define MODBUS_RTU_ADDRESS_SIZE 1U

/** Broadcast address */
#define MODBUS_RTU_BROADCAST_ADDR 0U

/* ==========================================================================
 * RTU Frame Building Functions
 * ========================================================================== */

/**
 * @brief Build an RTU frame from ADU
 *
 * Constructs a complete RTU frame including address, PDU, and CRC.
 *
 * @param[in] adu Pointer to ADU structure
 * @param[out] frame Output buffer for the frame
 * @param[in] frame_size Size of output buffer
 * @param[out] frame_length Pointer to store actual frame length
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_rtu_build_frame(const modbus_adu_t *adu, uint8_t *frame,
                                      uint16_t  frame_size,
                                      uint16_t *frame_length) {
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((adu != NULL) && (frame != NULL) && (frame_length != NULL)) {
        /* Calculate total frame length: address + PDU + CRC */
        uint16_t pdu_length =
            1U + adu->pdu.data_length; /* function code + data */
        uint16_t total_length =
            MODBUS_RTU_ADDRESS_SIZE + pdu_length + MODBUS_RTU_CRC_SIZE;

        if (frame_size >= total_length) {
            /* Build frame: Address */
            frame[0] = adu->unit_id;

            /* Build frame: PDU */
            modbus_error_t err = modbus_pdu_serialize(
                &adu->pdu, &frame[1], (uint16_t)(frame_size - 1U), &pdu_length);
            if (err == MODBUS_OK) {
                /* Calculate and append CRC (little-endian) */
                uint16_t crc =
                    modbus_crc16(frame, MODBUS_RTU_ADDRESS_SIZE + pdu_length);
                frame[MODBUS_RTU_ADDRESS_SIZE + pdu_length] =
                    (uint8_t)(crc & 0xFFU);
                frame[MODBUS_RTU_ADDRESS_SIZE + pdu_length + 1U] =
                    (uint8_t)(crc >> 8U);

                *frame_length =
                    MODBUS_RTU_ADDRESS_SIZE + pdu_length + MODBUS_RTU_CRC_SIZE;
                result = MODBUS_OK;
            } else {
                result = err;
            }
        } else {
            result = MODBUS_ERROR_BUFFER_OVERFLOW;
        }
    }

    return result;
}

/**
 * @brief Parse an RTU frame into ADU
 *
 * Parses a received RTU frame, verifies CRC, and extracts the ADU.
 *
 * @param[in] frame Input frame buffer
 * @param[in] frame_length Length of frame data
 * @param[out] adu Pointer to ADU structure to fill
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_rtu_parse_frame(const uint8_t *frame,
                                      uint16_t       frame_length,
                                      modbus_adu_t  *adu) {
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((frame != NULL) && (adu != NULL)) {
        /* Validate minimum frame length */
        if (frame_length < MODBUS_RTU_MIN_FRAME_SIZE) {
            result = MODBUS_ERROR_FRAME;
        } else if (frame_length > MODBUS_RTU_MAX_FRAME_SIZE) {
            /* Validate maximum frame length */
            result = MODBUS_ERROR_FRAME;
        } else if (!modbus_crc16_verify(frame, frame_length)) {
            /* Verify CRC */
            result = MODBUS_ERROR_CRC;
        } else {
            /* Extract address */
            adu->unit_id = frame[0];

            /* Extract PDU (excluding address and CRC) */
            uint16_t pdu_length =
                (uint16_t)(frame_length - MODBUS_RTU_ADDRESS_SIZE -
                           MODBUS_RTU_CRC_SIZE);
            modbus_error_t err =
                modbus_pdu_deserialize(&adu->pdu, &frame[1], pdu_length);
            if (err == MODBUS_OK) {
                /* Clear TCP-specific fields */
                adu->transaction_id = 0U;
                adu->protocol_id    = 0U;
                result              = MODBUS_OK;
            } else {
                result = err;
            }
        }
    }

    return result;
}

/* ==========================================================================
 * RTU Timing Functions
 * ========================================================================== */

/**
 * @brief Calculate inter-frame delay in microseconds for given baud rate
 *
 * The inter-frame delay (t3.5) is the time for 3.5 characters.
 * For baud rates > 19200, a fixed delay of 1750us is used.
 *
 * @param[in] baudrate Baud rate in bits per second
 * @return uint32_t Inter-frame delay in microseconds
 */
uint32_t modbus_rtu_get_interframe_delay_us(uint32_t baudrate) {
    uint32_t result;

    if (baudrate == 0U) {
        result = 1750U; /* Default for invalid baud rate */
    } else if (baudrate > 19200U) {
        /* For baud rates > 19200, use fixed 1750us (per Modbus spec) */
        result = 1750U;
    } else {
        /*
         * Calculate character time:
         * 1 character = 11 bits (1 start + 8 data + 1 parity + 1 stop)
         * Time for 3.5 characters = 3.5 * 11 * 1000000 / baudrate
         * = 38500000 / baudrate
         */
        result = (38500000U + (baudrate / 2U)) / baudrate;
    }

    return result;
}

/**
 * @brief Calculate inter-character timeout in microseconds
 *
 * The inter-character timeout (t1.5) is the time for 1.5 characters.
 * If this timeout expires during reception, the frame is considered complete.
 *
 * @param[in] baudrate Baud rate in bits per second
 * @return uint32_t Inter-character timeout in microseconds
 */
uint32_t modbus_rtu_get_interchar_timeout_us(uint32_t baudrate) {
    uint32_t result;

    if (baudrate == 0U) {
        result = 750U; /* Default for invalid baud rate */
    } else if (baudrate > 19200U) {
        /* For baud rates > 19200, use fixed 750us (per Modbus spec) */
        result = 750U;
    } else {
        /*
         * Calculate character time:
         * 1 character = 11 bits
         * Time for 1.5 characters = 1.5 * 11 * 1000000 / baudrate
         * = 16500000 / baudrate
         */
        result = (16500000U + (baudrate / 2U)) / baudrate;
    }

    return result;
}

/* ==========================================================================
 * RTU Frame Receiver State Machine
 * ========================================================================== */

/**
 * @brief RTU receiver states
 */
typedef enum {
    RTU_RX_STATE_IDLE = 0,  /**< Waiting for start of frame */
    RTU_RX_STATE_RECEIVING, /**< Receiving frame data */
    RTU_RX_STATE_COMPLETE,  /**< Frame reception complete */
    RTU_RX_STATE_ERROR      /**< Reception error */
} modbus_rtu_rx_state_t;

/**
 * @brief RTU receiver context
 */
typedef struct {
    modbus_rtu_rx_state_t state;                /**< Current receiver state */
    uint8_t  buffer[MODBUS_RTU_MAX_FRAME_SIZE]; /**< Receive buffer */
    uint16_t index;                             /**< Current buffer index */
    uint32_t last_byte_time;       /**< Timestamp of last byte received */
    uint32_t interchar_timeout_us; /**< Inter-character timeout */
    uint32_t interframe_delay_us;  /**< Inter-frame delay */
} modbus_rtu_rx_context_t;

/**
 * @brief Initialize RTU receiver context
 *
 * @param[out] ctx Pointer to receiver context
 * @param[in] baudrate Baud rate for timing calculations
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_rtu_rx_init(modbus_rtu_rx_context_t *ctx,
                                  uint32_t                 baudrate) {
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (ctx != NULL) {
        ctx->state          = RTU_RX_STATE_IDLE;
        ctx->index          = 0U;
        ctx->last_byte_time = 0U;
        ctx->interchar_timeout_us =
            modbus_rtu_get_interchar_timeout_us(baudrate);
        ctx->interframe_delay_us = modbus_rtu_get_interframe_delay_us(baudrate);
        result                   = MODBUS_OK;
    }

    return result;
}

/**
 * @brief Reset RTU receiver to idle state
 *
 * @param[in,out] ctx Pointer to receiver context
 */
void modbus_rtu_rx_reset(modbus_rtu_rx_context_t *ctx) {
    if (ctx != NULL) {
        ctx->state = RTU_RX_STATE_IDLE;
        ctx->index = 0U;
    }
}

/**
 * @brief Process received byte in RTU receiver
 *
 * @param[in,out] ctx Pointer to receiver context
 * @param[in] byte Received byte
 * @param[in] current_time_us Current timestamp in microseconds
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_rtu_rx_process_byte(modbus_rtu_rx_context_t *ctx,
                                          uint8_t                  byte,
                                          uint32_t current_time_us) {
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (ctx != NULL) {
        result = MODBUS_OK;

        switch (ctx->state) {
            case RTU_RX_STATE_IDLE:
                /* Start of new frame */
                ctx->buffer[0]      = byte;
                ctx->index          = 1U;
                ctx->last_byte_time = current_time_us;
                ctx->state          = RTU_RX_STATE_RECEIVING;
                break;

            case RTU_RX_STATE_RECEIVING: {
                /* Check inter-character timeout */
                uint32_t elapsed = current_time_us - ctx->last_byte_time;
                if (elapsed > ctx->interchar_timeout_us) {
                    /* Timeout - start new frame */
                    ctx->buffer[0] = byte;
                    ctx->index     = 1U;
                } else {
                    /* Add byte to frame */
                    if (ctx->index < MODBUS_RTU_MAX_FRAME_SIZE) {
                        ctx->buffer[ctx->index] = byte;
                        ctx->index++;
                    } else {
                        /* Buffer overflow */
                        ctx->state = RTU_RX_STATE_ERROR;
                        result     = MODBUS_ERROR_BUFFER_OVERFLOW;
                    }
                }
                ctx->last_byte_time = current_time_us;
                break;
            }

            case RTU_RX_STATE_COMPLETE:
            case RTU_RX_STATE_ERROR:
                /* Ignore bytes in these states - need explicit reset */
                break;

            default:
                ctx->state = RTU_RX_STATE_ERROR;
                result     = MODBUS_ERROR_INVALID_STATE;
                break;
        }
    }

    return result;
}

/**
 * @brief Check if frame reception is complete (inter-frame delay expired)
 *
 * @param[in,out] ctx Pointer to receiver context
 * @param[in] current_time_us Current timestamp in microseconds
 * @return bool true if frame is complete, false otherwise
 */
bool modbus_rtu_rx_is_complete(modbus_rtu_rx_context_t *ctx,
                               uint32_t                 current_time_us) {
    bool result = false;

    if (ctx != NULL) {
        if (ctx->state == RTU_RX_STATE_COMPLETE) {
            result = true;
        } else if (ctx->state == RTU_RX_STATE_RECEIVING) {
            /* Check if inter-frame delay has expired */
            uint32_t elapsed = current_time_us - ctx->last_byte_time;
            if (elapsed >= ctx->interframe_delay_us) {
                /* Frame complete - check minimum length */
                if (ctx->index >= MODBUS_RTU_MIN_FRAME_SIZE) {
                    ctx->state = RTU_RX_STATE_COMPLETE;
                    result     = true;
                } else {
                    /* Frame too short */
                    ctx->state = RTU_RX_STATE_ERROR;
                }
            }
        } else {
            /* IDLE or ERROR state - not complete */
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
modbus_error_t modbus_rtu_rx_get_frame(const modbus_rtu_rx_context_t *ctx,
                                       const uint8_t                **frame,
                                       uint16_t                      *length) {
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((ctx != NULL) && (frame != NULL) && (length != NULL)) {
        if (ctx->state == RTU_RX_STATE_COMPLETE) {
            *frame  = ctx->buffer;
            *length = ctx->index;
            result  = MODBUS_OK;
        } else {
            result = MODBUS_ERROR_INVALID_STATE;
        }
    }

    return result;
}

/* ==========================================================================
 * RTU Address Validation
 * ========================================================================== */

/**
 * @brief Check if address matches or is broadcast
 *
 * @param[in] frame_address Address from received frame
 * @param[in] slave_address Configured slave address
 * @return bool true if address matches or is broadcast
 */
bool modbus_rtu_address_match(uint8_t frame_address, uint8_t slave_address) {
    bool result;

    /* Broadcast address matches all slaves */
    if (frame_address == MODBUS_RTU_BROADCAST_ADDR) {
        result = true;
    } else {
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
bool modbus_rtu_is_broadcast(uint8_t address) {
    return (address == MODBUS_RTU_BROADCAST_ADDR);
}

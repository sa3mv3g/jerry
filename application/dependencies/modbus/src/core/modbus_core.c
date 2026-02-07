/**
 * @file modbus_core.c
 * @brief Modbus core state machine and request processing
 *
 * This module implements the core Modbus functionality including:
 * - Context management
 * - Request/response processing
 * - Callback invocation for data access
 * - State machine for slave/master operation
 *
 * @copyright Copyright (c) 2026
 */

#include <string.h>

#include "modbus.h"
#include "modbus_callbacks.h"
#include "modbus_internal.h"
#include "modbus_types.h"

/* ==========================================================================
 * Private Constants
 * ========================================================================== */

/** Maximum number of coils that can be read */
#define MAX_READ_COILS 2000U

/** Maximum number of registers that can be read */
#define MAX_READ_REGISTERS 125U

/** Maximum number of coils that can be written */
#define MAX_WRITE_COILS 1968U

/** Maximum number of registers that can be written */
#define MAX_WRITE_REGISTERS 123U

/* ==========================================================================
 * Context Structure Definition
 * ========================================================================== */

/**
 * @brief Modbus context structure
 *
 * Contains all state and configuration for a Modbus instance.
 */
struct modbus_context
{
    modbus_config_t config;      /**< Configuration */
    modbus_state_t  state;       /**< Current state */
    bool            initialized; /**< Initialization flag */

    /* Buffers for temporary data */
    uint8_t  coil_buffer[256];     /**< Buffer for coil data */
    uint16_t register_buffer[125]; /**< Buffer for register data */

    /* Statistics */
    uint32_t requests_processed; /**< Number of requests processed */
    uint32_t errors_count;       /**< Number of errors */
    uint32_t exceptions_sent;    /**< Number of exceptions sent */
};

/* ==========================================================================
 * Context Management Functions
 * ========================================================================== */

/**
 * @brief Initialize a Modbus context
 *
 * @param[out] ctx Pointer to context storage (must be at least
 * modbus_context_size() bytes)
 * @param[in] config Pointer to configuration structure
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_init(modbus_context_t *ctx, const modbus_config_t *config)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((ctx != NULL) && (config != NULL))
    {
        /* Initialize context */
        (void)memset(ctx, 0, sizeof(modbus_context_t));
        (void)memcpy(&ctx->config, config, sizeof(modbus_config_t));
        ctx->state       = MODBUS_STATE_IDLE;
        ctx->initialized = true;
        result           = MODBUS_OK;
    }

    return result;
}

/**
 * @brief Get the size of a Modbus context structure
 *
 * @return Size in bytes required for a modbus_context_t
 */
size_t modbus_context_size(void) { return sizeof(modbus_context_t); }

/**
 * @brief Deinitialize a Modbus context
 *
 * @param[in] ctx Pointer to context to deinitialize
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_deinit(modbus_context_t *ctx)
{
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if (ctx != NULL)
    {
        if (!ctx->initialized)
        {
            result = MODBUS_ERROR_NOT_INITIALIZED;
        }
        else
        {
            /* Clear context */
            ctx->initialized = false;
            ctx->state       = MODBUS_STATE_IDLE;
            result           = MODBUS_OK;
        }
    }

    return result;
}

/**
 * @brief Get current state of context
 *
 * @param[in] ctx Pointer to context
 * @return modbus_state_t Current state
 */
modbus_state_t modbus_get_state(const modbus_context_t *ctx)
{
    modbus_state_t result = MODBUS_STATE_ERROR;

    if ((ctx != NULL) && (ctx->initialized))
    {
        result = ctx->state;
    }

    return result;
}

/* ==========================================================================
 * Slave Request Processing Functions
 * ========================================================================== */

/**
 * @brief Process Read Coils (FC01) request
 */
static modbus_error_t process_read_coils(modbus_context_t   *ctx,
                                         const modbus_pdu_t *request,
                                         modbus_pdu_t       *response)
{
    uint16_t           start_address;
    uint16_t           quantity;
    modbus_exception_t exception;
    modbus_error_t     err;
    modbus_error_t     result;

    err =
        modbus_pdu_decode_read_bits_request(request, &start_address, &quantity);
    if (err != MODBUS_OK)
    {
        result =
            modbus_pdu_encode_exception(response, MODBUS_FC_READ_COILS,
                                        MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else if ((quantity == 0U) || (quantity > MAX_READ_COILS))
    {
        /* Validate quantity */
        result =
            modbus_pdu_encode_exception(response, MODBUS_FC_READ_COILS,
                                        MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else
    {
        /* Call user callback */
        exception =
            modbus_cb_read_coils(start_address, quantity, ctx->coil_buffer);
        if (exception != MODBUS_EXCEPTION_NONE)
        {
            ctx->exceptions_sent++;
            result = modbus_pdu_encode_exception(response, MODBUS_FC_READ_COILS,
                                                 exception);
        }
        else
        {
            result = modbus_pdu_encode_read_bits_response(
                response, MODBUS_FC_READ_COILS, ctx->coil_buffer, quantity);
        }
    }

    return result;
}

/**
 * @brief Process Read Discrete Inputs (FC02) request
 */
static modbus_error_t process_read_discrete_inputs(modbus_context_t   *ctx,
                                                   const modbus_pdu_t *request,
                                                   modbus_pdu_t       *response)
{
    uint16_t           start_address;
    uint16_t           quantity;
    modbus_exception_t exception;
    modbus_error_t     err;
    modbus_error_t     result;

    err =
        modbus_pdu_decode_read_bits_request(request, &start_address, &quantity);
    if (err != MODBUS_OK)
    {
        result = modbus_pdu_encode_exception(
            response, MODBUS_FC_READ_DISCRETE_INPUTS,
            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else if ((quantity == 0U) || (quantity > MAX_READ_COILS))
    {
        /* Validate quantity */
        result = modbus_pdu_encode_exception(
            response, MODBUS_FC_READ_DISCRETE_INPUTS,
            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else
    {
        /* Call user callback */
        exception = modbus_cb_read_discrete_inputs(start_address, quantity,
                                                   ctx->coil_buffer);
        if (exception != MODBUS_EXCEPTION_NONE)
        {
            ctx->exceptions_sent++;
            result = modbus_pdu_encode_exception(
                response, MODBUS_FC_READ_DISCRETE_INPUTS, exception);
        }
        else
        {
            result = modbus_pdu_encode_read_bits_response(
                response, MODBUS_FC_READ_DISCRETE_INPUTS, ctx->coil_buffer,
                quantity);
        }
    }

    return result;
}

/**
 * @brief Process Read Holding Registers (FC03) request
 */
static modbus_error_t process_read_holding_registers(
    modbus_context_t *ctx, const modbus_pdu_t *request, modbus_pdu_t *response)
{
    uint16_t           start_address;
    uint16_t           quantity;
    modbus_exception_t exception;
    modbus_error_t     err;
    modbus_error_t     result;

    err = modbus_pdu_decode_read_registers_request(request, &start_address,
                                                   &quantity);
    if (err != MODBUS_OK)
    {
        result = modbus_pdu_encode_exception(
            response, MODBUS_FC_READ_HOLDING_REGISTERS,
            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else if ((quantity == 0U) || (quantity > MAX_READ_REGISTERS))
    {
        /* Validate quantity */
        result = modbus_pdu_encode_exception(
            response, MODBUS_FC_READ_HOLDING_REGISTERS,
            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else
    {
        /* Call user callback */
        exception = modbus_cb_read_holding_registers(start_address, quantity,
                                                     ctx->register_buffer);
        if (exception != MODBUS_EXCEPTION_NONE)
        {
            ctx->exceptions_sent++;
            result = modbus_pdu_encode_exception(
                response, MODBUS_FC_READ_HOLDING_REGISTERS, exception);
        }
        else
        {
            result = modbus_pdu_encode_read_registers_response(
                response, MODBUS_FC_READ_HOLDING_REGISTERS,
                ctx->register_buffer, quantity);
        }
    }

    return result;
}

/**
 * @brief Process Read Input Registers (FC04) request
 */
static modbus_error_t process_read_input_registers(modbus_context_t   *ctx,
                                                   const modbus_pdu_t *request,
                                                   modbus_pdu_t       *response)
{
    uint16_t           start_address;
    uint16_t           quantity;
    modbus_exception_t exception;
    modbus_error_t     err;
    modbus_error_t     result;

    err = modbus_pdu_decode_read_registers_request(request, &start_address,
                                                   &quantity);
    if (err != MODBUS_OK)
    {
        result = modbus_pdu_encode_exception(
            response, MODBUS_FC_READ_INPUT_REGISTERS,
            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else if ((quantity == 0U) || (quantity > MAX_READ_REGISTERS))
    {
        /* Validate quantity */
        result = modbus_pdu_encode_exception(
            response, MODBUS_FC_READ_INPUT_REGISTERS,
            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else
    {
        /* Call user callback */
        exception = modbus_cb_read_input_registers(start_address, quantity,
                                                   ctx->register_buffer);
        if (exception != MODBUS_EXCEPTION_NONE)
        {
            ctx->exceptions_sent++;
            result = modbus_pdu_encode_exception(
                response, MODBUS_FC_READ_INPUT_REGISTERS, exception);
        }
        else
        {
            result = modbus_pdu_encode_read_registers_response(
                response, MODBUS_FC_READ_INPUT_REGISTERS, ctx->register_buffer,
                quantity);
        }
    }

    return result;
}

/**
 * @brief Process Write Single Coil (FC05) request
 */
static modbus_error_t process_write_single_coil(modbus_context_t   *ctx,
                                                const modbus_pdu_t *request,
                                                modbus_pdu_t       *response)
{
    uint16_t           address;
    bool               value;
    modbus_exception_t exception;
    modbus_error_t     err;
    modbus_error_t     result;

    (void)ctx; /* Unused in this function */

    err =
        modbus_pdu_decode_write_single_coil_request(request, &address, &value);
    if (err != MODBUS_OK)
    {
        result =
            modbus_pdu_encode_exception(response, MODBUS_FC_WRITE_SINGLE_COIL,
                                        MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else
    {
        /* Call user callback */
        exception = modbus_cb_write_single_coil(address, value);
        if (exception != MODBUS_EXCEPTION_NONE)
        {
            result = modbus_pdu_encode_exception(
                response, MODBUS_FC_WRITE_SINGLE_COIL, exception);
        }
        else
        {
            /* Echo request as response */
            result = modbus_pdu_encode_write_single_response(
                response, MODBUS_FC_WRITE_SINGLE_COIL, address,
                value ? 0xFF00U : 0x0000U);
        }
    }

    return result;
}

/**
 * @brief Process Write Single Register (FC06) request
 */
static modbus_error_t process_write_single_register(modbus_context_t   *ctx,
                                                    const modbus_pdu_t *request,
                                                    modbus_pdu_t *response)
{
    uint16_t           address;
    uint16_t           value;
    modbus_exception_t exception;
    modbus_error_t     err;
    modbus_error_t     result;

    (void)ctx; /* Unused in this function */

    err = modbus_pdu_decode_write_single_register_request(request, &address,
                                                          &value);
    if (err != MODBUS_OK)
    {
        result = modbus_pdu_encode_exception(
            response, MODBUS_FC_WRITE_SINGLE_REGISTER,
            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else
    {
        /* Call user callback */
        exception = modbus_cb_write_single_register(address, value);
        if (exception != MODBUS_EXCEPTION_NONE)
        {
            result = modbus_pdu_encode_exception(
                response, MODBUS_FC_WRITE_SINGLE_REGISTER, exception);
        }
        else
        {
            /* Echo request as response */
            result = modbus_pdu_encode_write_single_response(
                response, MODBUS_FC_WRITE_SINGLE_REGISTER, address, value);
        }
    }

    return result;
}

/**
 * @brief Process Write Multiple Coils (FC15) request
 */
static modbus_error_t process_write_multiple_coils(modbus_context_t   *ctx,
                                                   const modbus_pdu_t *request,
                                                   modbus_pdu_t       *response)
{
    uint16_t           start_address;
    uint16_t           quantity;
    const uint8_t     *values;
    modbus_exception_t exception;
    modbus_error_t     err;
    modbus_error_t     result;

    (void)ctx; /* Unused in this function */

    err = modbus_pdu_decode_write_multiple_coils_request(
        request, &start_address, &quantity, &values);
    if (err != MODBUS_OK)
    {
        result = modbus_pdu_encode_exception(
            response, MODBUS_FC_WRITE_MULTIPLE_COILS,
            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else if ((quantity == 0U) || (quantity > MAX_WRITE_COILS))
    {
        /* Validate quantity */
        result = modbus_pdu_encode_exception(
            response, MODBUS_FC_WRITE_MULTIPLE_COILS,
            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else
    {
        /* Call user callback */
        exception =
            modbus_cb_write_multiple_coils(start_address, quantity, values);
        if (exception != MODBUS_EXCEPTION_NONE)
        {
            result = modbus_pdu_encode_exception(
                response, MODBUS_FC_WRITE_MULTIPLE_COILS, exception);
        }
        else
        {
            result = modbus_pdu_encode_write_multiple_response(
                response, MODBUS_FC_WRITE_MULTIPLE_COILS, start_address,
                quantity);
        }
    }

    return result;
}

/**
 * @brief Process Write Multiple Registers (FC16) request
 */
static modbus_error_t process_write_multiple_registers(
    modbus_context_t *ctx, const modbus_pdu_t *request, modbus_pdu_t *response)
{
    uint16_t           start_address;
    uint16_t           quantity;
    modbus_exception_t exception;
    modbus_error_t     err;
    modbus_error_t     result;

    err = modbus_pdu_decode_write_multiple_registers_request(
        request, &start_address, &quantity, ctx->register_buffer,
        MAX_WRITE_REGISTERS);
    if (err != MODBUS_OK)
    {
        result = modbus_pdu_encode_exception(
            response, MODBUS_FC_WRITE_MULTIPLE_REGISTERS,
            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else if ((quantity == 0U) || (quantity > MAX_WRITE_REGISTERS))
    {
        /* Validate quantity */
        result = modbus_pdu_encode_exception(
            response, MODBUS_FC_WRITE_MULTIPLE_REGISTERS,
            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    }
    else
    {
        /* Call user callback */
        exception = modbus_cb_write_multiple_registers(start_address, quantity,
                                                       ctx->register_buffer);
        if (exception != MODBUS_EXCEPTION_NONE)
        {
            result = modbus_pdu_encode_exception(
                response, MODBUS_FC_WRITE_MULTIPLE_REGISTERS, exception);
        }
        else
        {
            result = modbus_pdu_encode_write_multiple_response(
                response, MODBUS_FC_WRITE_MULTIPLE_REGISTERS, start_address,
                quantity);
        }
    }

    return result;
}

/* ==========================================================================
 * Main Request Processing
 * ========================================================================== */

/**
 * @brief Process a Modbus request PDU and generate response PDU
 *
 * @param[in] ctx Pointer to context
 * @param[in] request Pointer to request PDU
 * @param[out] response Pointer to response PDU to fill
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_slave_process_pdu(modbus_context_t   *ctx,
                                        const modbus_pdu_t *request,
                                        modbus_pdu_t       *response)
{
    modbus_error_t err;
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((ctx != NULL) && (request != NULL) && (response != NULL))
    {
        if (!ctx->initialized)
        {
            result = MODBUS_ERROR_NOT_INITIALIZED;
        }
        else
        {
            ctx->requests_processed++;

            /* Process based on function code */
            switch (request->function_code)
            {
                case MODBUS_FC_READ_COILS:
                    err = process_read_coils(ctx, request, response);
                    break;

                case MODBUS_FC_READ_DISCRETE_INPUTS:
                    err = process_read_discrete_inputs(ctx, request, response);
                    break;

                case MODBUS_FC_READ_HOLDING_REGISTERS:
                    err =
                        process_read_holding_registers(ctx, request, response);
                    break;

                case MODBUS_FC_READ_INPUT_REGISTERS:
                    err = process_read_input_registers(ctx, request, response);
                    break;

                case MODBUS_FC_WRITE_SINGLE_COIL:
                    err = process_write_single_coil(ctx, request, response);
                    break;

                case MODBUS_FC_WRITE_SINGLE_REGISTER:
                    err = process_write_single_register(ctx, request, response);
                    break;

                case MODBUS_FC_WRITE_MULTIPLE_COILS:
                    err = process_write_multiple_coils(ctx, request, response);
                    break;

                case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
                    err = process_write_multiple_registers(ctx, request,
                                                           response);
                    break;

                default:
                    /* Unsupported function code */
                    ctx->exceptions_sent++;
                    err = modbus_pdu_encode_exception(
                        response, request->function_code,
                        MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
                    break;
            }

            if (err != MODBUS_OK)
            {
                ctx->errors_count++;
            }

            result = err;
        }
    }

    return result;
}

/**
 * @brief Process a complete Modbus ADU and generate response ADU
 *
 * @param[in] ctx Pointer to context
 * @param[in] request Pointer to request ADU
 * @param[out] response Pointer to response ADU to fill
 * @param[out] send_response Pointer to flag indicating if response should be
 * sent
 * @return modbus_error_t MODBUS_OK on success
 */
modbus_error_t modbus_slave_process_adu(modbus_context_t   *ctx,
                                        const modbus_adu_t *request,
                                        modbus_adu_t       *response,
                                        bool               *send_response)
{
    modbus_error_t err;
    modbus_error_t result = MODBUS_ERROR_INVALID_PARAM;

    if ((ctx != NULL) && (request != NULL) && (response != NULL) &&
        (send_response != NULL))
    {
        /* Check if request is for this slave */
        if ((request->unit_id != ctx->config.unit_id) &&
            (request->unit_id != 0U))
        {
            /* Not for us - don't respond */
            *send_response = false;
            result         = MODBUS_OK;
        }
        else
        {
            /* Process PDU */
            err = modbus_slave_process_pdu(ctx, &request->pdu, &response->pdu);
            if (err != MODBUS_OK)
            {
                *send_response = false;
                result         = err;
            }
            else
            {
                /* Copy addressing info to response */
                response->unit_id        = ctx->config.unit_id;
                response->transaction_id = request->transaction_id;
                response->protocol_id    = request->protocol_id;

                /* Don't respond to broadcast requests */
                *send_response = (request->unit_id != 0U);
                result         = MODBUS_OK;
            }
        }
    }

    return result;
}

/* ==========================================================================
 * Statistics Functions
 * ========================================================================== */

/**
 * @brief Get number of requests processed
 *
 * @param[in] ctx Pointer to context
 * @return uint32_t Number of requests processed
 */
uint32_t modbus_get_requests_processed(const modbus_context_t *ctx)
{
    uint32_t result = 0U;

    if ((ctx != NULL) && (ctx->initialized))
    {
        result = ctx->requests_processed;
    }

    return result;
}

/**
 * @brief Get number of errors
 *
 * @param[in] ctx Pointer to context
 * @return uint32_t Number of errors
 */
uint32_t modbus_get_errors_count(const modbus_context_t *ctx)
{
    uint32_t result = 0U;

    if ((ctx != NULL) && (ctx->initialized))
    {
        result = ctx->errors_count;
    }

    return result;
}

/**
 * @brief Get number of exceptions sent
 *
 * @param[in] ctx Pointer to context
 * @return uint32_t Number of exceptions sent
 */
uint32_t modbus_get_exceptions_sent(const modbus_context_t *ctx)
{
    uint32_t result = 0U;

    if ((ctx != NULL) && (ctx->initialized))
    {
        result = ctx->exceptions_sent;
    }

    return result;
}

/**
 * @brief Reset statistics counters
 *
 * @param[in] ctx Pointer to context
 */
void modbus_reset_statistics(modbus_context_t *ctx)
{
    if ((ctx != NULL) && (ctx->initialized))
    {
        ctx->requests_processed = 0U;
        ctx->errors_count       = 0U;
        ctx->exceptions_sent    = 0U;
    }
}

/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * Modbus TCP Server Task
 *
 * This task implements a Modbus TCP server that listens on port 502
 * and handles Modbus requests using the generated register callbacks.
 */

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "jerry_device_registers.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netbuf.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "modbus_callbacks.h"
#include "modbus_internal.h"
#include "peripheral_adapters.h"
#include "task.h"

/* ==========================================================================
 * Configuration
 * ========================================================================== */

/** Modbus TCP port number */
#define MODBUS_TCP_PORT 502U

/** Modbus slave/unit ID */
#define MODBUS_UNIT_ID 1U

/** Maximum number of simultaneous connections */
#define MODBUS_MAX_CONNECTIONS 4U

/** Receive timeout in milliseconds */
#define MODBUS_RECV_TIMEOUT_MS 5000U

/** Register update interval in milliseconds */
#define MODBUS_UPDATE_INTERVAL_MS 50U

/* ==========================================================================
 * Private Types
 * ========================================================================== */

/**
 * @brief Connection handler context
 */
typedef struct
{
    struct netconn *conn;
    bool            active;
} modbus_connection_t;

/* ==========================================================================
 * Private Data
 * ========================================================================== */

/** Active connections */
static modbus_connection_t s_connections[MODBUS_MAX_CONNECTIONS];

/** Receive buffer */
static uint8_t s_rx_buffer[MODBUS_TCP_MAX_ADU_SIZE];

/** Transmit buffer */
static uint8_t s_tx_buffer[MODBUS_TCP_MAX_ADU_SIZE];

/* ==========================================================================
 * Private Function Prototypes
 * ========================================================================== */

static void           modbus_tcp_server_thread(void *arg);
static void           modbus_handle_connection(struct netconn *conn);
static modbus_error_t modbus_process_request(const uint8_t *request,
                                             uint16_t       request_len,
                                             uint8_t       *response,
                                             uint16_t      *response_len);

/* ==========================================================================
 * Public Functions
 * ========================================================================== */

void vModbusTask(void *pvParameters)
{
    (void)pvParameters;

    printf("Modbus Task Started\n");

    /* Wait for LwIP to be fully initialized
     * The tcpip_init() is called from main and takes some time to complete.
     * We need to wait until the network stack is ready before creating
     * netconn structures.
     */
    printf("Modbus: Waiting for network stack initialization...\n");
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Initialize register data structures */
    jerry_device_registers_init();
    printf("Modbus registers initialized\n");

    /* Initialize peripheral adapters */
    peripheral_adapters_init();
    printf("Peripheral adapters initialized\n");

    /* Initialize connection tracking */
    for (uint8_t i = 0U; i < MODBUS_MAX_CONNECTIONS; i++)
    {
        s_connections[i].conn   = NULL;
        s_connections[i].active = false;
    }

    /* Start the Modbus TCP server */
    modbus_tcp_server_thread(NULL);

    /* Should not reach here */
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ==========================================================================
 * Private Functions
 * ========================================================================== */

/**
 * @brief Modbus TCP server main thread
 */
static void modbus_tcp_server_thread(void *arg)
{
    struct netconn *listen_conn;
    struct netconn *new_conn;
    err_t           err;

    (void)arg;

    /* Create a new TCP connection handle */
    listen_conn = netconn_new(NETCONN_TCP);
    if (listen_conn == NULL)
    {
        printf("Modbus: Failed to create connection\n");
        return;
    }

    /* Bind to Modbus TCP port */
    err = netconn_bind(listen_conn, IP_ADDR_ANY, MODBUS_TCP_PORT);
    if (err != ERR_OK)
    {
        printf("Modbus: Failed to bind to port %u: %d\n", MODBUS_TCP_PORT, err);
        netconn_delete(listen_conn);
        return;
    }

    /* Start listening */
    err = netconn_listen(listen_conn);
    if (err != ERR_OK)
    {
        printf("Modbus: Failed to listen: %d\n", err);
        netconn_delete(listen_conn);
        return;
    }

    printf("Modbus TCP Server listening on port %u\n", MODBUS_TCP_PORT);

    /* Main server loop */
    while (1)
    {
        /* Accept new connections */
        err = netconn_accept(listen_conn, &new_conn);
        if (err == ERR_OK)
        {
            printf("Modbus: New connection accepted\n");

            /* Set receive timeout */
            netconn_set_recvtimeout(new_conn, MODBUS_RECV_TIMEOUT_MS);

            /* Handle the connection (blocking) */
            modbus_handle_connection(new_conn);

            /* Clean up */
            netconn_close(new_conn);
            netconn_delete(new_conn);
            printf("Modbus: Connection closed\n");
        }
        else
        {
            printf("Modbus: Accept error: %d\n", err);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

/**
 * @brief Handle a single Modbus TCP connection
 */
static void modbus_handle_connection(struct netconn *conn)
{
    struct netbuf *buf;
    err_t          err;
    void          *data;
    u16_t          len;
    uint16_t       response_len;
    modbus_error_t modbus_err;
    TickType_t     last_update = xTaskGetTickCount();

    while (1)
    {
        /* Update registers periodically */
        TickType_t now = xTaskGetTickCount();
        if ((now - last_update) >= pdMS_TO_TICKS(MODBUS_UPDATE_INTERVAL_MS))
        {
            peripheral_adapters_update_registers();
            last_update = now;
        }

        /* Receive data */
        err = netconn_recv(conn, &buf);
        if (err == ERR_OK)
        {
            /* Get data from netbuf */
            netbuf_data(buf, &data, &len);

            if (len > 0U)
            {
                /* Copy to receive buffer */
                uint16_t copy_len = (len > sizeof(s_rx_buffer))
                                        ? (uint16_t)sizeof(s_rx_buffer)
                                        : len;
                (void)memcpy(s_rx_buffer, data, copy_len);

                /* Process the Modbus request */
                modbus_err = modbus_process_request(s_rx_buffer, copy_len,
                                                    s_tx_buffer, &response_len);

                if (modbus_err == MODBUS_OK)
                {
                    /* Send response */
                    err = netconn_write(conn, s_tx_buffer, response_len,
                                        NETCONN_COPY);
                    if (err != ERR_OK)
                    {
                        printf("Modbus: Write error: %d\n", err);
                    }
                }
                else
                {
                    printf("Modbus: Process error: %d\n", (int)modbus_err);
                }

                /* Apply any output changes */
                peripheral_adapters_apply_outputs();
            }

            netbuf_delete(buf);
        }
        else if (err == ERR_TIMEOUT)
        {
            /* Timeout - update registers and continue */
            peripheral_adapters_update_registers();
            last_update = xTaskGetTickCount();
        }
        else
        {
            /* Connection error - exit */
            printf("Modbus: Receive error: %d\n", err);
            break;
        }
    }
}

/**
 * @brief Process a Modbus TCP request and generate response
 */
static modbus_error_t modbus_process_request(const uint8_t *request,
                                             uint16_t       request_len,
                                             uint8_t       *response,
                                             uint16_t      *response_len)
{
    modbus_adu_t       request_adu;
    modbus_adu_t       response_adu;
    modbus_pdu_t       response_pdu;
    modbus_error_t     err;
    modbus_exception_t exception = MODBUS_EXCEPTION_NONE;

    /* Parse the TCP frame */
    err = modbus_tcp_parse_frame(request, request_len, &request_adu);
    if (err != MODBUS_OK)
    {
        printf("Modbus: Frame parse error: %d\n", (int)err);
        return err;
    }

    /* Check unit ID (0 = broadcast, or match our ID) */
    if ((request_adu.unit_id != 0U) && (request_adu.unit_id != MODBUS_UNIT_ID))
    {
        /* Not for us - ignore */
        return MODBUS_ERROR_INVALID_PARAM;
    }

    /* Initialize response PDU */
    (void)memset(&response_pdu, 0, sizeof(response_pdu));

    /* Process based on function code */
    switch (request_adu.pdu.function_code)
    {
        case MODBUS_FC_READ_COILS:
        {
            uint16_t start_address;
            uint16_t quantity;
            uint8_t  coil_values[256];

            err = modbus_pdu_decode_read_bits_request(
                &request_adu.pdu, &start_address, &quantity);
            if (err == MODBUS_OK)
            {
                exception =
                    modbus_cb_read_coils(start_address, quantity, coil_values);
                if (exception == MODBUS_EXCEPTION_NONE)
                {
                    err = modbus_pdu_encode_read_bits_response(
                        &response_pdu, MODBUS_FC_READ_COILS, coil_values,
                        quantity);
                }
            }
            break;
        }

        case MODBUS_FC_READ_DISCRETE_INPUTS:
        {
            uint16_t start_address;
            uint16_t quantity;
            uint8_t  input_values[256];

            err = modbus_pdu_decode_read_bits_request(
                &request_adu.pdu, &start_address, &quantity);
            if (err == MODBUS_OK)
            {
                exception = modbus_cb_read_discrete_inputs(
                    start_address, quantity, input_values);
                if (exception == MODBUS_EXCEPTION_NONE)
                {
                    err = modbus_pdu_encode_read_bits_response(
                        &response_pdu, MODBUS_FC_READ_DISCRETE_INPUTS,
                        input_values, quantity);
                }
            }
            break;
        }

        case MODBUS_FC_READ_HOLDING_REGISTERS:
        {
            uint16_t start_address;
            uint16_t quantity;
            uint16_t register_values[125];

            err = modbus_pdu_decode_read_registers_request(
                &request_adu.pdu, &start_address, &quantity);
            if (err == MODBUS_OK)
            {
                exception = modbus_cb_read_holding_registers(
                    start_address, quantity, register_values);
                if (exception == MODBUS_EXCEPTION_NONE)
                {
                    err = modbus_pdu_encode_read_registers_response(
                        &response_pdu, MODBUS_FC_READ_HOLDING_REGISTERS,
                        register_values, quantity);
                }
            }
            break;
        }

        case MODBUS_FC_READ_INPUT_REGISTERS:
        {
            uint16_t start_address;
            uint16_t quantity;
            uint16_t register_values[125];

            err = modbus_pdu_decode_read_registers_request(
                &request_adu.pdu, &start_address, &quantity);
            if (err == MODBUS_OK)
            {
                exception = modbus_cb_read_input_registers(
                    start_address, quantity, register_values);
                if (exception == MODBUS_EXCEPTION_NONE)
                {
                    err = modbus_pdu_encode_read_registers_response(
                        &response_pdu, MODBUS_FC_READ_INPUT_REGISTERS,
                        register_values, quantity);
                }
            }
            break;
        }

        case MODBUS_FC_WRITE_SINGLE_COIL:
        {
            uint16_t address;
            bool     value;

            err = modbus_pdu_decode_write_single_coil_request(&request_adu.pdu,
                                                              &address, &value);
            if (err == MODBUS_OK)
            {
                exception = modbus_cb_write_single_coil(address, value);
                if (exception == MODBUS_EXCEPTION_NONE)
                {
                    uint16_t coil_value = value ? 0xFF00U : 0x0000U;
                    err = modbus_pdu_encode_write_single_response(
                        &response_pdu, MODBUS_FC_WRITE_SINGLE_COIL, address,
                        coil_value);
                }
            }
            break;
        }

        case MODBUS_FC_WRITE_SINGLE_REGISTER:
        {
            uint16_t address;
            uint16_t value;

            err = modbus_pdu_decode_write_single_register_request(
                &request_adu.pdu, &address, &value);
            if (err == MODBUS_OK)
            {
                exception = modbus_cb_write_single_register(address, value);
                if (exception == MODBUS_EXCEPTION_NONE)
                {
                    err = modbus_pdu_encode_write_single_response(
                        &response_pdu, MODBUS_FC_WRITE_SINGLE_REGISTER, address,
                        value);
                }
            }
            break;
        }

        case MODBUS_FC_WRITE_MULTIPLE_COILS:
        {
            uint16_t       start_address;
            uint16_t       quantity;
            const uint8_t *values;

            err = modbus_pdu_decode_write_multiple_coils_request(
                &request_adu.pdu, &start_address, &quantity, &values);
            if (err == MODBUS_OK)
            {
                exception = modbus_cb_write_multiple_coils(start_address,
                                                           quantity, values);
                if (exception == MODBUS_EXCEPTION_NONE)
                {
                    err = modbus_pdu_encode_write_multiple_response(
                        &response_pdu, MODBUS_FC_WRITE_MULTIPLE_COILS,
                        start_address, quantity);
                }
            }
            break;
        }

        case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
        {
            uint16_t start_address;
            uint16_t quantity;
            uint16_t values[123];

            err = modbus_pdu_decode_write_multiple_registers_request(
                &request_adu.pdu, &start_address, &quantity, values, 123U);
            if (err == MODBUS_OK)
            {
                exception = modbus_cb_write_multiple_registers(
                    start_address, quantity, values);
                if (exception == MODBUS_EXCEPTION_NONE)
                {
                    err = modbus_pdu_encode_write_multiple_response(
                        &response_pdu, MODBUS_FC_WRITE_MULTIPLE_REGISTERS,
                        start_address, quantity);
                }
            }
            break;
        }

        default:
            exception = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
            break;
    }

    /* Build exception response if needed */
    if (exception != MODBUS_EXCEPTION_NONE)
    {
        err = modbus_pdu_encode_exception(
            &response_pdu, request_adu.pdu.function_code, exception);
    }

    if (err != MODBUS_OK)
    {
        return err;
    }

    /* Build response ADU */
    response_adu.transaction_id = request_adu.transaction_id;
    response_adu.protocol_id    = 0U;
    response_adu.unit_id        = MODBUS_UNIT_ID;
    response_adu.pdu            = response_pdu;

    /* Build TCP frame */
    err = modbus_tcp_build_frame(&response_adu, response,
                                 MODBUS_TCP_MAX_ADU_SIZE, response_len);

    return err;
}

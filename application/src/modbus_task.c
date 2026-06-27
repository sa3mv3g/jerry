/*
 * Copyright (c) 2026
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
#include "bsp.h"
#include "jerry_device_registers.h"
#include "lcd_manager.h"
#include "log.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netbuf.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "modbus_callbacks.h"
#include "modbus_internal.h"
#include "register_lock.h"
#include "task.h"

/* ==========================================================================
 * Configuration
 * ========================================================================== */

/** Modbus TCP port number */
#define MODBUS_TCP_PORT 502U

/** Base Modbus slave/unit ID (added to DEVADDR value) */
#define MODBUS_UNIT_ID_BASE 1U

/** Maximum number of simultaneous connections */
#define MODBUS_MAX_CONNECTIONS 4U

/** Receive timeout in milliseconds */
#define MODBUS_RECV_TIMEOUT_MS 5000U

#define BSP_EEPROM_READ(A, B, C) \
    if (BSP_OK != BSP_EEPROM_Read(A, B, C)) break;

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

/** Modbus unit ID (initialized from DEVADDR pins) */
static uint8_t s_modbus_unit_id = MODBUS_UNIT_ID_BASE;

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

    uint8_t                           dev_addr;
    uint16_t                          initDigitalOutputCoilsValues;
    jerry_device_holding_registers_t *hrRegs;
    bsp_error_t                       err;

    LOG_INF("[Modbus] Task Started");

    /* Read device address from DEVADDR pins and set Modbus unit ID */
    err              = BSP_ERROR;
    dev_addr         = BSP_GetDeviceAddress();
    s_modbus_unit_id = MODBUS_UNIT_ID_BASE + dev_addr;

    LOG_INF("[Modbus] Device address from DEVADDR pins: %u, Unit ID: %u",
            dev_addr, s_modbus_unit_id);

    /* Initialize register data structures and the mutex guarding them */
    jerry_device_registers_init();
    RegisterLock_Init();
    /* Read parameters from EEPROM */
    hrRegs = jerry_device_get_holding_registers();
    do
    {
        err = BSP_EEPROM_Read(MODBUS_NVM_ADC_0_SCALE_FACTOR,
                              (uint8_t *)&hrRegs->adc_0_scale_factor,
                              sizeof(float));
        if (err != BSP_OK)
        {
            LOG_INF(
                "[Modbus] Virgin MCU detected (EEPROM uninitialized). Using "
                "default calibration.");
            /* Reset err to OK so we don't print the generic failure message
             * below */
            err = BSP_OK;
            break;
        }

        BSP_EEPROM_READ(MODBUS_NVM_ADC_0_OFFSET_TERM,
                        (uint8_t *)&hrRegs->adc_0_offset_term, sizeof(float));
        BSP_EEPROM_READ(MODBUS_NVM_ADC_0_DEAD_ZONE,
                        (uint8_t *)&hrRegs->adc_0_dead_zone, sizeof(float));

        BSP_EEPROM_READ(MODBUS_NVM_ADC_1_SCALE_FACTOR,
                        (uint8_t *)&hrRegs->adc_1_scale_factor, sizeof(float));
        BSP_EEPROM_READ(MODBUS_NVM_ADC_1_OFFSET_TERM,
                        (uint8_t *)&hrRegs->adc_1_offset_term, sizeof(float));
        BSP_EEPROM_READ(MODBUS_NVM_ADC_1_DEAD_ZONE,
                        (uint8_t *)&hrRegs->adc_1_dead_zone, sizeof(float));

        BSP_EEPROM_READ(MODBUS_NVM_ADC_2_SCALE_FACTOR,
                        (uint8_t *)&hrRegs->adc_2_scale_factor, sizeof(float));
        BSP_EEPROM_READ(MODBUS_NVM_ADC_2_OFFSET_TERM,
                        (uint8_t *)&hrRegs->adc_2_offset_term, sizeof(float));
        BSP_EEPROM_READ(MODBUS_NVM_ADC_2_DEAD_ZONE,
                        (uint8_t *)&hrRegs->adc_2_dead_zone, sizeof(float));

        BSP_EEPROM_READ(MODBUS_NVM_ADC_3_SCALE_FACTOR,
                        (uint8_t *)&hrRegs->adc_3_scale_factor, sizeof(float));
        BSP_EEPROM_READ(MODBUS_NVM_ADC_3_OFFSET_TERM,
                        (uint8_t *)&hrRegs->adc_3_offset_term, sizeof(float));
        BSP_EEPROM_READ(MODBUS_NVM_ADC_3_DEAD_ZONE,
                        (uint8_t *)&hrRegs->adc_3_dead_zone, sizeof(float));
        err = BSP_OK;

    } while (0);

    if (BSP_OK != err)
    {
        LOG_ERR("[Modbus] Failed to read calibration data from EEPROM!!");
    }

    LOG_INF("[Modbus] registers initialized");

    /* Print calibration values in use after EEPROM load (or defaults on virgin
     * flash) */
    LOG_INF("[Modbus] Calibration values:");
    LOG_INF("[Modbus]   ADC0: scale=%.6f  offset=%.6f  dead_zone=%.6f",
            (double)hrRegs->adc_0_scale_factor,
            (double)hrRegs->adc_0_offset_term, (double)hrRegs->adc_0_dead_zone);
    LOG_INF("[Modbus]   ADC1: scale=%.6f  offset=%.6f  dead_zone=%.6f",
            (double)hrRegs->adc_1_scale_factor,
            (double)hrRegs->adc_1_offset_term, (double)hrRegs->adc_1_dead_zone);
    LOG_INF("[Modbus]   ADC2: scale=%.6f  offset=%.6f  dead_zone=%.6f",
            (double)hrRegs->adc_2_scale_factor,
            (double)hrRegs->adc_2_offset_term, (double)hrRegs->adc_2_dead_zone);
    LOG_INF("[Modbus]   ADC3: scale=%.6f  offset=%.6f  dead_zone=%.6f",
            (double)hrRegs->adc_3_scale_factor,
            (double)hrRegs->adc_3_offset_term, (double)hrRegs->adc_3_dead_zone);

    /* Initialize connection tracking */
    for (uint8_t i = 0U; i < MODBUS_MAX_CONNECTIONS; i++)
    {
        s_connections[i].conn   = NULL;
        s_connections[i].active = false;
    }

    HAL_IWDG_Refresh(&hiwdg);

    xEventGroupSync(xSyncEventGroup, APPTASK_MODBUS_TASK_EVENT_MASK,
                    APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);

    LcdManager_UpdateModbusDeviceAddress(s_modbus_unit_id);
    initDigitalOutputCoilsValues = 0x0;
    if (BSP_OK == BSP_I2CDO_Read(&initDigitalOutputCoilsValues))
    {
        /*
         * Corrects endianness issue N-02.
         * The 16-bit value from the I2C I/O expander is explicitly
         * unpacked into a 2-byte array to match the Modbus coil packing
         * order (little-endian: byte 0 = coils 0-7, byte 1 = coils 8-15).
         * This avoids an undefined-behavior pointer cast and makes the
         * byte order explicit.
         */
        uint8_t coils[2] = {(uint8_t)(initDigitalOutputCoilsValues & 0xFFU),
                            (uint8_t)(initDigitalOutputCoilsValues >> 8)};
        modbus_cb_write_multiple_coils(0, 16, coils);
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

    LOG_INF("[Modbus]: Available netconns: %d",
            lwip_stats.memp[MEMP_NETCONN]->avail);
    /* Create a new TCP connection handle */
    listen_conn = netconn_new(NETCONN_TCP);
    if (listen_conn == NULL)
    {
        LOG_INF("[Modbus]: Failed to create connection");
        return;
    }

    /* Bind to Modbus TCP port */
    err = netconn_bind(listen_conn, IP_ADDR_ANY, MODBUS_TCP_PORT);
    if (err != ERR_OK)
    {
        LOG_INF("[Modbus]: Failed to bind to port %u: %d", MODBUS_TCP_PORT,
                err);
        netconn_delete(listen_conn);
        return;
    }

    /* Start listening */
    err = netconn_listen(listen_conn);
    if (err != ERR_OK)
    {
        LOG_INF("[Modbus]: Failed to listen: %d", err);
        netconn_delete(listen_conn);
        return;
    }

    LOG_INF("[Modbus] TCP Server listening on port %u", MODBUS_TCP_PORT);

    /* Main server loop */
    while (1)
    {
        /* Accept new connections */
        err = netconn_accept(listen_conn, &new_conn);
        if (err == ERR_OK)
        {
            LOG_INF("[Modbus]: New connection accepted");

            /* Set receive timeout */
            netconn_set_recvtimeout(new_conn, MODBUS_RECV_TIMEOUT_MS);

            /* Handle the connection (blocking) */
            modbus_handle_connection(new_conn);

            /* Clean up */
            netconn_close(new_conn);
            netconn_delete(new_conn);
            LOG_INF("[Modbus]: Connection closed");
        }
        else
        {
            LOG_INF("[Modbus]: Accept error: %d", err);
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

    while (1)
    {
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
                    /* Send response only if there is data to send */
                    /* (response_len=0 means request was for different unit ID)
                     */
                    if (response_len > 0U)
                    {
                        err = netconn_write(conn, s_tx_buffer, response_len,
                                            NETCONN_COPY);
                        if (err != ERR_OK)
                        {
                            LOG_ERR("[Modbus]: Write error: %d", err);
                        }
                    }
                }
                else
                {
                    LOG_ERR("[Modbus]: Process error: %d", (int)modbus_err);
                }
            }

            netbuf_delete(buf);
        }
        else if (err == ERR_TIMEOUT)
        {
            /* Timeout - continue waiting */
        }
        else
        {
            /* Connection error - exit */
            LOG_ERR("[Modbus]: Receive error: %d", err);
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
    static modbus_adu_t request_adu;
    static modbus_adu_t response_adu;
    static modbus_pdu_t response_pdu;
    modbus_error_t      err;
    modbus_exception_t  exception = MODBUS_EXCEPTION_NONE;

    /* Parse the TCP frame */
    err = modbus_tcp_parse_frame(request, request_len, &request_adu);
    if (err != MODBUS_OK)
    {
        LOG_ERR("[Modbus]: Frame parse error: %d", (int)err);
        return err;
    }

    /* Check unit ID (0 = broadcast, or match our ID) */
    if ((request_adu.unit_id != 0U) &&
        (request_adu.unit_id != s_modbus_unit_id))
    {
        /* Not for us - silently ignore (no response per Modbus spec) */
        *response_len = 0U;
        return MODBUS_OK;
    }

    /* Initialize response PDU */
    (void)memset(&response_pdu, 0, sizeof(response_pdu));

    /* Guard all shared register access performed by the callbacks below
     * (BUG-04). The lock is released on every exit path from here on. */
    RegisterLock_Acquire();

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

    /* Done touching shared register data — release the lock. */
    RegisterLock_Release();

    if (err != MODBUS_OK)
    {
        return err;
    }

    /* Build response ADU */
    response_adu.transaction_id = request_adu.transaction_id;
    response_adu.protocol_id    = 0U;
    response_adu.unit_id        = s_modbus_unit_id;
    response_adu.pdu            = response_pdu;

    /* Build TCP frame */
    err = modbus_tcp_build_frame(&response_adu, response,
                                 MODBUS_TCP_MAX_ADU_SIZE, response_len);

    return err;
}

/*
 * Copyright (c) 2026
 * All rights reserved.
 *
 * Modbus TCP Server Task
 *
 * This task implements a Modbus TCP server that listens on port 502
 * and handles Modbus requests using the generated register callbacks.
 */

#include <string.h>

#include "FreeRTOS.h"
#include "app_log.h"
#include "app_main.h"
#include "app_tasks.h"
#include "bsp.h"
#include "jerry_device_registers.h"
#include "lcd_manager.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netbuf.h"
#include "lwip/opt.h"
#include "lwip/stats.h"
#include "modbus_callbacks.h"
#include "modbus_internal.h"
#include "network_sync.h"
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

static char s_modbus_unit_id_string[APP_MODBUSID_STR_MAX_SZ_BYTES];

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
    BaseType_t                        isTcpReady;
    bsp_error_t                       err;

    LOG_INF("[Modbus] Task Started");

    /* Read device address from DEVADDR pins and set Modbus unit ID */
    err              = BSP_ERROR;
    dev_addr         = BSP_GetDeviceAddress();
    s_modbus_unit_id = MODBUS_UNIT_ID_BASE + dev_addr;

    memset(s_modbus_unit_id_string, 0, sizeof(s_modbus_unit_id_string));
    snprintf(s_modbus_unit_id_string, sizeof(s_modbus_unit_id_string), "%u",
             s_modbus_unit_id);

    LOG_INF("[Modbus] Device address from DEVADDR pins: %u, Unit ID: %u",
            dev_addr, s_modbus_unit_id);

    /* Initialize register data structures and the mutex guarding them */
    jerry_device_registers_init();
    RegisterLock_Init();
    /* Read parameters from EEPROM */
    hrRegs = jerry_device_get_holding_registers();
    do
    {
#define BSP_EEPROM_READ(A, B, C) \
    if (BSP_OK != BSP_EEPROM_Read(A, B, C)) break;
#define BSP_EEPROM_WRITE(A, B, C) \
    if (BSP_OK != BSP_EEPROM_Write(A, B, C)) break;

        err = BSP_EEPROM_Read(MODBUS_NVM_ADC_0_SCALE_FACTOR,
                              (uint8_t *)&hrRegs->adc_0_scale_factor,
                              sizeof(float));
        if (err != BSP_OK)
        {
            LOG_INF(
                "[Modbus] EEPROM uninitialized. Using "
                "default calibration.");
            BSP_EEPROM_WRITE(MODBUS_NVM_ADC_0_SCALE_FACTOR,
                             (uint8_t *)&hrRegs->adc_0_scale_factor,
                             sizeof(float));
            BSP_EEPROM_WRITE(MODBUS_NVM_ADC_0_OFFSET_TERM,
                             (uint8_t *)&hrRegs->adc_0_offset_term,
                             sizeof(float));
            BSP_EEPROM_WRITE(MODBUS_NVM_ADC_0_DEAD_ZONE,
                             (uint8_t *)&hrRegs->adc_0_dead_zone,
                             sizeof(float));

            BSP_EEPROM_WRITE(MODBUS_NVM_ADC_1_SCALE_FACTOR,
                             (uint8_t *)&hrRegs->adc_1_scale_factor,
                             sizeof(float));
            BSP_EEPROM_WRITE(MODBUS_NVM_ADC_1_OFFSET_TERM,
                             (uint8_t *)&hrRegs->adc_1_offset_term,
                             sizeof(float));
            BSP_EEPROM_WRITE(MODBUS_NVM_ADC_1_DEAD_ZONE,
                             (uint8_t *)&hrRegs->adc_1_dead_zone,
                             sizeof(float));

            BSP_EEPROM_WRITE(MODBUS_NVM_ADC_2_SCALE_FACTOR,
                             (uint8_t *)&hrRegs->adc_2_scale_factor,
                             sizeof(float));
            BSP_EEPROM_WRITE(MODBUS_NVM_ADC_2_OFFSET_TERM,
                             (uint8_t *)&hrRegs->adc_2_offset_term,
                             sizeof(float));
            BSP_EEPROM_WRITE(MODBUS_NVM_ADC_2_DEAD_ZONE,
                             (uint8_t *)&hrRegs->adc_2_dead_zone,
                             sizeof(float));

            BSP_EEPROM_WRITE(MODBUS_NVM_ADC_3_SCALE_FACTOR,
                             (uint8_t *)&hrRegs->adc_3_scale_factor,
                             sizeof(float));
            BSP_EEPROM_WRITE(MODBUS_NVM_ADC_3_OFFSET_TERM,
                             (uint8_t *)&hrRegs->adc_3_offset_term,
                             sizeof(float));
            BSP_EEPROM_WRITE(MODBUS_NVM_ADC_3_DEAD_ZONE,
                             (uint8_t *)&hrRegs->adc_3_dead_zone,
                             sizeof(float));

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

#undef BSP_EEPROM_READ
#undef BSP_EEPROM_WRITE
    } while (0);

    if (BSP_OK != err)
    {
        LOG_ERR("[Modbus] Failed to read/write calibration data from EEPROM!!");
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

    /* Wait for network to be enabled */
    do
    {
        isTcpReady = NetworkSync_WaitForTcpReady();
        HAL_IWDG_Refresh(&hiwdg);
    } while (isTcpReady == pdFALSE);

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

uint8_t App_GetModbusId() { return s_modbus_unit_id; }
char   *App_GetModbusIdString() { return s_modbus_unit_id_string; }

/* ==========================================================================
 * Private Functions
 * ========================================================================== */

/**
 * @brief Modbus TCP server main thread
 */
static void modbus_tcp_server_thread(void *arg)
{
    (void)arg;

    while (1)
    {
#if LWIP_STATS && MEMP_STATS
        if (lwip_stats.memp[MEMP_NETCONN] != NULL)
        {
            LOG_INF("[Modbus]: Available netconns: %u",
                    (unsigned int)lwip_stats.memp[MEMP_NETCONN]->avail);
        }
#endif
        struct netconn *listen_conn = netconn_new(NETCONN_TCP);
        if (listen_conn == NULL)
        {
            LOG_ERR(
                "[Modbus] Failed to create listen connection. Retrying in 5s.");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        err_t err = netconn_bind(listen_conn, IP_ADDR_ANY, MODBUS_TCP_PORT);
        if (err != ERR_OK)
        {
            LOG_ERR("[Modbus] Failed to bind to port %u: %d. Retrying in 5s.",
                    MODBUS_TCP_PORT, err);
            netconn_delete(listen_conn);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        err = netconn_listen(listen_conn);
        if (err != ERR_OK)
        {
            LOG_ERR("[Modbus] Failed to listen: %d. Retrying in 5s.", err);
            netconn_delete(listen_conn);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        LOG_INF("[Modbus] TCP Server listening on port %u", MODBUS_TCP_PORT);

        while (1)
        {
            struct netconn *new_conn;
            err = netconn_accept(listen_conn, &new_conn);

            if (err == ERR_OK)
            {
                LOG_INF("[Modbus] New connection accepted");
                netconn_set_recvtimeout(new_conn, MODBUS_RECV_TIMEOUT_MS);
                modbus_handle_connection(new_conn);
                netconn_close(new_conn);
                netconn_delete(new_conn);
                LOG_INF("[Modbus] Connection closed");
            }
            else
            {
                LOG_ERR("[Modbus] Accept error: %d. Resetting listener.", err);
                break; /* Break inner loop to re-create listen_conn */
            }
        }

        netconn_close(listen_conn);
        netconn_delete(listen_conn);
    }
}

/**
 * @brief Handle a single Modbus TCP connection
 */
static void modbus_handle_connection(struct netconn *conn)
{
    struct netbuf *buf;
    err_t          err;
    uint16_t       total_len = 0;

    while (1)
    {
        err = netconn_recv(conn, &buf);
        if (err == ERR_OK)
        {
            struct pbuf *p;
            uint16_t     len;
            bool         overflow = false;

            /* Walk the pbuf chain to copy all fragments into a single buffer */
            for (p = buf->p; p != NULL; p = p->next)
            {
                len = p->len;
                if (total_len + len > sizeof(s_rx_buffer))
                {
                    LOG_ERR(
                        "[Modbus] RX buffer overflow. Dropping oversized "
                        "frame.");
                    total_len = 0; /* Reset for next frame */
                    overflow  = true;
                }
                else
                {
                    memcpy(&s_rx_buffer[total_len], p->payload, len);
                    total_len += len;
                }
                if (overflow)
                {
                    break;
                }
            }

            netbuf_delete(buf);

            if (overflow)
            {
                continue;
            }

            if (total_len >= 6)
            {
                uint16_t expected_len =
                    (uint16_t)((s_rx_buffer[4] << 8) | s_rx_buffer[5]) + 6;

                if (total_len >= expected_len)
                {
                    modbus_error_t modbus_err;
                    uint16_t       response_len;

                    modbus_err = modbus_process_request(
                        s_rx_buffer, expected_len, s_tx_buffer, &response_len);

                    if (modbus_err == MODBUS_OK && response_len > 0)
                    {
                        err = netconn_write(conn, s_tx_buffer, response_len,
                                            NETCONN_COPY);
                        if (err != ERR_OK)
                        {
                            LOG_ERR("[Modbus]: Write error: %d", err);
                        }
                    }
                    else if (modbus_err != MODBUS_OK)
                    {
                        LOG_ERR("[Modbus]: Process error: %d", (int)modbus_err);
                    }

                    /* Reset for next frame */
                    total_len = 0;
                }
            }
        }
        else if (err == ERR_TIMEOUT)
        {
            if (total_len > 0)
            {
                LOG_WRN(
                    "[Modbus] Discarding partial frame on timeout (%u bytes)",
                    total_len);
                total_len = 0;
            }
        }
        else
        {
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
    /* These static variables are used to avoid large stack allocations. This is
     * only safe because the server is single-threaded and handles one
     * connection at a time. If multi-threaded connection handling is added,
     * these must be allocated per-connection. */
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
            uint16_t       start_address;
            uint16_t       quantity;
            static uint8_t coil_values[256];

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
            uint16_t       start_address;
            uint16_t       quantity;
            static uint8_t input_values[256];

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
            uint16_t        start_address;
            uint16_t        quantity;
            static uint16_t register_values[125];

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
            uint16_t        start_address;
            uint16_t        quantity;
            static uint16_t register_values[125];

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
            uint16_t        start_address;
            uint16_t        quantity;
            static uint16_t values[123];

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

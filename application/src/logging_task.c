/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_log.h"
#include "app_tasks.h"
#include "bsp.h"
#include "jerry_device_registers.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "message_buffer.h"
#include "network_sync.h"

/* ========================================================================== */
/*                 Private Definitions and Macros                             */
/* ========================================================================== */
#define SYSLOG_MSG_BUFFER_SIZE (1024U)  // Total bytes for message buffer

/* ========================================================================== */
/*                 Private Function Prototype                                 */
/* ========================================================================== */
static int Logging_UdpSendInit(void);
static int Logging_UdpSendString(const char *message);

/* ========================================================================== */
/*                 Private Variable Declaration                               */
/* ========================================================================== */
#if CMAKE_ENABLE_SYSLOG
static StaticMessageBuffer_t xSyslogMessageBufferStruct;
static MessageBufferHandle_t xSyslogMessageBuffer = NULL;
static uint8_t            xSyslogMessageBufferStorage[SYSLOG_MSG_BUFFER_SIZE];
static struct netconn    *g_syslog_conn = NULL;
static ip_addr_t          g_dest_ip;
extern IWDG_HandleTypeDef hiwdg;
#endif

/* ========================================================================== */
/*                 Public Functions                                           */
/* ========================================================================== */
void vLoggingTask(void *pvParameters)
{
    (void)pvParameters;

#if CMAKE_ENABLE_SYSLOG
    xSyslogMessageBuffer = xMessageBufferCreateStatic(
        SYSLOG_MSG_BUFFER_SIZE, xSyslogMessageBufferStorage,
        &xSyslogMessageBufferStruct);
#endif

    /* let wait for all other task configuration to finish */
    xEventGroupSync(xSyncEventGroup, APPTASK_LOGGING_TASK_EVENT_MASK,
                    APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);

#if CMAKE_ENABLE_SYSLOG
    BaseType_t isTcpReady;
    /* Wait for network to be enabled */
    do
    {
        isTcpReady = NetworkSync_WaitForTcpReady();
        HAL_IWDG_Refresh(&hiwdg);
    } while (isTcpReady == pdFALSE);

    Logging_UdpSendInit();

    LOG_INF("Syslog Logging started");

    for (;;)
    {
        static char buf[LOG_MAX_MSG_LEN];  // Buffer to receive formatted string
        size_t      received_bytes = xMessageBufferReceive(
            xSyslogMessageBuffer, buf, sizeof(buf) - 1, portMAX_DELAY);

        if (received_bytes > 0)
        {
            buf[received_bytes] = '\0';
            Logging_UdpSendString(buf);
        }
    }
#else
    for (;;)
    {
        vTaskDelay(portMAX_DELAY);
    }
#endif
}

void Applog_Syslog(const char *msg)
{
#if CMAKE_ENABLE_SYSLOG
    if (NULL != xSyslogMessageBuffer && msg != NULL)
    {
        /* Cannot use MessageBufferSend safely before scheduler is running */
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        {
            size_t len = strlen(msg);
            /* Check if we're in an ISR context */
            if (xPortIsInsideInterrupt())
            {
                BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                xMessageBufferSendFromISR(xSyslogMessageBuffer, msg, len,
                                          &xHigherPriorityTaskWoken);
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
            else
            {
                /* Non-blocking send — won't stall the caller */
                xMessageBufferSend(xSyslogMessageBuffer, msg, len, 0);
            }
        }
    }
#else
    (void)msg;
#endif
}

/* ========================================================================== */
/*                 Private Functions                                          */
/* ========================================================================== */

#if CMAKE_ENABLE_SYSLOG
static int Logging_UdpSendInit(void)
{
    jerry_device_holding_registers_t *hrRegs;
    uint32_t                          sntp_ip;

    g_syslog_conn = netconn_new(NETCONN_UDP);
    if (g_syslog_conn == NULL) return -1;

    hrRegs  = jerry_device_get_holding_registers();
    sntp_ip = hrRegs->sntp_server_ip;

    IP_ADDR4(&g_dest_ip, (sntp_ip >> 24) & 0xFF, (sntp_ip >> 16) & 0xFF,
             (sntp_ip >> 8) & 0xFF, sntp_ip & 0xFF);

    err_t err = netconn_bind(g_syslog_conn, IP_ADDR_ANY, 0);
    if (err != ERR_OK)
    {
        netconn_delete(g_syslog_conn);
        g_syslog_conn = NULL;
        return -1;
    }

    err = netconn_connect(g_syslog_conn, &g_dest_ip, 514);
    if (err != ERR_OK)
    {
        netconn_delete(g_syslog_conn);
        g_syslog_conn = NULL;
        return -1;
    }

    return 0;
}

static int Logging_UdpSendString(const char *message)
{
    err_t err = ERR_OK;
    if (g_syslog_conn == NULL) return -1;

    uint16_t len = (uint16_t)strlen(message);
    struct netbuf *buf = netbuf_new();
    if (buf == NULL) return -1;

    void *data = netbuf_alloc(buf, len);
    if (data == NULL)
    {
        netbuf_delete(buf);
        return -1;
    }

    memcpy(data, message, len);

    err = netconn_send(g_syslog_conn, buf);

    netbuf_delete(buf);

    return (err == ERR_OK) ? 0 : -1;
}
#endif

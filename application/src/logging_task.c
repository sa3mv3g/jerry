/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_log.h"
#include "app_main.h"
#include "app_tasks.h"
#include "bsp.h"
#include "ip_specs.h"
#include "lwip/ip_addr.h"
#include "lwip/udp.h"
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
static struct udp_pcb    *g_udp_pcb = NULL;
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
    g_udp_pcb = udp_new();
    if (g_udp_pcb == NULL) return -1;

    IP4_ADDR(&g_dest_ip, SYSLOG_SERVER_IP_ADDR0, SYSLOG_SERVER_IP_ADDR1,
             SYSLOG_SERVER_IP_ADDR2, SYSLOG_SERVER_IP_ADDR3);

    err_t err = udp_bind(g_udp_pcb, IP_ADDR_ANY, 0);  // 0 = ephemeral port
    if (err != ERR_OK)
    {
        udp_remove(g_udp_pcb);
        g_udp_pcb = NULL;
        return -1;
    }

    return 0;
}

static int Logging_UdpSendString(const char *message)
{
    err_t err = ERR_OK;
    if (g_udp_pcb == NULL) return -1;

    uint16_t     len = (uint16_t)strlen(message);
    struct pbuf *p   = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_POOL);
    if (p == NULL) return -1;

    memcpy(p->payload, message, len);

    err = udp_sendto(g_udp_pcb, p, &g_dest_ip, 514);

    pbuf_free(p);

    return (err == ERR_OK) ? 0 : -1;
}
#endif

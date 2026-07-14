/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_log.h"
#include "app_tasks.h"
#include "ip_specs.h"
#include "lwip/ip_addr.h"
#include "lwip/udp.h"
#include "network_sync.h"
#include "queue.h"
#include "task.h"

/* ========================================================================== */
/*                 Private Definitions and Macros                             */
/* ========================================================================== */
#define SYSLOG_MAX_MSG_LEN (128U)
#define SYSLOG_QUEUE_DEPTH (10U)

/* ========================================================================== */
/*                 Private Typedefs                                           */
/* ========================================================================== */
typedef struct
{
    int  facility;
    int  severity;
    char msg[SYSLOG_MAX_MSG_LEN];
} SyslogEntry_t;

/* ========================================================================== */
/*                 Private Function Prototype                                 */
/* ========================================================================== */
static int Logging_UdpSendInit(void);
static int Logging_UdpSendString(const char *message);

/* ========================================================================== */
/*                 Private Variable Declaration                               */
/* ========================================================================== */
static StaticQueue_t xSyslogQueueBuffer;
static QueueHandle_t xSyslogQueue = NULL;
static uint8_t xSyslogQueueStorage[SYSLOG_QUEUE_DEPTH * sizeof(SyslogEntry_t)];
static struct udp_pcb *g_udp_pcb = NULL;
static ip_addr_t       g_dest_ip;
/* ========================================================================== */
/*                 Public Functions                                           */
/* ========================================================================== */
void vLoggingTask(void *pvParameters)
{
    (void)pvParameters;

    /* let wait for all other task configuration to finish */
    xEventGroupSync(xSyncEventGroup, APPTASK_LOGGING_TASK_EVENT_MASK,
                    APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);

    /* Wait for network to be enabled */
    NetworkSync_WaitForTcpReady();

    xSyslogQueue = xQueueCreateStatic(SYSLOG_QUEUE_DEPTH, sizeof(SyslogEntry_t),
                                      xSyslogQueueStorage, &xSyslogQueueBuffer);

    Logging_UdpSendInit();

    for (;;)
    {
        static SyslogEntry_t entry;
        if (xQueueReceive(xSyslogQueue, &entry, portMAX_DELAY) == pdPASS)
        {
            static char buf[256];
            int         priority = (entry.facility * 8) + entry.severity;

            snprintf(buf, sizeof(buf),
                     "<%d>1 - STM32H5 app - - - \xEF\xBB\xBF%s", priority,
                     entry.msg);

            Logging_UdpSendString(buf);
        }
    }
}

void Applog_Syslog(int facility, int severity, const char *msg)
{
    if (NULL != xSyslogQueue)
    {
        SyslogEntry_t entry;

        entry.facility = facility;
        entry.severity = severity;
        strncpy(entry.msg, msg, SYSLOG_MAX_MSG_LEN - 1);
        entry.msg[SYSLOG_MAX_MSG_LEN - 1] = '\0';

        /* Non-blocking send — won't stall the caller */
        if (xQueueSend(xSyslogQueue, &entry, 0) != pdPASS)
        {
            /* Queue full — message dropped, handle as needed */
        }
    }
}

/* ========================================================================== */
/*                 Private Functions                                          */
/* ========================================================================== */

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
    struct pbuf *p   = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == NULL) return -1;

    memcpy(p->payload, message, len);

    err = udp_sendto(g_udp_pcb, p, &g_dest_ip, 514);

    pbuf_free(p);

    return (err == ERR_OK) ? 0 : -1;
}

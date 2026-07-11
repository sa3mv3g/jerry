#include "syslog_task.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "message_buffer.h"
#include "semphr.h"
#include "task.h"

#include "lwip/api.h"
#include "rtc_manager.h"
#include "log.h"
#include "app_tasks.h"
#include "bsp.h"

#define SYSLOG_MSG_BUFFER_SIZE  1024
#define SYSLOG_UDP_PORT         514
#define HOST_IP_ADDR0           169
#define HOST_IP_ADDR1           254
#define HOST_IP_ADDR2           4
#define HOST_IP_ADDR3           50

/* Static Message Buffer allocation */
static uint8_t ucMessageBufferStorage[SYSLOG_MSG_BUFFER_SIZE];
static StaticMessageBuffer_t xMessageBufferStruct;
static MessageBufferHandle_t xSyslogMessageBuffer = NULL;

/* Static Mutex allocation */
static StaticSemaphore_t xMutexBuffer;
static SemaphoreHandle_t xSyslogMutex = NULL;

/* Double-buffered payload arrays for Zero-Copy DMA */
static char syslog_payload_buffers[2][SYSLOG_MAX_MSG_LEN + 128];
static uint8_t current_buffer_idx = 0;

void SyslogTask_Init(void)
{
    if (xSyslogMessageBuffer == NULL)
    {
        xSyslogMessageBuffer = xMessageBufferCreateStatic(
            SYSLOG_MSG_BUFFER_SIZE, ucMessageBufferStorage, &xMessageBufferStruct);
    }
    if (xSyslogMutex == NULL)
    {
        xSyslogMutex = xSemaphoreCreateMutexStatic(&xMutexBuffer);
    }
}

void SyslogTask_SendLog(const char *log_str, size_t len)
{
    if (xSyslogMessageBuffer == NULL || xSyslogMutex == NULL)
    {
        return;
    }

    /* Do not block ISRs or critical tasks. If we can't get the mutex, drop log. */
    if (xPortIsInsideInterrupt())
    {
        /* Banned from ISR to prevent blocking/MessageBuffer issues */
        return;
    }

    if (xSemaphoreTake(xSyslogMutex, 0) == pdTRUE)
    {
        /* Send to message buffer (non-blocking if full) */
        xMessageBufferSend(xSyslogMessageBuffer, (const void *)log_str, len, 0);
        xSemaphoreGive(xSyslogMutex);
    }
}

void vSyslogTask(void *pvParameters)
{
    (void)pvParameters;
    
    struct netconn *conn = NULL;
    struct netbuf *buf = NULL;
    ip_addr_t host_ip;
    char raw_log[SYSLOG_MAX_MSG_LEN];

    /* Wait for network init to finish */
    extern EventGroupHandle_t xSyncEventGroup;
    if (xSyncEventGroup != NULL)
    {
        xEventGroupSync(xSyncEventGroup, APPTASK_SYSLOG_TASK_EVENT_MASK,
                        APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);
    }

    IP4_ADDR(&host_ip, HOST_IP_ADDR0, HOST_IP_ADDR1, HOST_IP_ADDR2, HOST_IP_ADDR3);

    conn = netconn_new(NETCONN_UDP);
    if (conn == NULL)
    {
        LOG_ERR("Failed to create Syslog UDP netconn");
        for (;;);
    }

    if (netconn_connect(conn, &host_ip, SYSLOG_UDP_PORT) != ERR_OK)
    {
        LOG_ERR("Failed to connect Syslog UDP netconn");
    }
    
    buf = netbuf_new();
    if (buf == NULL)
    {
        LOG_ERR("Failed to create Syslog netbuf");
        for (;;);
    }

    for (;;)
    {
        size_t rx_len = xMessageBufferReceive(xSyslogMessageBuffer, raw_log, sizeof(raw_log) - 1, portMAX_DELAY);
        if (rx_len > 0)
        {
            raw_log[rx_len] = '\0';

            /* RFC 5424 Format: <PRIVAL>VERSION TIMESTAMP HOSTNAME APP-NAME PROCID MSGID STRUCTURED-DATA MSG */
            /* PRIVAL 14 (User level, Info) */
            
            App_RTC_TimeTypeDef timeDate = {0};
            uint32_t ms = 0;
            int out_len = 0;
            char *out_buf = syslog_payload_buffers[current_buffer_idx];

            if (RTC_Manager_IsTimeSynced() && RTC_Manager_GetTimeAndDate(&timeDate))
            {
                ms = RTC_Manager_GetTimeWithMs(&timeDate);
                out_len = snprintf(out_buf, SYSLOG_MAX_MSG_LEN + 128,
                         "<14>1 20%02u-%02u-%02uT%02u:%02u:%02u.%03luZ Jerry STM32H5 - - - %s",
                         timeDate.year, timeDate.month, timeDate.date,
                         timeDate.hours, timeDate.minutes, timeDate.seconds, ms,
                         raw_log);
            }
            else
            {
                uint32_t ticks = xTaskGetTickCount();
                out_len = snprintf(out_buf, SYSLOG_MAX_MSG_LEN + 128,
                         "<14>1 - Jerry STM32H5 - - - [%lu] %s",
                         (unsigned long)ticks, raw_log);
            }

            /* Use netbuf_ref to point to our static buffer (Zero-Allocation constraint) */
            netbuf_ref(buf, out_buf, (u16_t)out_len);
            netconn_send(conn, buf);

            /* Toggle buffer to prevent DMA races */
            current_buffer_idx = (current_buffer_idx + 1) % 2;
        }
    }
}

#include <stdio.h>

#include "FreeRTOS.h"
#include "SEGGER_RTT.h"
#include "app_log.h"
#include "app_tasks.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/sockets.h"
#include "lwip/udp.h"
#include "rtc_manager.h"
#include "task.h"

#define SYSLOG_STATE_UNCONFIG          (0U)
#define SYSLOG_STATE_INIT              (1U)
#define SYSLOG_PRI(facility, severity) ((facility * 8) + severity)
#define FACILITY_USER                  (1U)
#define SEVERITY_INFO                  (6U)

/* Buffer for log module */
static char log_buffer[512];

static void log_hook_with_timestamp(const char *str)
{
    static char         ts_buf[64];
    App_RTC_TimeTypeDef timeDate = {0};

    if (RTC_Manager_GetTimeAndDate(&timeDate) == true)
    {
        /* Format timestamp using RTC */
        uint32_t ms = 0;
        ms          = RTC_Manager_GetTimeWithMs(&timeDate);
        int ts_len  = snprintf(
            ts_buf, sizeof(ts_buf), "[%04d-%02d-%02d %02d:%02d:%02d.%03lu] ",
            (int)(2000 + timeDate.year), (int)timeDate.month,
            (int)timeDate.date, (int)timeDate.hours, (int)timeDate.minutes,
            (int)timeDate.seconds, (unsigned long)ms);
        SEGGER_RTT_Write(0, ts_buf, ts_len);
    }
    else
    {
        /* Fallback if RTC fails */
        uint32_t ticks = xTaskGetTickCount();
        int      ts_len =
            snprintf(ts_buf, sizeof(ts_buf), "[%lu] ", (unsigned long)ticks);
        SEGGER_RTT_Write(0, ts_buf, ts_len);
    }

    /* Print the actual log message */
    int len = 0;
    while (str[len] != '\0') len++;
    SEGGER_RTT_Write(0, str, len);

    Applog_Syslog(1, 6, str);
}

void AppLog_Init(void)
{
    SEGGER_RTT_Init();
    log_init(log_buffer, sizeof(log_buffer), log_hook_with_timestamp);
#ifdef DEBUG
    log_set_level(LOG_LEVEL_VERBOSE);
#else
    log_set_level(LOG_LEVEL_WARNING);
#endif
}

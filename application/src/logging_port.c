#include <stdarg.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "SEGGER_RTT.h"
#include "app_log.h"
#include "rtc_manager.h"
#include "task.h"

#define SYSLOG_STATE_UNCONFIG          (0U)
#define SYSLOG_STATE_INIT              (1U)
#define SYSLOG_PRI(facility, severity) ((facility * 8) + severity)
#define FACILITY_USER                  (1U)
#define SYSLOG_HOSTNAME                "jerry"
#define SYSLOG_APP_NAME                "jerry"
static int g_log_level = LOG_LEVEL_WARNING;

void AppLog_SetLevel(int level) { g_log_level = level; }

int AppLog_GetLevel(void) { return g_log_level; }

void AppLog_Message(int level, int syslog_severity, const char* level_str,
                    const char* fmt, ...)
{
    if (level < g_log_level)
    {
        return;
    }

    char combined_buf[LOG_MAX_MSG_LEN];
    int  offset = 0;

    App_RTC_TimeTypeDef timeDate = {0};

    const char* task_name = "-";
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
        if (current_task != NULL)
        {
            task_name = pcTaskGetName(current_task);
        }
    }

    if (RTC_Manager_GetTimeAndDate(&timeDate) == true)
    {
        uint32_t ms = RTC_Manager_GetTimeWithMs(&timeDate);
        char     timestamp[32];
        snprintf(timestamp, sizeof(timestamp),
                 "20%02d-%02d-%02dT%02d:%02d:%02d.%03luZ", timeDate.year,
                 timeDate.month, timeDate.date, timeDate.hours,
                 timeDate.minutes, timeDate.seconds, (unsigned long)ms);

        offset =
            snprintf(combined_buf, sizeof(combined_buf),
                     "<%d>1 %s %s %s %s %s - \xEF\xBB\xBF",
                     SYSLOG_PRI(FACILITY_USER, syslog_severity),
                     timestamp,  // ISO 8601
                     SYSLOG_HOSTNAME, SYSLOG_APP_NAME, task_name, level_str);
    }
    else
    {
        uint32_t ticks = 0;
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        {
            ticks = xTaskGetTickCount();
        }
        offset = snprintf(combined_buf, sizeof(combined_buf), "[%lu] [%s] ",
                          (unsigned long)ticks, level_str);
    }

    if (offset < 0 || offset >= (int)sizeof(combined_buf))
    {
        return;  // snprintf error or buffer too small
    }

    va_list args;
    va_start(args, fmt);
    int msg_len =
        vsnprintf(combined_buf + offset, sizeof(combined_buf) - offset - 2, fmt,
                  args);  // leave room for \r\n
    va_end(args);

    if (msg_len > 0)
    {
        offset += msg_len;
        if (offset >= (int)sizeof(combined_buf) - 2)
        {
            offset = sizeof(combined_buf) - 3;
        }

        // Add explicit newline
        combined_buf[offset++] = '\r';
        combined_buf[offset++] = '\n';
        combined_buf[offset]   = '\0';

        SEGGER_RTT_Write(0, combined_buf, offset);

        // Pass the fully formatted string to the logging task
        Applog_Syslog(combined_buf);
    }
}

void AppLog_Init(void)
{
    SEGGER_RTT_Init();
#ifdef DEBUG
    AppLog_SetLevel(LOG_LEVEL_VERBOSE);
#else
    AppLog_SetLevel(LOG_LEVEL_WARNING);
#endif
}

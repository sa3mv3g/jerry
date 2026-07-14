#include <stdio.h>
#include <stdarg.h>

#include "FreeRTOS.h"
#include "SEGGER_RTT.h"
#include "app_log.h"
#include "rtc_manager.h"
#include "task.h"

#define SYSLOG_STATE_UNCONFIG          (0U)
#define SYSLOG_STATE_INIT              (1U)
#define SYSLOG_PRI(facility, severity) ((facility * 8) + severity)
#define FACILITY_USER                  (1U)

static int g_log_level = LOG_LEVEL_WARNING;

void log_set_level(int level)
{
    g_log_level = level;
}

int log_get_level(void)
{
    return g_log_level;
}

void AppLog_Message(int level, int syslog_severity, const char* level_str, const char* fmt, ...)
{
    if (level < g_log_level)
    {
        return;
    }

    char combined_buf[512];
    int offset = 0;
    
    App_RTC_TimeTypeDef timeDate = {0};

    if (RTC_Manager_GetTimeAndDate(&timeDate) == true)
    {
        uint32_t ms = RTC_Manager_GetTimeWithMs(&timeDate);
        offset = snprintf(
            combined_buf, sizeof(combined_buf), "[%04d-%02d-%02d %02d:%02d:%02d.%03lu] [%s] ",
            (int)(2000 + timeDate.year), (int)timeDate.month,
            (int)timeDate.date, (int)timeDate.hours, (int)timeDate.minutes,
            (int)timeDate.seconds, (unsigned long)ms, level_str);
    }
    else
    {
        uint32_t ticks = xTaskGetTickCount();
        offset = snprintf(combined_buf, sizeof(combined_buf), "[%lu] [%s] ", (unsigned long)ticks, level_str);
    }

    if (offset < 0 || offset >= (int)sizeof(combined_buf))
    {
        return; // snprintf error or buffer too small
    }

    va_list args;
    va_start(args, fmt);
    int msg_len = vsnprintf(combined_buf + offset, sizeof(combined_buf) - offset - 2, fmt, args); // leave room for \r\n
    va_end(args);

    if (msg_len > 0)
    {
        offset += msg_len;
        if (offset >= (int)sizeof(combined_buf) - 2)
        {
            offset = sizeof(combined_buf) - 3;
        }
        
        // Strip trailing \n if present, so we don't get double newlines
        while (offset > 0 && (combined_buf[offset - 1] == '\n' || combined_buf[offset - 1] == '\r'))
        {
            offset--;
        }

        // Add explicit newline
        combined_buf[offset++] = '\r';
        combined_buf[offset++] = '\n';
        combined_buf[offset] = '\0';
        
        SEGGER_RTT_Write(0, combined_buf, offset);

        // Syslog format usually doesn't need \r\n, but keeping it is fine, or we can strip for syslog.
        // The original code passed the whole string to Applog_Syslog.
        Applog_Syslog(FACILITY_USER, syslog_severity, combined_buf);
    }
}

void AppLog_Init(void)
{
    SEGGER_RTT_Init();
#ifdef DEBUG
    log_set_level(LOG_LEVEL_VERBOSE);
#else
    log_set_level(LOG_LEVEL_WARNING);
#endif
}

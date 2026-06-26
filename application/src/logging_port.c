#include "logging_port.h"

#include <stdio.h>

#include "rtc_manager.h"

#ifndef BSP_USING_RTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

/* Buffer for log module */
static char log_buffer[512];

/* The actual hardware write function */
extern int _write(int file, char *ptr, int len);

static void log_hook_with_timestamp(const char *str)
{
    /* Format timestamp using RTC */
    App_RTC_TimeTypeDef timeDate = {0};
    uint32_t            ms       = 0;

    char ts_buf[64];

    if (RTC_Manager_GetTimeAndDate(&timeDate) == true)
    {
        ms         = RTC_Manager_GetTimeWithMs(&timeDate);
        int ts_len = snprintf(
            ts_buf, sizeof(ts_buf), "[%04d-%02d-%02d %02d:%02d:%02d.%03lu] ",
            (int)(2000 + timeDate.year), (int)timeDate.month,
            (int)timeDate.date, (int)timeDate.hours, (int)timeDate.minutes,
            (int)timeDate.seconds, (unsigned long)ms);
        _write(1, ts_buf, ts_len);
    }
    else
    {
        /* Fallback if RTC fails */
#ifdef BSP_USING_RTOS
        uint32_t ticks = xTaskGetTickCount();
#else
        uint32_t ticks = 0;  // HAL_GetTick();
#endif
        int ts_len =
            snprintf(ts_buf, sizeof(ts_buf), "[%lu] ", (unsigned long)ticks);
        _write(1, ts_buf, ts_len);
    }

    /* Print the actual log message */
    int len = 0;
    while (str[len] != '\0') len++;
    _write(1, (char *)str, len);
}

void Logging_Init(void)
{
    log_init(log_buffer, sizeof(log_buffer), log_hook_with_timestamp);
#ifdef DEBUG
    log_set_level(LOG_LEVEL_VERBOSE);
#else
    log_set_level(LOG_LEVEL_WARNING);
#endif
}

#include "rtc_manager.h"

#include <stdio.h>
#include <time.h>

#include "log.h"
#include "lwipopts.h"

void BSP_RTC_SetUnixTimestamp(unsigned int sec)
{
    // 1. Cast the 32-bit integer to a standard time_t type
    time_t raw_time = (time_t)sec;

    // 2. Convert to a broken-down time structure (UTC)
    struct tm *timeinfo;
    timeinfo = gmtime(&raw_time);

    // 3. Extract the exact values for your STM32 RTC driver
    uint8_t year =
        timeinfo->tm_year - 100;  // Years since 2000 (STM32 RTC standard)
    uint8_t month =
        timeinfo->tm_mon + 1;           // Months are 0-11 in C, RTC needs 1-12
    uint8_t day   = timeinfo->tm_mday;  // 1-31
    uint8_t hours = timeinfo->tm_hour;  // 0-23
    uint8_t mins  = timeinfo->tm_min;   // 0-59
    uint8_t secs  = timeinfo->tm_sec;   // 0-59

    printf("[RTC] Synced Time: 20%02d-%02d-%02d %02d:%02d:%02d UTC\n", year,
           month, day, hours, mins, secs);

    // TODO: Pass these variables into your HAL_RTC_SetTime() and
    // HAL_RTC_SetDate() functions!
}

void RTC_Manager_Init(void)
{
    // The Secure world has already initialized the RTC hardware.
    // Any application-level initialization can go here.
}

bool RTC_Manager_GetTimeAndDate(App_RTC_TimeTypeDef *pTimeDate)
{
    if (NULL != pTimeDate)
    {
        pTimeDate->hours           = 0;
        pTimeDate->minutes         = 0;
        pTimeDate->seconds         = 0;
        pTimeDate->date            = 0;
        pTimeDate->month           = 0;
        pTimeDate->year            = 0;
        pTimeDate->weekday         = 0;
        pTimeDate->subseconds      = 0;
        pTimeDate->second_fraction = 0;
    }
    return true;
}

uint32_t RTC_Manager_GetTimeWithMs(App_RTC_TimeTypeDef *pTimeDate)
{
    return RTC_Manager_GetTimeAndDate(pTimeDate);
}

bool RTC_Manager_SetTimeAndDate(const App_RTC_TimeTypeDef *pTimeDate)
{
    (void)pTimeDate;
    return true;
}

void RTC_Manager_PrintCurrentTime(void)
{
    App_RTC_TimeTypeDef timeDate;

    if (RTC_Manager_GetTimeAndDate(&timeDate))
    {
        LOG_INF("Current Time: %02d:%02d:%02d Date: %02d/%02d/%04d",
                timeDate.hours, timeDate.minutes, timeDate.seconds,
                timeDate.date, timeDate.month, 2000 + timeDate.year);
    }
    else
    {
        LOG_ERR("Error reading RTC");
    }
}

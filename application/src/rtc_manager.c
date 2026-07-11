#include "rtc_manager.h"

#include <stdio.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"
#include "log.h"

static bool sntp_is_synced = false;
static uint32_t sntp_sync_epoch = 0;
static uint32_t sntp_sync_ticks = 0;

void RTC_Manager_Init(void)
{
    // Software clock init
    sntp_is_synced = false;
    sntp_sync_epoch = 0;
    sntp_sync_ticks = 0;
}

bool RTC_Manager_IsTimeSynced(void)
{
    return sntp_is_synced;
}

bool RTC_Manager_GetTimeAndDate(App_RTC_TimeTypeDef *pTimeDate)
{
    if (!sntp_is_synced || pTimeDate == NULL)
    {
        return false;
    }

    uint32_t current_ticks = xTaskGetTickCount();
    uint32_t elapsed_ticks = current_ticks - sntp_sync_ticks;
    uint32_t elapsed_sec = elapsed_ticks / configTICK_RATE_HZ;
    uint32_t current_epoch = sntp_sync_epoch + elapsed_sec;

    uint32_t days = current_epoch / 86400U;
    uint32_t seconds_in_day = current_epoch % 86400U;

    pTimeDate->hours = (uint8_t)(seconds_in_day / 3600U);
    pTimeDate->minutes = (uint8_t)((seconds_in_day % 3600U) / 60U);
    pTimeDate->seconds = (uint8_t)(seconds_in_day % 60U);

    uint32_t year = 1970;
    while (1) {
        bool is_leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
        uint32_t days_in_year = is_leap ? 366 : 365;
        if (days >= days_in_year) {
            days -= days_in_year;
            year++;
        } else {
            break;
        }
    }

    bool is_leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
    uint8_t days_in_month[] = { 31, is_leap ? (uint8_t)29 : (uint8_t)28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    uint32_t month;
    for (month = 0; month < 12; month++) {
        if (days >= days_in_month[month]) {
            days -= days_in_month[month];
        } else {
            break;
        }
    }

    if (year >= 2000 && year <= 2099) {
        pTimeDate->year = (uint8_t)(year - 2000);
    } else {
        pTimeDate->year = 0;
    }
    
    pTimeDate->month = (uint8_t)(month + 1);
    pTimeDate->date = (uint8_t)(days + 1);
    pTimeDate->weekday = (uint8_t)(((current_epoch / 86400U) + 3) % 7 + 1); /* 1=Mon..7=Sun */

    return true;
}

uint32_t RTC_Manager_GetTimeWithMs(App_RTC_TimeTypeDef *pTimeDate)
{
    if (RTC_Manager_GetTimeAndDate(pTimeDate))
    {
        uint32_t current_ticks = xTaskGetTickCount();
        uint32_t elapsed_ticks = current_ticks - sntp_sync_ticks;
        uint32_t ms = (elapsed_ticks % configTICK_RATE_HZ) * (1000 / configTICK_RATE_HZ);
        return ms;
    }
    return 0;
}

bool RTC_Manager_SetTimeAndDate(const App_RTC_TimeTypeDef *pTimeDate)
{
    /* We don't use SECURE_RTC anymore, so this is unused or stubbed */
    (void)pTimeDate;
    return false; 
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
        LOG_ERR("Time not synced yet");
    }
}

void sntp_set_system_time(unsigned int sec)
{
    sntp_sync_epoch = sec;
    sntp_sync_ticks = xTaskGetTickCount();
    sntp_is_synced = true;

    App_RTC_TimeTypeDef timeDate;
    if (RTC_Manager_GetTimeAndDate(&timeDate))
    {
        LOG_INF("SNTP Time Sync: 20%02u-%02u-%02u %02u:%02u:%02u", 
                (unsigned int)timeDate.year, (unsigned int)timeDate.month, (unsigned int)timeDate.date, 
                (unsigned int)timeDate.hours, (unsigned int)timeDate.minutes, (unsigned int)timeDate.seconds);
    }
}

#include "rtc_manager.h"

#include <stdio.h>

#include "log.h"

void RTC_Manager_Init(void)
{
    // The Secure world has already initialized the RTC hardware.
    // Any application-level initialization can go here.
}

bool RTC_Manager_GetTimeAndDate(App_RTC_TimeTypeDef *pTimeDate)
{
    if (SECURE_RTC_GetTimeDate(pTimeDate) != 0)
    {
        return false;
    }

    return true;
}

uint32_t RTC_Manager_GetTimeWithMs(App_RTC_TimeTypeDef *pTimeDate)
{
    if (RTC_Manager_GetTimeAndDate(pTimeDate))
    {
        /* Calculate milliseconds from subseconds */
        /* Formula: ms = ((SecondFraction - SubSeconds) * 1000) /
         * (SecondFraction + 1) */
        if (pTimeDate->second_fraction > 0)
        {
            if (pTimeDate->subseconds > pTimeDate->second_fraction)
            {
                return 0;
            }
            uint32_t ms =
                ((pTimeDate->second_fraction - pTimeDate->subseconds) * 1000U) /
                (pTimeDate->second_fraction + 1U);
            return ms;
        }
    }
    return 0;
}

bool RTC_Manager_SetTimeAndDate(const App_RTC_TimeTypeDef *pTimeDate)
{
    if (SECURE_RTC_SetTimeDate(pTimeDate) != 0)
    {
        return false;
    }

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

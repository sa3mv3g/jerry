#include "rtc_manager.h"
#include <stdio.h>

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
        printf("Current Time: %02d:%02d:%02d Date: %02d/%02d/%04d\n", 
                timeDate.hours, timeDate.minutes, timeDate.seconds, 
                timeDate.date, timeDate.month, 2000 + timeDate.year);
    }
    else
    {
        printf("Error reading RTC\n");
    }
}

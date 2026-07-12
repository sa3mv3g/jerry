#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  Shared RTC Time and Date structure
 */
typedef struct
{
    uint8_t  hours;
    uint8_t  minutes;
    uint8_t  seconds;
    uint8_t  date;
    uint8_t  month;
    uint8_t  year;
    uint8_t  weekday;
    uint32_t subseconds;
    uint32_t second_fraction;
} App_RTC_TimeTypeDef;

/**
 * @brief Initialize the RTC Manager (if needed for app logic)
 */
void RTC_Manager_Init(void);

/**
 * @brief Print the current RTC time via debug interface
 */
void RTC_Manager_PrintCurrentTime(void);

/**
 * @brief Get the current RTC time and date from hardware
 *
 * @param pTimeDate Pointer to App_RTC_TimeTypeDef struct
 * @return true if successful, false otherwise
 */
bool RTC_Manager_GetTimeAndDate(App_RTC_TimeTypeDef *pTimeDate);

/**
 * @brief Get the current RTC time and date, along with calculated milliseconds
 *
 * @param pTimeDate Pointer to App_RTC_TimeTypeDef struct
 * @return Milliseconds part of the current time (0-999)
 */
uint32_t RTC_Manager_GetTimeWithMs(App_RTC_TimeTypeDef *pTimeDate);

/**
 * @brief Set the RTC time and date in hardware
 *
 * @param pTimeDate Pointer to App_RTC_TimeTypeDef struct
 * @return true if successful, false otherwise
 */
bool RTC_Manager_SetTimeAndDate(const App_RTC_TimeTypeDef *pTimeDate);

#endif  // RTC_MANAGER_H

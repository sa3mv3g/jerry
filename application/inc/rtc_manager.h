#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "secure_nsc.h"

/**
 * @brief Initialize the RTC Manager (if needed for app logic)
 */
void RTC_Manager_Init(void);

/**
 * @brief Check if SNTP time sync has occurred
 *
 * @return true if time is synced, false otherwise
 */
bool RTC_Manager_IsTimeSynced(void);

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

/**
 * @brief Hook for lwIP SNTP to set system time
 *
 * @param sec Seconds since Jan 1, 1970 (Unix Epoch)
 */
void sntp_set_system_time(unsigned int sec);

#endif  // RTC_MANAGER_H

#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "secure_nsc.h"

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
 * @brief Set the RTC time and date in hardware
 * 
 * @param pTimeDate Pointer to App_RTC_TimeTypeDef struct
 * @return true if successful, false otherwise
 */
bool RTC_Manager_SetTimeAndDate(const App_RTC_TimeTypeDef *pTimeDate);

#endif // RTC_MANAGER_H

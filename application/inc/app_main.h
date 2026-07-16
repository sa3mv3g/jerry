#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdbool.h>
#include <stdint.h>

#define APP_API_STATUS_OK    (0U)
#define APP_API_STATUS_ERROR (1U)

#define APP_MODBUSID_STR_MAX_SZ_BYTES (4U)

uint8_t App_GetModbusId();
char*   App_GetModbusIdString();

#endif
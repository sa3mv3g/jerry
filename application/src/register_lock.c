/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#include "register_lock.h"

#include "FreeRTOS.h"
#include "semphr.h"

/** Mutex guarding all access to the shared Modbus register data. */
static SemaphoreHandle_t s_register_mutex = NULL;

/** Static storage for the register-data mutex. */
static StaticSemaphore_t s_register_mutex_buffer;

void RegisterLock_Init(void)
{
    /* Static allocation — safe to create before the scheduler starts. */
    s_register_mutex = xSemaphoreCreateMutexStatic(&s_register_mutex_buffer);
}

void RegisterLock_Acquire(void)
{
    if (s_register_mutex != NULL)
    {
        (void)xSemaphoreTake(s_register_mutex, portMAX_DELAY);
    }
}

void RegisterLock_Release(void)
{
    if (s_register_mutex != NULL)
    {
        (void)xSemaphoreGive(s_register_mutex);
    }
}

/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * LwIP Architecture - FreeRTOS System Architecture Header
 */

#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define SYS_MBOX_NULL  ((QueueHandle_t)NULL)
#define SYS_SEM_NULL   ((SemaphoreHandle_t)NULL)
#define SYS_MUTEX_NULL ((SemaphoreHandle_t)NULL)

    typedef SemaphoreHandle_t sys_sem_t;
    typedef SemaphoreHandle_t sys_mutex_t;
    typedef QueueHandle_t     sys_mbox_t;
    typedef TaskHandle_t      sys_thread_t;
    typedef uint32_t          sys_prot_t;

#define sys_sem_valid(sem) (((sem) != NULL) && (*(sem) != NULL))
#define sys_sem_set_invalid(sem) \
    do                           \
    {                            \
        if ((sem) != NULL)       \
        {                        \
            *(sem) = NULL;       \
        }                        \
    } while (0)

#define sys_mutex_valid(mutex) (((mutex) != NULL) && (*(mutex) != NULL))
#define sys_mutex_set_invalid(mutex) \
    do                               \
    {                                \
        if ((mutex) != NULL)         \
        {                            \
            *(mutex) = NULL;         \
        }                            \
    } while (0)

#define sys_mbox_valid(mbox) (((mbox) != NULL) && (*(mbox) != NULL))
#define sys_mbox_set_invalid(mbox) \
    do                             \
    {                              \
        if ((mbox) != NULL)        \
        {                          \
            *(mbox) = NULL;        \
        }                          \
    } while (0)

#define sys_msleep(ms) vTaskDelay(pdMS_TO_TICKS(ms))

#ifdef __cplusplus
}
#endif

#endif /* LWIP_ARCH_SYS_ARCH_H */

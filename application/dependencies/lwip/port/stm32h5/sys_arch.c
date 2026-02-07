/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * LwIP System Architecture for FreeRTOS (Static Allocation)
 */

#include <string.h>

#include "FreeRTOS.h"
#include "lwip/opt.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#if !NO_SYS

/*-----------------------------------------------------------------------------------*/
/* Mutex Functions - Using Static Allocation */
/*-----------------------------------------------------------------------------------*/

/* Static mutex storage pool */
#define MAX_MUTEXES 8
static StaticSemaphore_t mutexBuffers[MAX_MUTEXES];
static uint8_t           mutexUsed[MAX_MUTEXES] = {0};

err_t sys_mutex_new(sys_mutex_t *mutex)
{
    int i;
    for (i = 0; i < MAX_MUTEXES; i++)
    {
        if (mutexUsed[i] == 0)
        {
            mutexUsed[i] = 1;
            *mutex       = xSemaphoreCreateMutexStatic(&mutexBuffers[i]);
            if (*mutex == NULL)
            {
                mutexUsed[i] = 0;
                SYS_STATS_INC(mutex.err);
                return ERR_MEM;
            }
            SYS_STATS_INC_USED(mutex);
            return ERR_OK;
        }
    }
    SYS_STATS_INC(mutex.err);
    return ERR_MEM;
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
    xSemaphoreTake(*mutex, portMAX_DELAY);
}

void sys_mutex_unlock(sys_mutex_t *mutex) { xSemaphoreGive(*mutex); }

void sys_mutex_free(sys_mutex_t *mutex)
{
    int i;
    SYS_STATS_DEC(mutex.used);
    vSemaphoreDelete(*mutex);
    /* Find and free the static buffer */
    for (i = 0; i < MAX_MUTEXES; i++)
    {
        if (*mutex == (SemaphoreHandle_t)&mutexBuffers[i])
        {
            mutexUsed[i] = 0;
            break;
        }
    }
    *mutex = NULL;
}

/*-----------------------------------------------------------------------------------*/
/* Semaphore Functions - Using Static Allocation */
/*-----------------------------------------------------------------------------------*/

#define MAX_SEMAPHORES 16
static StaticSemaphore_t semBuffers[MAX_SEMAPHORES];
static uint8_t           semUsed[MAX_SEMAPHORES] = {0};

err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    int i;
    for (i = 0; i < MAX_SEMAPHORES; i++)
    {
        if (semUsed[i] == 0)
        {
            semUsed[i] = 1;
            *sem = xSemaphoreCreateCountingStatic(0xFF, count, &semBuffers[i]);
            if (*sem == NULL)
            {
                semUsed[i] = 0;
                SYS_STATS_INC(sem.err);
                return ERR_MEM;
            }
            SYS_STATS_INC_USED(sem);
            return ERR_OK;
        }
    }
    SYS_STATS_INC(sem.err);
    return ERR_MEM;
}

void sys_sem_signal(sys_sem_t *sem) { xSemaphoreGive(*sem); }

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
    TickType_t startTick;
    TickType_t endTick;
    TickType_t elapsedTicks;

    startTick = xTaskGetTickCount();

    if (timeout == 0)
    {
        xSemaphoreTake(*sem, portMAX_DELAY);
        endTick      = xTaskGetTickCount();
        elapsedTicks = endTick - startTick;
        return (u32_t)(elapsedTicks * portTICK_PERIOD_MS);
    }
    else
    {
        if (xSemaphoreTake(*sem, pdMS_TO_TICKS(timeout)) == pdTRUE)
        {
            endTick      = xTaskGetTickCount();
            elapsedTicks = endTick - startTick;
            return (u32_t)(elapsedTicks * portTICK_PERIOD_MS);
        }
        else
        {
            return SYS_ARCH_TIMEOUT;
        }
    }
}

void sys_sem_free(sys_sem_t *sem)
{
    int i;
    SYS_STATS_DEC(sem.used);
    vSemaphoreDelete(*sem);
    for (i = 0; i < MAX_SEMAPHORES; i++)
    {
        if (*sem == (SemaphoreHandle_t)&semBuffers[i])
        {
            semUsed[i] = 0;
            break;
        }
    }
    *sem = NULL;
}

/*-----------------------------------------------------------------------------------*/
/* Mailbox Functions - Using Static Allocation */
/*-----------------------------------------------------------------------------------*/

#define MAX_MBOXES 8
#define MBOX_SIZE  16
static StaticQueue_t mboxStaticBuffers[MAX_MBOXES];
static uint8_t       mboxStorage[MAX_MBOXES][MBOX_SIZE * sizeof(void *)];
static uint8_t       mboxUsed[MAX_MBOXES] = {0};

err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    int i;
    (void)size; /* Use fixed size */

    for (i = 0; i < MAX_MBOXES; i++)
    {
        if (mboxUsed[i] == 0)
        {
            mboxUsed[i] = 1;
            *mbox       = xQueueCreateStatic(MBOX_SIZE, sizeof(void *),
                                             mboxStorage[i], &mboxStaticBuffers[i]);
            if (*mbox == NULL)
            {
                mboxUsed[i] = 0;
                SYS_STATS_INC(mbox.err);
                return ERR_MEM;
            }
            SYS_STATS_INC_USED(mbox);
            return ERR_OK;
        }
    }
    SYS_STATS_INC(mbox.err);
    return ERR_MEM;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    while (xQueueSendToBack(*mbox, &msg, portMAX_DELAY) != pdTRUE)
    {
        /* Keep trying */
    }
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    if (xQueueSendToBack(*mbox, &msg, 0) == pdTRUE)
    {
        return ERR_OK;
    }
    else
    {
        SYS_STATS_INC(mbox.err);
        return ERR_MEM;
    }
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendToBackFromISR(*mbox, &msg, &xHigherPriorityTaskWoken) ==
        pdTRUE)
    {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return ERR_OK;
    }
    else
    {
        return ERR_MEM;
    }
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
    TickType_t startTick;
    TickType_t endTick;
    TickType_t elapsedTicks;
    void      *dummyPtr;

    if (msg == NULL)
    {
        msg = &dummyPtr;
    }

    startTick = xTaskGetTickCount();

    if (timeout == 0)
    {
        xQueueReceive(*mbox, msg, portMAX_DELAY);
        endTick      = xTaskGetTickCount();
        elapsedTicks = endTick - startTick;
        return (u32_t)(elapsedTicks * portTICK_PERIOD_MS);
    }
    else
    {
        if (xQueueReceive(*mbox, msg, pdMS_TO_TICKS(timeout)) == pdTRUE)
        {
            endTick      = xTaskGetTickCount();
            elapsedTicks = endTick - startTick;
            return (u32_t)(elapsedTicks * portTICK_PERIOD_MS);
        }
        else
        {
            *msg = NULL;
            return SYS_ARCH_TIMEOUT;
        }
    }
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    void *dummyPtr;

    if (msg == NULL)
    {
        msg = &dummyPtr;
    }

    if (xQueueReceive(*mbox, msg, 0) == pdTRUE)
    {
        return 0;
    }
    else
    {
        return SYS_MBOX_EMPTY;
    }
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    int i;
    if (*mbox != NULL)
    {
        SYS_STATS_DEC(mbox.used);
        vQueueDelete(*mbox);
        for (i = 0; i < MAX_MBOXES; i++)
        {
            if (*mbox == (QueueHandle_t)&mboxStaticBuffers[i])
            {
                mboxUsed[i] = 0;
                break;
            }
        }
        *mbox = NULL;
    }
}

/*-----------------------------------------------------------------------------------*/
/* Thread Functions - Using Static Allocation */
/*-----------------------------------------------------------------------------------*/

#define MAX_THREADS       4
#define THREAD_STACK_SIZE 512
static StaticTask_t threadTCBs[MAX_THREADS];
static StackType_t  threadStacks[MAX_THREADS][THREAD_STACK_SIZE];
static uint8_t      threadUsed[MAX_THREADS] = {0};

sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg,
                            int stacksize, int prio)
{
    int i;
    (void)stacksize; /* Use fixed stack size */

    for (i = 0; i < MAX_THREADS; i++)
    {
        if (threadUsed[i] == 0)
        {
            threadUsed[i]           = 1;
            TaskHandle_t taskHandle = xTaskCreateStatic(
                thread, name, THREAD_STACK_SIZE, arg, (UBaseType_t)prio,
                threadStacks[i], &threadTCBs[i]);
            return taskHandle;
        }
    }
    return NULL;
}

/*-----------------------------------------------------------------------------------*/
/* Critical Section / Protection Functions */
/*-----------------------------------------------------------------------------------*/
#if SYS_LIGHTWEIGHT_PROT
static sys_prot_t critNestingCount;

sys_prot_t sys_arch_protect(void)
{
    taskENTER_CRITICAL();
    return critNestingCount++;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    (void)pval;
    critNestingCount--;
    taskEXIT_CRITICAL();
}
#endif /* SYS_LIGHTWEIGHT_PROT */

/*-----------------------------------------------------------------------------------*/
/* System Time Functions */
/*-----------------------------------------------------------------------------------*/
u32_t sys_now(void)
{
    return (u32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void sys_init(void) { /* Nothing to do for FreeRTOS */ }

#endif /* !NO_SYS */

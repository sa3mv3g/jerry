/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "bsp.h"
#include "task.h"
#include "timers.h"

/* LwIP includes for memory stats */
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/stats.h"

/* Stack size for the tasks */
#define MAIN_TASK_STACK_SIZE     256
#define LOG_TASK_STACK_SIZE      256
#define MODBUS_TASK_STACK_SIZE   512
#define FOTA_TASK_STACK_SIZE     512
#define MONITOR_TASK_STACK_SIZE  256 /* Increased from 128 for printf calls */
#define TCP_ECHO_TASK_STACK_SIZE 1024

/* ==========================================================================
 * Forward Declarations (MISRA 8.4)
 * ========================================================================== */
void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer,
                                   StackType_t**  ppxIdleTaskStackBuffer,
                                   uint32_t*      pulIdleTaskStackSize);
void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer,
                                    StackType_t**  ppxTimerTaskStackBuffer,
                                    uint32_t*      pulTimerTaskStackSize);
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName);
void vApplicationMallocFailedHook(void);
void vApplicationIdleHook(void);
void vApplicationTickHook(void);
void vMainTask(void* pvParameters);

#if LWIP_STATS
void print_lwip_memory_stats(void);
bool check_lwip_memory_critical(void);
#endif
void print_task_stack_usage(void);

/* Static Task Structures */
static StaticTask_t xMainTaskTCB;
static StackType_t  xMainTaskStack[MAIN_TASK_STACK_SIZE];

static StaticTask_t xLogTaskTCB;
static StackType_t  xLogTaskStack[LOG_TASK_STACK_SIZE];

static StaticTask_t xModbusTaskTCB;
static StackType_t  xModbusTaskStack[MODBUS_TASK_STACK_SIZE];

static StaticTask_t xFotaTaskTCB;
static StackType_t  xFotaTaskStack[FOTA_TASK_STACK_SIZE];

static StaticTask_t xMonitorTaskTCB;
static StackType_t  xMonitorTaskStack[MONITOR_TASK_STACK_SIZE];

static StaticTask_t xTcpEchoTaskTCB;
static StackType_t  xTcpEchoTaskStack[TCP_ECHO_TASK_STACK_SIZE];

/* Task Handles */
static TaskHandle_t xMainTaskHandle = NULL;

/* FreeRTOS Static Allocation Hooks */
void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer,
                                   StackType_t**  ppxIdleTaskStackBuffer,
                                   uint32_t*      pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t  xIdleTaskStack[configMINIMAL_STACK_SIZE];

    *ppxIdleTaskTCBBuffer   = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = xIdleTaskStack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer,
                                    StackType_t**  ppxTimerTaskStackBuffer,
                                    uint32_t*      pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t  xTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer   = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = xTimerTaskStack;
    *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

/* ==========================================================================
 * Debug/Diagnostic Hooks
 * ========================================================================== */

/**
 * @brief Stack Overflow Hook - Called when FreeRTOS detects stack overflow
 *
 * This hook is called when configCHECK_FOR_STACK_OVERFLOW is enabled and
 * a stack overflow is detected. The task name is printed to help identify
 * which task caused the overflow.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    (void)xTask;

    /* CRITICAL: Stack overflow detected! */
    (void)printf("\n\n");
    (void)printf(
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    (void)printf("!!! STACK OVERFLOW DETECTED !!!\n");
    (void)printf(
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    (void)printf("Task Name: %s\n", pcTaskName);
    (void)printf(
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    (void)printf("\n");

    /* Halt the system - in production, you might want to reset */
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        /* Infinite loop to halt execution */
    }
}

/**
 * @brief Malloc Failed Hook - Called when pvPortMalloc fails
 *
 * Note: This project uses static allocation, so this should not be called.
 * If it is called, it indicates a configuration error.
 */
void vApplicationMallocFailedHook(void)
{
    (void)printf("\n\n");
    (void)printf(
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    (void)printf("!!! MALLOC FAILED !!!\n");
    (void)printf(
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    (void)printf("FreeRTOS heap allocation failed!\n");
    (void)printf("This should not happen with static allocation.\n");
    (void)printf(
        "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    (void)printf("\n");

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        /* Infinite loop to halt execution */
    }
}

/* Idle Hook */
void vApplicationIdleHook(void) { /* Called when the idle task runs */ }

/* Tick Hook */
void vApplicationTickHook(void) { /* Called every tick interrupt */ }

/* ==========================================================================
 * LwIP Memory Monitoring
 * ========================================================================== */

#if LWIP_STATS
/**
 * @brief Print LwIP memory statistics
 *
 * Call this function periodically or when debugging memory issues.
 */
void print_lwip_memory_stats(void)
{
    (void)printf("\n=== LwIP Memory Statistics ===\n");

#if MEM_STATS
    (void)printf("Heap Memory:\n");
    (void)printf("  Available: %u bytes\n", (unsigned int)lwip_stats.mem.avail);
    (void)printf("  Used:      %u bytes\n", (unsigned int)lwip_stats.mem.used);
    (void)printf("  Max Used:  %u bytes\n", (unsigned int)lwip_stats.mem.max);
    (void)printf("  Errors:    %u\n", (unsigned int)lwip_stats.mem.err);
    if (lwip_stats.mem.err > 0)
    {
        (void)printf("  !!! MEMORY ALLOCATION ERRORS DETECTED !!!\n");
    }
#endif

#if MEMP_STATS
    (void)printf("\nMemory Pools:\n");

    /* PBUF pool - check for NULL before accessing */
    if (lwip_stats.memp[MEMP_PBUF] != NULL)
    {
        (void)printf("  PBUF Pool:\n");
        (void)printf("    Available: %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_PBUF]->avail);
        (void)printf("    Used:      %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_PBUF]->used);
        (void)printf("    Max Used:  %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_PBUF]->max);
        (void)printf("    Errors:    %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_PBUF]->err);
    }

    /* TCP_SEG pool - critical for diagnosing memory leaks with NETCONN_COPY */
    if (lwip_stats.memp[MEMP_TCP_SEG] != NULL)
    {
        (void)printf("  TCP_SEG Pool:\n");
        (void)printf("    Available: %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_TCP_SEG]->avail);
        (void)printf("    Used:      %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_TCP_SEG]->used);
        (void)printf("    Max Used:  %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_TCP_SEG]->max);
        (void)printf("    Errors:    %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_TCP_SEG]->err);
    }

    /* TCP PCB pool - check for NULL before accessing */
    if (lwip_stats.memp[MEMP_TCP_PCB] != NULL)
    {
        (void)printf("  TCP_PCB Pool:\n");
        (void)printf("    Available: %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_TCP_PCB]->avail);
        (void)printf("    Used:      %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_TCP_PCB]->used);
        (void)printf("    Max Used:  %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_TCP_PCB]->max);
        (void)printf("    Errors:    %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_TCP_PCB]->err);
    }

    /* NETCONN pool - check for NULL before accessing */
    if (lwip_stats.memp[MEMP_NETCONN] != NULL)
    {
        (void)printf("  NETCONN Pool:\n");
        (void)printf("    Available: %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_NETCONN]->avail);
        (void)printf("    Used:      %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_NETCONN]->used);
        (void)printf("    Max Used:  %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_NETCONN]->max);
        (void)printf("    Errors:    %u\n",
                     (unsigned int)lwip_stats.memp[MEMP_NETCONN]->err);
    }

    /* Check for errors - with NULL checks and explicit parentheses (MISRA 12.1)
     */
    if (((lwip_stats.memp[MEMP_PBUF] != NULL) &&
         (lwip_stats.memp[MEMP_PBUF]->err > 0)) ||
        ((lwip_stats.memp[MEMP_TCP_SEG] != NULL) &&
         (lwip_stats.memp[MEMP_TCP_SEG]->err > 0)) ||
        ((lwip_stats.memp[MEMP_TCP_PCB] != NULL) &&
         (lwip_stats.memp[MEMP_TCP_PCB]->err > 0)) ||
        ((lwip_stats.memp[MEMP_NETCONN] != NULL) &&
         (lwip_stats.memp[MEMP_NETCONN]->err > 0)))
    {
        (void)printf("\n  !!! MEMORY POOL EXHAUSTION DETECTED !!!\n");
    }
#endif

    (void)printf("==============================\n\n");
}

/**
 * @brief Check if LwIP memory is critically low
 * @return true if memory is critically low, false otherwise
 */
bool check_lwip_memory_critical(void)
{
    bool critical = false;

#if MEM_STATS
    /* Check if heap is more than 90% used */
    if (lwip_stats.mem.avail > 0)
    {
        uint32_t usage_percent =
            (lwip_stats.mem.used * 100U) / lwip_stats.mem.avail;
        if (usage_percent > 90U)
        {
            (void)printf("WARNING: LwIP heap usage at %u%%\n",
                         (unsigned int)usage_percent);
            critical = true;
        }
    }

    /* Check for allocation errors */
    if (lwip_stats.mem.err > 0)
    {
        (void)printf("ERROR: LwIP heap allocation errors: %u\n",
                     (unsigned int)lwip_stats.mem.err);
        critical = true;
    }
#endif

#if MEMP_STATS
    /* Check PBUF pool - with NULL check and explicit parentheses (MISRA 12.1)
     */
    if ((lwip_stats.memp[MEMP_PBUF] != NULL) &&
        (lwip_stats.memp[MEMP_PBUF]->err > 0))
    {
        (void)printf("ERROR: PBUF pool exhausted %u times\n",
                     (unsigned int)lwip_stats.memp[MEMP_PBUF]->err);
        critical = true;
    }

    /* Check TCP PCB pool - with NULL check and explicit parentheses
     * (MISRA 12.1) */
    if ((lwip_stats.memp[MEMP_TCP_PCB] != NULL) &&
        (lwip_stats.memp[MEMP_TCP_PCB]->err > 0))
    {
        (void)printf("ERROR: TCP_PCB pool exhausted %u times\n",
                     (unsigned int)lwip_stats.memp[MEMP_TCP_PCB]->err);
        critical = true;
    }

    /* Check NETCONN pool - with NULL check and explicit parentheses
     * (MISRA 12.1) */
    if ((lwip_stats.memp[MEMP_NETCONN] != NULL) &&
        (lwip_stats.memp[MEMP_NETCONN]->err > 0))
    {
        (void)printf("ERROR: NETCONN pool exhausted %u times\n",
                     (unsigned int)lwip_stats.memp[MEMP_NETCONN]->err);
        critical = true;
    }
#endif

    return critical;
}
#endif /* LWIP_STATS */

/* ==========================================================================
 * Task Stack Monitoring
 * ========================================================================== */

/**
 * @brief Print stack usage for all tasks
 *
 * Uses uxTaskGetStackHighWaterMark to show remaining stack space.
 * Lower values indicate higher stack usage.
 */
void print_task_stack_usage(void)
{
    (void)printf("\n=== Task Stack Usage ===\n");
    (void)printf("Task Name       Stack High Water Mark (words)\n");
    (void)printf("------------------------------------------------\n");

    /* Get stack high water mark for each task */
    TaskHandle_t task_handles[] = {
        xMainTaskHandle,
        /* Add other task handles here if needed */
    };
    const char* task_names[] = {
        "Main",
    };

    for (size_t i = 0; i < (sizeof(task_handles) / sizeof(task_handles[0]));
         i++)
    {
        if (task_handles[i] != NULL)
        {
            UBaseType_t hwm = uxTaskGetStackHighWaterMark(task_handles[i]);
            (void)printf("%-15s %u\n", task_names[i], (unsigned int)hwm);

            /* Warn if stack is getting low (less than 50 words remaining) */
            if (hwm < 50U)
            {
                (void)printf("  !!! WARNING: Stack critically low !!!\n");
            }
        }
    }

    (void)printf("================================================\n\n");
}

/* Main Entry Point */
int main(void)
{
    /* Initialize Hardware (BSP) */
    BSP_Init();

    /* Create Main Task */
    xMainTaskHandle = xTaskCreateStatic(
        vMainTask, "Main", MAIN_TASK_STACK_SIZE, NULL,
        (UBaseType_t)(tskIDLE_PRIORITY + 1U), xMainTaskStack, &xMainTaskTCB);

    if (NULL != xMainTaskHandle)
    {
        /* Start Scheduler */
        vTaskStartScheduler();
    }

    /* Should never reach here - MISRA 15.6 compliant compound statement */
    while (1)
    {
        /* Infinite loop */
    }
    return 0;
}

/* Main Task */
void vMainTask(void* pvParameters)
{
    (void)pvParameters;

    /* Initialize sub-systems */
    (void)xTaskCreateStatic(vLoggingTask, "Log", LOG_TASK_STACK_SIZE, NULL,
                            tskIDLE_PRIORITY + 1, xLogTaskStack, &xLogTaskTCB);

    (void)xTaskCreateStatic(vModbusTask, "Modbus", MODBUS_TASK_STACK_SIZE, NULL,
                            tskIDLE_PRIORITY + 2, xModbusTaskStack,
                            &xModbusTaskTCB);

    (void)xTaskCreateStatic(vFotaTask, "Fota", FOTA_TASK_STACK_SIZE, NULL,
                            tskIDLE_PRIORITY + 1, xFotaTaskStack,
                            &xFotaTaskTCB);

    (void)xTaskCreateStatic(vMonitorTask, "Monitor", MONITOR_TASK_STACK_SIZE,
                            NULL, tskIDLE_PRIORITY + 1, xMonitorTaskStack,
                            &xMonitorTaskTCB);

    (void)xTaskCreateStatic(vTcpEchoTask, "TcpEcho", TCP_ECHO_TASK_STACK_SIZE,
                            NULL, tskIDLE_PRIORITY + 1, xTcpEchoTaskStack,
                            &xTcpEchoTaskTCB);

    for (;;)
    {
        /* Main loop */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

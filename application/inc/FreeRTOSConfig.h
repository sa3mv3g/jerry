/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Ensure stdint is included */
#include <stdint.h>
extern uint32_t SystemCoreClock;

#define configUSE_PREEMPTION                    1U
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0U
#define configUSE_TICKLESS_IDLE                 0U
#define configCPU_CLOCK_HZ                      (SystemCoreClock)
#define configTICK_RATE_HZ                      ((TickType_t)1000U)
#define configMAX_PRIORITIES                    (5U)
#define configMINIMAL_STACK_SIZE                ((uint16_t)128U)
#define configMAX_TASK_NAME_LEN                 (16U)
#define configIDLE_SHOULD_YIELD                 1U
#define configUSE_TASK_NOTIFICATIONS            1U
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3U
#define configUSE_MUTEXES                       1U
#define configUSE_RECURSIVE_MUTEXES             1U
#define configUSE_COUNTING_SEMAPHORES           1U
#define configQUEUE_REGISTRY_SIZE               10U
#define configUSE_QUEUE_SETS                    0U
#define configUSE_TIME_SLICING                  0U
#define configUSE_NEWLIB_REENTRANT              0U
#define configENABLE_BACKWARD_COMPATIBILITY     0U
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5U
#define configUSE_16_BIT_TICKS                  1U

/* Memory allocation related definitions. */
#define configSUPPORT_STATIC_ALLOCATION 1U
#define configSUPPORT_DYNAMIC_ALLOCATION \
    0U /* Requirement: No dynamic allocation */
#define configTOTAL_HEAP_SIZE                                            \
    ((size_t)(10U * 1024U)) /* Not used but often required to be defined \
                             */
#define configAPPLICATION_ALLOCATED_HEAP 1U

/* Hook function related definitions. */
#define configUSE_IDLE_HOOK 1U
#define configUSE_TICK_HOOK 1U
#define configCHECK_FOR_STACK_OVERFLOW \
    2U /* Requirement: Monitor stack overflow */
#define configUSE_MALLOC_FAILED_HOOK       1U
#define configUSE_DAEMON_TASK_STARTUP_HOOK 0U

/* Run time and task stats gathering related definitions. */
#define configGENERATE_RUN_TIME_STATS        0U
#define configUSE_TRACE_FACILITY             1U /* Requirement: Track stack usage */
#define configUSE_STATS_FORMATTING_FUNCTIONS 0U

/* Co-routine related definitions. */
#define configUSE_CO_ROUTINES           0U
#define configMAX_CO_ROUTINE_PRIORITIES (2U)

/* Software timer related definitions. */
#define configUSE_TIMERS             1U
#define configTIMER_TASK_PRIORITY    (configMAX_PRIORITIES - 1U)
#define configTIMER_QUEUE_LENGTH     10U
#define configTIMER_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 2U)

/* Interrupt nesting behaviour configuration. */
#define configKERNEL_INTERRUPT_PRIORITY (0xFFU) /* Priority 15, or lowest */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (0x50U) /* Priority 5, or mid-range */

/* Define to trap errors during development. */
#define configASSERT(x)           \
    if ((x) == 0) {               \
        taskDISABLE_INTERRUPTS(); \
        for (;;);                 \
    }

/* Optional functions - most linkers will remove unused functions anyway. */
#define INCLUDE_vTaskPrioritySet            1U
#define INCLUDE_uxTaskPriorityGet           1U
#define INCLUDE_vTaskDelete                 1U
#define INCLUDE_vTaskCleanUpResources       1U
#define INCLUDE_vTaskSuspend                1U
#define INCLUDE_vTaskDelayUntil             1U
#define INCLUDE_vTaskDelay                  1U
#define INCLUDE_xTaskGetSchedulerState      1U
#define INCLUDE_xTaskGetCurrentTaskHandle   1U
#define INCLUDE_uxTaskGetStackHighWaterMark 1U
#define INCLUDE_xTaskGetIdleTaskHandle      1U
#define INCLUDE_eTaskGetState               1U
#define INCLUDE_xTimerPendFunctionCall      1U
#define INCLUDE_xTaskAbortDelay             1U
#define INCLUDE_xTaskGetHandle              1U
#define INCLUDE_xTaskResumeFromISR          1U

/* Cortex-M33 specific definitions */
#define configENABLE_FPU       1U
#define configENABLE_MPU       0U
#define configENABLE_TRUSTZONE 0U

#endif /* FREERTOS_CONFIG_H */

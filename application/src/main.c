
/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "SEGGER_RTT.h"
#include "app_log.h"
#include "app_tasks.h"
#include "bsp.h"
#include "digital_output.h"
#include "jerry_device_registers.h"
#include "lcd_manager.h"
#include "log.h"
#include "register_lock.h"
#include "rtc_manager.h"
#include "task.h"
#include "timers.h"
#ifdef ENABLE_I2C_DEVICE_SCAN
#include "i2c_scanner.h"
#endif
/* LwIP includes for memory stats */
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "lwip/stats.h"
#include "network_sync.h"

/* Stack size for the tasks */
#define MAIN_TASK_STACK_SIZE       512U
#define LOG_TASK_STACK_SIZE        1024U
#define MODBUS_TASK_STACK_SIZE     1024U
#define FOTA_TASK_STACK_SIZE       configMINIMAL_STACK_SIZE
#define MONITOR_TASK_STACK_SIZE    512U
#define TCP_ECHO_TASK_STACK_SIZE   1024U
#define LCD_MANAGE_TASK_STACK_SIZE 1024U

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

EventGroupHandle_t        xSyncEventGroup;
static StaticEventGroup_t xSyncEventGroupBuff;

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

static StaticTask_t xLcdManageTaskTCB;
static StackType_t  xLcdManageTaskStack[LCD_MANAGE_TASK_STACK_SIZE];

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
 * @brief Crude blocking busy-wait used by the fatal-fault LED indicator.
 *
 * Must not depend on the scheduler or SysTick (it runs with interrupts
 * disabled from a fault hook). The iteration count is approximate; the LED
 * *cadence*, not exact timing, identifies the fault.
 *
 * @param loops Number of empty loop iterations to spin.
 */
static void fatal_fault_busy_wait(volatile uint32_t loops)
{
    while (loops > 0U)
    {
        loops--;
    }
}

/* Approximate busy-wait counts (tuned to the core clock; timing is not
 * critical, the pattern is what identifies the fault). */
#define FATAL_FAULT_BLINK_ON_LOOPS  (600000U)  /* ~150 ms on  */
#define FATAL_FAULT_BLINK_OFF_LOOPS (600000U)  /* ~150 ms off */
#define FATAL_FAULT_BLINK_GAP_LOOPS (4000000U) /* ~1 s pause  */

/**
 * @brief Stack Overflow Hook - Called when FreeRTOS detects stack overflow
 *
 * This hook is called when configCHECK_FOR_STACK_OVERFLOW is enabled and a
 * stack overflow is detected. At this point the task stack is already
 * corrupted, so we must NOT use the logging subsystem (snprintf and friends
 * are stack-heavy and could fault or corrupt memory). Instead we disable
 * interrupts and drive the on-board RED LED in a characteristic pattern —
 * 3 fast blinks then a ~1 s pause, repeating forever — which is documented in
 * the README ("Diagnostic LED Patterns"). See BUG-08 / issue #28.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    /* Halt immediately: the stack is corrupt, so do nothing stack-heavy. */
    taskDISABLE_INTERRUPTS();

    /* Defensive: ensure the LED GPIO is configured even if init never ran. */
    (void)BSP_LED_Init(LED_RED);

    for (;;)
    {
        /* 3 fast blinks ... */
        for (uint32_t blink = 0U; blink < 3U; blink++)
        {
            (void)BSP_LED_On(LED_RED);
            fatal_fault_busy_wait(FATAL_FAULT_BLINK_ON_LOOPS);
            (void)BSP_LED_Off(LED_RED);
            fatal_fault_busy_wait(FATAL_FAULT_BLINK_OFF_LOOPS);
        }
        /* ... then a long pause, repeat forever. */
        fatal_fault_busy_wait(FATAL_FAULT_BLINK_GAP_LOOPS);
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
    LOG_ERR("");
    LOG_ERR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    LOG_ERR("!!! MALLOC FAILED !!!");
    LOG_ERR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    LOG_ERR("FreeRTOS heap allocation failed!");
    LOG_ERR("This should not happen with static allocation.");
    LOG_ERR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    LOG_ERR("");

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
        /* Infinite loop */
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
    LOG_INF("=== LwIP Memory Statistics ===");

#if MEM_STATS
    LOG_INF("Heap Memory:");
    LOG_INF("  Available: %u bytes", (unsigned int)lwip_stats.mem.avail);
    LOG_INF("  Used:      %u bytes", (unsigned int)lwip_stats.mem.used);
    LOG_INF("  Max Used:  %u bytes", (unsigned int)lwip_stats.mem.max);
    LOG_INF("  Errors:    %u", (unsigned int)lwip_stats.mem.err);
    if (lwip_stats.mem.err > 0)
    {
        LOG_INF("  !!! MEMORY ALLOCATION ERRORS DETECTED !!!");
    }
#endif

#if MEMP_STATS
    LOG_INF("\nMemory Pools:");

    /* PBUF pool - check for NULL before accessing */
    if (lwip_stats.memp[MEMP_PBUF] != NULL)
    {
        LOG_INF("  PBUF Pool:");
        LOG_INF("    Available: %u",
                (unsigned int)lwip_stats.memp[MEMP_PBUF]->avail);
        LOG_INF("    Used:      %u",
                (unsigned int)lwip_stats.memp[MEMP_PBUF]->used);
        LOG_INF("    Max Used:  %u",
                (unsigned int)lwip_stats.memp[MEMP_PBUF]->max);
        LOG_INF("    Errors:    %u",
                (unsigned int)lwip_stats.memp[MEMP_PBUF]->err);
    }

    /* TCP_SEG pool - critical for diagnosing memory leaks with NETCONN_COPY */
    if (lwip_stats.memp[MEMP_TCP_SEG] != NULL)
    {
        LOG_INF("  TCP_SEG Pool:");
        LOG_INF("    Available: %u",
                (unsigned int)lwip_stats.memp[MEMP_TCP_SEG]->avail);
        LOG_INF("    Used:      %u",
                (unsigned int)lwip_stats.memp[MEMP_TCP_SEG]->used);
        LOG_INF("    Max Used:  %u",
                (unsigned int)lwip_stats.memp[MEMP_TCP_SEG]->max);
        LOG_INF("    Errors:    %u",
                (unsigned int)lwip_stats.memp[MEMP_TCP_SEG]->err);
    }

    /* TCP PCB pool - check for NULL before accessing */
    if (lwip_stats.memp[MEMP_TCP_PCB] != NULL)
    {
        LOG_INF("  TCP_PCB Pool:");
        LOG_INF("    Available: %u",
                (unsigned int)lwip_stats.memp[MEMP_TCP_PCB]->avail);
        LOG_INF("    Used:      %u",
                (unsigned int)lwip_stats.memp[MEMP_TCP_PCB]->used);
        LOG_INF("    Max Used:  %u",
                (unsigned int)lwip_stats.memp[MEMP_TCP_PCB]->max);
        LOG_INF("    Errors:    %u",
                (unsigned int)lwip_stats.memp[MEMP_TCP_PCB]->err);
    }

    /* NETCONN pool - check for NULL before accessing */
    if (lwip_stats.memp[MEMP_NETCONN] != NULL)
    {
        LOG_INF("  NETCONN Pool:");
        LOG_INF("    Available: %u",
                (unsigned int)lwip_stats.memp[MEMP_NETCONN]->avail);
        LOG_INF("    Used:      %u",
                (unsigned int)lwip_stats.memp[MEMP_NETCONN]->used);
        LOG_INF("    Max Used:  %u",
                (unsigned int)lwip_stats.memp[MEMP_NETCONN]->max);
        LOG_INF("    Errors:    %u",
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
        LOG_ERR("  !!! MEMORY POOL EXHAUSTION DETECTED !!!");
    }
#endif

    LOG_INF("==============================");
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
            LOG_WRN("WARNING: LwIP heap usage at %u%%",
                    (unsigned int)usage_percent);
            critical = true;
        }
    }

    /* Check for allocation errors */
    if (lwip_stats.mem.err > 0)
    {
        LOG_ERR("ERROR: LwIP heap allocation errors: %u",
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
        LOG_ERR("ERROR: PBUF pool exhausted %u times",
                (unsigned int)lwip_stats.memp[MEMP_PBUF]->err);
        critical = true;
    }

    /* Check TCP PCB pool - with NULL check and explicit parentheses
     * (MISRA 12.1) */
    if ((lwip_stats.memp[MEMP_TCP_PCB] != NULL) &&
        (lwip_stats.memp[MEMP_TCP_PCB]->err > 0))
    {
        LOG_ERR("ERROR: TCP_PCB pool exhausted %u times",
                (unsigned int)lwip_stats.memp[MEMP_TCP_PCB]->err);
        critical = true;
    }

    /* Check NETCONN pool - with NULL check and explicit parentheses
     * (MISRA 12.1) */
    if ((lwip_stats.memp[MEMP_NETCONN] != NULL) &&
        (lwip_stats.memp[MEMP_NETCONN]->err > 0))
    {
        LOG_ERR("ERROR: NETCONN pool exhausted %u times",
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
    LOG_INF("=== Task Stack Usage ===");
    LOG_INF("Task Name       Stack High Water Mark (words)");
    LOG_INF("------------------------------------------------");

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
            LOG_INF("%-15s %u", task_names[i], (unsigned int)hwm);

            /* Warn if stack is getting low (less than 50 words remaining) */
            if (hwm < 50U)
            {
                LOG_INF("  !!! WARNING: Stack critically low !!!");
            }
        }
    }

    LOG_INF("================================================");
}

/* Main Entry Point */
int main(void)
{
    /* Relocate Vector Table to RAM so we can inject the RTT Control Block */
    {
        /* Must be 1024-byte aligned for Cortex-M33 with 150+ IRQs */
#if defined(__ICCARM__)
#pragma data_alignment = 1024
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION)
        __attribute__((aligned(1024)))
#elif defined(__GNUC__)
        __attribute__((aligned(1024)))
#endif
        static uint32_t ram_vector_table[256];

        uint32_t* flash_vector_table = (uint32_t*)SCB->VTOR;
        for (int i = 0; i < 256; i++)
        {
            ram_vector_table[i] = flash_vector_table[i];
        }

        /* Inject RTT Control Block address at offset 0x20 (Word 8) */
        extern SEGGER_RTT_CB _SEGGER_RTT;
        ram_vector_table[8] = (uint32_t)&_SEGGER_RTT + 2;

        /* Activate RAM Vector Table */
        SCB->VTOR = (uint32_t)ram_vector_table;
    }

    NetworkSync_Init();

    /* Initialize Logging */
    AppLog_Init();

    /* Initialize Hardware (BSP) */
    BSP_Init();

#ifdef ENABLE_I2C_DEVICE_SCAN
    /* Scan I2C bus for connected devices (optional feature) */
    I2C_ScanBus();
#endif

    xSyncEventGroup = xEventGroupCreateStatic(&xSyncEventGroupBuff);

    if (NULL != xSyncEventGroup)
    {
        /* Create Main Task */
        xMainTaskHandle =
            xTaskCreateStatic(vMainTask, "Main", MAIN_TASK_STACK_SIZE, NULL,
                              (UBaseType_t)(tskIDLE_PRIORITY + 1U),
                              xMainTaskStack, &xMainTaskTCB);

        if (NULL != xMainTaskHandle)
        {
            /* Start Scheduler */
            vTaskStartScheduler();
        }
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
                            (UBaseType_t)(tskIDLE_PRIORITY + 1U), xLogTaskStack,
                            &xLogTaskTCB);

    (void)xTaskCreateStatic(vModbusTask, "Modbus", MODBUS_TASK_STACK_SIZE, NULL,
                            (UBaseType_t)(tskIDLE_PRIORITY + 2U),
                            xModbusTaskStack, &xModbusTaskTCB);

    (void)xTaskCreateStatic(vFotaTask, "Fota", FOTA_TASK_STACK_SIZE, NULL,
                            (UBaseType_t)(tskIDLE_PRIORITY + 1U),
                            xFotaTaskStack, &xFotaTaskTCB);

    (void)xTaskCreateStatic(vMonitorTask, "Monitor", MONITOR_TASK_STACK_SIZE,
                            NULL, (UBaseType_t)(tskIDLE_PRIORITY + 1U),
                            xMonitorTaskStack, &xMonitorTaskTCB);

    (void)xTaskCreateStatic(vTcpEchoTask, "TcpEcho", TCP_ECHO_TASK_STACK_SIZE,
                            NULL, (UBaseType_t)(tskIDLE_PRIORITY + 1U),
                            xTcpEchoTaskStack, &xTcpEchoTaskTCB);

    (void)xTaskCreateStatic(vLcdManageTask, "LCDMan",
                            LCD_MANAGE_TASK_STACK_SIZE, NULL,
                            (UBaseType_t)(tskIDLE_PRIORITY + 1U),
                            xLcdManageTaskStack, &xLcdManageTaskTCB);

    xEventGroupSync(xSyncEventGroup, APPTASK_MAIN_TASK_EVENT_MASK,
                    APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);

    DigitalOutput_Init();
    DigitalOutput_SyncLcd();

    /* Counter for sub-interval updates (ADC/AO every 3 s = 6 × 500 ms) */
    uint32_t update_counter = 0U;

    for (;;)
    {
        /* Main loop: 500 ms interval */
        vTaskDelay(pdMS_TO_TICKS(500));

        /* Digital inputs: every loop (500 ms) */
        for (uint8_t digitalInputChannelIndex = BSP_GPIODI_INDEX_0;
             digitalInputChannelIndex <= BSP_GPIODI_INDEX_7;
             digitalInputChannelIndex++)
        {
            uint32_t digitalInputValue;
            bool     digitalInputStatus;

            if (BSP_GPIODI_Read(digitalInputChannelIndex, &digitalInputValue) ==
                BSP_OK)
            {
                if (digitalInputValue == GPIO_PIN_SET)
                {
                    digitalInputStatus = true;
                }
                else
                {
                    digitalInputStatus = false;
                }
                LcdManager_UpdateDigitalInputStatus(digitalInputChannelIndex,
                                                    digitalInputStatus);
            }
        }

        /* RTC time: every loop (500 ms) */
        App_RTC_TimeTypeDef timeDate;
        if (RTC_Manager_GetTimeAndDate(&timeDate))
        {
            LcdManager_UpdateTime(timeDate.hours, timeDate.minutes,
                                  timeDate.seconds);
        }

        /* Analog inputs (ADC) and outputs (AO): every 3 s (6 × 500 ms) */
        update_counter++;
        if (update_counter >= 6U)
        {
            uint16_t pwm_0_amplitude;
            uint16_t pwm_1_amplitude;

            update_counter = 0U;

            /* Update analog inputs (ADC) on LCD */
            float32_t adc_values[BSP_ADC1_NUM_CHANNELS];
            if (BSP_ADC1_IsFilterSettled() &&
                (BSP_ADC1_GetFilteredValuesAll(adc_values) == BSP_OK))
            {
                for (uint8_t ch = 0; ch < BSP_ADC1_NUM_CHANNELS; ch++)
                {
                    LcdManager_UpdateAnalogInput(ch, adc_values[ch] * 3300.0f);
                }
            }

            /* Update analog outputs (PWM duty cycles) on LCD */
            jerry_device_holding_registers_t* regs =
                jerry_device_get_holding_registers();
            RegisterLock_Acquire();
            pwm_0_amplitude = (uint16_t)regs->pwm_0_amplitude;
            pwm_1_amplitude = (uint16_t)regs->pwm_1_amplitude;
            RegisterLock_Release();

            LcdManager_UpdateAnalogOutput(0, pwm_0_amplitude);
            LcdManager_UpdateAnalogOutput(1, pwm_1_amplitude);
        }
    }
}

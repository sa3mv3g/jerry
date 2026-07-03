/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "bsp.h"
#include "jerry_device_registers.h"
#include "lcd_manager.h"
#include "log.h"
#include "register_lock.h"
#include "rtc_manager.h"
#include "task.h"

/* LwIP includes for memory stats */
#include "lwip/stats.h"

/* External diagnostic functions from main.c */
#if LWIP_STATS
extern void print_lwip_memory_stats(void);
extern bool check_lwip_memory_critical(void);
#endif
extern void print_task_stack_usage(void);

/* Configuration */
#define MONITOR_INTERVAL_MS       5000U /* Check every 5 seconds */
#define FULL_STATS_INTERVAL_COUNT 12U   /* Print full stats every 60 seconds */

/* Static counters for tracking errors */
static uint32_t s_last_mem_err     = 0;
static uint32_t s_last_pbuf_err    = 0;
static uint32_t s_last_tcp_seg_err = 0;
static uint32_t s_last_tcp_pcb_err = 0;
static uint32_t s_last_netconn_err = 0;

/**
 * @brief Check for new LwIP memory errors and report them immediately
 */
static void check_lwip_errors(void)
{
#if LWIP_STATS && MEM_STATS
    /* Check heap memory errors */
    if (lwip_stats.mem.err > s_last_mem_err)
    {
        LOG_ERR("!!! NEW LwIP HEAP ERROR !!! (total: %u)",
                (unsigned int)lwip_stats.mem.err);
        s_last_mem_err = lwip_stats.mem.err;
    }
#endif

#if LWIP_STATS && MEMP_STATS
    /* Check PBUF pool errors - with NULL check and explicit parentheses
     * (MISRA 12.1) */
    if ((lwip_stats.memp[MEMP_PBUF] != NULL) &&
        (lwip_stats.memp[MEMP_PBUF]->err > s_last_pbuf_err))
    {
        LOG_ERR("!!! NEW PBUF POOL ERROR !!! (total: %u)",
                (unsigned int)lwip_stats.memp[MEMP_PBUF]->err);
        s_last_pbuf_err = lwip_stats.memp[MEMP_PBUF]->err;
    }

    /* Check TCP_SEG pool errors - critical for NETCONN_COPY memory leak
     * diagnosis */
    if ((lwip_stats.memp[MEMP_TCP_SEG] != NULL) &&
        (lwip_stats.memp[MEMP_TCP_SEG]->err > s_last_tcp_seg_err))
    {
        LOG_ERR("!!! NEW TCP_SEG POOL ERROR !!! (total: %u)",
                (unsigned int)lwip_stats.memp[MEMP_TCP_SEG]->err);
        s_last_tcp_seg_err = lwip_stats.memp[MEMP_TCP_SEG]->err;
    }

    /* Check TCP PCB pool errors - with NULL check and explicit parentheses
     * (MISRA 12.1) */
    if ((lwip_stats.memp[MEMP_TCP_PCB] != NULL) &&
        (lwip_stats.memp[MEMP_TCP_PCB]->err > s_last_tcp_pcb_err))
    {
        LOG_ERR("!!! NEW TCP_PCB POOL ERROR !!! (total: %u)",
                (unsigned int)lwip_stats.memp[MEMP_TCP_PCB]->err);
        s_last_tcp_pcb_err = lwip_stats.memp[MEMP_TCP_PCB]->err;
    }

    /* Check NETCONN pool errors - with NULL check and explicit parentheses
     * (MISRA 12.1) */
    if ((lwip_stats.memp[MEMP_NETCONN] != NULL) &&
        (lwip_stats.memp[MEMP_NETCONN]->err > s_last_netconn_err))
    {
        LOG_ERR("!!! NEW NETCONN POOL ERROR !!! (total: %u)",
                (unsigned int)lwip_stats.memp[MEMP_NETCONN]->err);
        s_last_netconn_err = lwip_stats.memp[MEMP_NETCONN]->err;
    }
#endif
}

/**
 * @brief Check task stack high water marks and warn if low
 */
static void check_task_stacks(void)
{
    /* Check current task's stack */
    UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
    if (hwm < 50U)
    {
        LOG_WRN("!!! WARNING: Monitor task stack low: %u words !!!",
                (unsigned int)hwm);
    }
}

/**
 * @brief Print filtered ADC values for all channels
 */
static void print_adc_values(void)
{
    float32_t adc_values[BSP_ADC1_NUM_CHANNELS];

    /* Check if filter has settled */
    if (!BSP_ADC1_IsFilterSettled())
    {
        LOG_INF("[ADC] Filter settling... (%u/%u samples)",
                (unsigned int)BSP_ADC1_GetFilterSampleCount(),
                (unsigned int)BSP_ADC1_FILTER_SETTLING_SAMPLES);
        return;
    }

    /* Get all filtered ADC values */
    if (BSP_ADC1_GetFilteredValuesAll(adc_values) == BSP_OK)
    {
        LOG_INF("[ADC] ");
        for (uint8_t ch = 0; ch < BSP_ADC1_NUM_CHANNELS; ch++)
        {
            /* Convert to millivolts (assuming 3.3V reference) */
            uint32_t mv = (uint32_t)(adc_values[ch] * 3300.0f);
            LOG_INF("CH%u:%4umV ", (unsigned int)ch, (unsigned int)mv);
        }
    }
    else
    {
        LOG_ERR("[ADC] Error reading filtered values");
    }
}

/**
 * @brief Print digital input values for all DI channels (DI0-DI7)
 */
static void print_digital_inputs(void)
{
    static char log_buf[128];
    uint32_t    di_values[8];

    /* Read all digital inputs (for logging only; LCD updates are owned by Main
     * task to avoid lost-update races — see N-01). */
    for (uint8_t i = 0; i < 8U; i++)
    {
        if (BSP_GPIODI_Read(i, &di_values[i]) != BSP_OK)
        {
            di_values[i] = 0xFFU; /* Mark as error */
        }
    }

    int offset = snprintf(log_buf, sizeof(log_buf), "[DI] ");
    for (uint8_t i = 0; i < 8U; i++)
    {
        if (offset < (int)sizeof(log_buf))
        {
            if (di_values[i] == 0xFFU)
            {
                offset +=
                    snprintf(&log_buf[offset], sizeof(log_buf) - (size_t)offset,
                             "DI%u=ERR ", i);
            }
            else
            {
                offset +=
                    snprintf(&log_buf[offset], sizeof(log_buf) - (size_t)offset,
                             "DI%u=%u ", i, (unsigned int)di_values[i]);
            }
        }
    }
    LOG_INF("%s", log_buf);
}

/**
 * @brief Update Time and Analog Outputs on LCD
 */
static void update_time_and_ao(void)
{
    /* Logging only; LCD updates are owned by Main task to avoid lost-update
     * races — see N-01. */
    App_RTC_TimeTypeDef timeDate;
    if (RTC_Manager_GetTimeAndDate(&timeDate))
    {
        LOG_INF("[RTC] %02u:%02u:%02u", (unsigned int)timeDate.hours,
                (unsigned int)timeDate.minutes, (unsigned int)timeDate.seconds);
    }

    jerry_device_holding_registers_t* regs =
        jerry_device_get_holding_registers();

    /* Snapshot the shared register fields under the lock (BUG-04). */
    RegisterLock_Acquire();
    uint16_t pwm_0_duty = (uint16_t)regs->pwm_0_amplitude;
    uint16_t pwm_1_duty = (uint16_t)regs->pwm_1_amplitude;
    RegisterLock_Release();

    LOG_INF("[AO] PWM0:%u PWM1:%u", (unsigned int)pwm_0_duty,
            (unsigned int)pwm_1_duty);
}

/* Stack Monitor Task */
void vMonitorTask(void* pvParameters)
{
    (void)pvParameters;

    uint32_t stats_counter = 0;

    HAL_IWDG_Refresh(&hiwdg);

    xEventGroupSync(xSyncEventGroup, APPTASK_MONITOR_TASK_EVENT_MASK,
                    APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);

    LOG_INF("[Monitor] Task started - monitoring stack and LwIP memory");

    for (;;)
    {
        HAL_IWDG_Refresh(&hiwdg);

        /* Check for new errors (quick check every interval) */
        check_lwip_errors();
        check_task_stacks();

        /* Print digital input values every interval */
        print_digital_inputs();

        /* Print ADC values every interval */
        print_adc_values();

        /* Update Time and AO every interval */
        update_time_and_ao();

        /* Print full statistics periodically */
        stats_counter++;
        if (stats_counter >= FULL_STATS_INTERVAL_COUNT)
        {
            stats_counter = 0;

#if LWIP_STATS
            print_lwip_memory_stats();

            /* Check if memory is critically low */
            if (check_lwip_memory_critical())
            {
                LOG_ERR("!!! CRITICAL: LwIP memory is critically low !!!");
            }
#endif
            print_task_stack_usage();
        }

        vTaskDelay(pdMS_TO_TICKS(MONITOR_INTERVAL_MS));
    }
}

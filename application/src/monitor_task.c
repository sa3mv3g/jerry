/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 */

#include <stdbool.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "app_tasks.h"
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
        (void)printf("!!! NEW LwIP HEAP ERROR !!! (total: %u)\n",
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
        (void)printf("!!! NEW PBUF POOL ERROR !!! (total: %u)\n",
                     (unsigned int)lwip_stats.memp[MEMP_PBUF]->err);
        s_last_pbuf_err = lwip_stats.memp[MEMP_PBUF]->err;
    }

    /* Check TCP_SEG pool errors - critical for NETCONN_COPY memory leak
     * diagnosis */
    if ((lwip_stats.memp[MEMP_TCP_SEG] != NULL) &&
        (lwip_stats.memp[MEMP_TCP_SEG]->err > s_last_tcp_seg_err))
    {
        (void)printf("!!! NEW TCP_SEG POOL ERROR !!! (total: %u)\n",
                     (unsigned int)lwip_stats.memp[MEMP_TCP_SEG]->err);
        s_last_tcp_seg_err = lwip_stats.memp[MEMP_TCP_SEG]->err;
    }

    /* Check TCP PCB pool errors - with NULL check and explicit parentheses
     * (MISRA 12.1) */
    if ((lwip_stats.memp[MEMP_TCP_PCB] != NULL) &&
        (lwip_stats.memp[MEMP_TCP_PCB]->err > s_last_tcp_pcb_err))
    {
        (void)printf("!!! NEW TCP_PCB POOL ERROR !!! (total: %u)\n",
                     (unsigned int)lwip_stats.memp[MEMP_TCP_PCB]->err);
        s_last_tcp_pcb_err = lwip_stats.memp[MEMP_TCP_PCB]->err;
    }

    /* Check NETCONN pool errors - with NULL check and explicit parentheses
     * (MISRA 12.1) */
    if ((lwip_stats.memp[MEMP_NETCONN] != NULL) &&
        (lwip_stats.memp[MEMP_NETCONN]->err > s_last_netconn_err))
    {
        (void)printf("!!! NEW NETCONN POOL ERROR !!! (total: %u)\n",
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
        (void)printf("!!! WARNING: Monitor task stack low: %u words !!!\n",
                     (unsigned int)hwm);
    }
}

/* Stack Monitor Task */
void vMonitorTask(void* pvParameters)
{
    (void)pvParameters;

    uint32_t stats_counter = 0;

    (void)printf("[Monitor] Task started - monitoring stack and LwIP memory\n");

    for (;;)
    {
        /* Check for new errors (quick check every interval) */
        check_lwip_errors();
        check_task_stacks();

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
                (void)printf(
                    "!!! CRITICAL: LwIP memory is critically low !!!\n");
            }
#endif
            print_task_stack_usage();
        }

        vTaskDelay(pdMS_TO_TICKS(MONITOR_INTERVAL_MS));
    }
}

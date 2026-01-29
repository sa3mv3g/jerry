/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 */

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "task.h"

/* Logging Task */
void vLoggingTask(void* pvParameters)
{
    (void)pvParameters;

    /* Initialize UART for Logging (115200 bps) */
    /* UART_Init(LOG_UART, 115200); */

    for (;;)
    {
        /* Process log queue and send over UART */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/*
 * Copyright (c) 2026
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

    xEventGroupSync(xSyncEventGroup, APPTASK_LOGGING_TASK_EVENT_MASK,
                    APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);
    for (;;)
    {
        /* Process log queue and send over UART */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

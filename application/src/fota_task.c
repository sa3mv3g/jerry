/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "task.h"

#define CONST 10000

/* FOTA Task */
void vFotaTask(void* pvParameters)
{
    (void)pvParameters;

    /* Initialize FOTA (Flash, Security, etc.) */

    xEventGroupSync(xSyncEventGroup, APPTASK_FOTA_TASK_EVENT_MASK,
                    APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);

    for (;;)
    {
        /* Check for updates */
        /* Verify signature */
        /* Write to dual bank */
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/*
 * Copyright (c) 2026
 * All rights reserved.
 */

/**
 * @file    fota_task.c
 * @brief   FOTA task — starts the HTTP server for firmware download.
 *
 * The NS FOTA task is write-only:
 *   1. Wait for all tasks to sync (xEventGroupSync)
 *   2. Start the FOTA HTTP server on port 8080 (blocks forever)
 *
 * Firmware verification and bank activation happen in Secure main.c
 * on the next external POR — not here.
 */

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "fota_http_server.h"
#include "task.h"

void vFotaTask(void *pvParameters)
{
    (void)pvParameters;

    /* Wait for all tasks to complete their initialization */
    xEventGroupSync(xSyncEventGroup, APPTASK_FOTA_TASK_EVENT_MASK,
                    APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);

    /* Start the FOTA HTTP server.
     * Blocks indefinitely, accepting one firmware download at a time.
     * After a successful download, SECURE_FOTA_Stage() sets FOTA_PENDING
     * in EEPROM. The new firmware activates on the next external POR. */
    fota_http_server_run();

    /* Should never reach here */
    vTaskDelete(NULL);
}

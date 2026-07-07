/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "calibration_storage.h"
#include "fota_http_server.h"
#include "task.h"

/**
 * @file    fota_task.c
 * @brief   FOTA task — startup validation + HTTP server.
 *
 * On every boot:
 *   1. Wait for all tasks to sync (xEventGroupSync)
 *   2. Run startup validation: check FOTA_VALID_FLAG in EDATA
 *      - If flag not set → call SECURE_FOTA_Rollback() (does not return)
 *      - If flag set → firmware is valid, continue
 *   3. Start the FOTA HTTP server on port 8080 (blocks forever)
 *
 * After a successful FOTA update and self-test, the application calls
 * fota_mark_valid() to set the flag. Until then, any reset will trigger
 * rollback to the previous firmware.
 */

void vFotaTask(void *pvParameters)
{
    (void)pvParameters;

    /* Wait for all tasks to complete their initialization */
    xEventGroupSync(xSyncEventGroup, APPTASK_FOTA_TASK_EVENT_MASK,
                    APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);

    /* Startup validation: roll back if new firmware hasn't been marked valid.
     * fota_startup_check() calls SECURE_FOTA_Rollback() if the flag is not set,
     * which triggers a system reset — it does not return in that case. */
    fota_startup_check();

    /* Firmware is valid — start the FOTA HTTP server.
     * Blocks indefinitely, accepting one firmware upload at a time. */
    fota_http_server_run();

    /* Should never reach here */
    vTaskDelete(NULL);
}

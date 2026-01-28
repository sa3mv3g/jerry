/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 */

#include <stdio.h>

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "task.h"

/* Stack Monitor Task */
void vMonitorTask(void* pvParameters) {
    (void)pvParameters;

    for (;;) {
        /* Iterate over tasks and check stack usage */
        /* UBaseType_t uxHighWaterMark;
           uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL); */

        /* Log if dangerously low */
        /* Removed debug print */

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

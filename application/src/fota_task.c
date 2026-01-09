#include "FreeRTOS.h"
#include "app_tasks.h"
#include "task.h"

/* FOTA Task */
void vFotaTask(void* pvParameters) {
  (void)pvParameters;

  /* Initialize FOTA (Flash, Security, etc.) */

  for (;;) {
    /* Check for updates */
    /* Verify signature */
    /* Write to dual bank */
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

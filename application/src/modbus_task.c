#include "FreeRTOS.h"
#include "app_tasks.h"
#include "modbus_data.h"
#include "task.h"

static ModbusData_t sModbusData;

void vModbusTask(void* pvParameters) {
  (void)pvParameters;

  /* Initialize Modbus RTU (UART 19200, 8N2) */
  /* ModbusRTU_Init(19200, Parity_None, StopBits_2); */

  /* Initialize Modbus TCP/IP */
  /* ModbusTCP_Init(); */

  for (;;) {
    /* Handle Modbus Requests */
    /* Update sModbusData from Hardware (Inputs/ADC) */
    /* Update Hardware from sModbusData (Outputs/PWM) */

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

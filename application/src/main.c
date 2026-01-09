#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "task.h"

/* Stack size for the tasks */
#define MAIN_TASK_STACK_SIZE    256
#define LOG_TASK_STACK_SIZE     256
#define MODBUS_TASK_STACK_SIZE  512
#define FOTA_TASK_STACK_SIZE    512
#define MONITOR_TASK_STACK_SIZE 128

/* Static Task Structures */
static StaticTask_t xMainTaskTCB;
static StackType_t  xMainTaskStack[MAIN_TASK_STACK_SIZE];

static StaticTask_t xLogTaskTCB;
static StackType_t  xLogTaskStack[LOG_TASK_STACK_SIZE];

static StaticTask_t xModbusTaskTCB;
static StackType_t  xModbusTaskStack[MODBUS_TASK_STACK_SIZE];

static StaticTask_t xFotaTaskTCB;
static StackType_t  xFotaTaskStack[FOTA_TASK_STACK_SIZE];

static StaticTask_t xMonitorTaskTCB;
static StackType_t  xMonitorTaskStack[MONITOR_TASK_STACK_SIZE];

/* Task Handles */
TaskHandle_t xMainTaskHandle = NULL;

/* Prototypes */
void vMainTask(void* pvParameters);

/* FreeRTOS Static Allocation Hooks */
void vApplicationGetIdleTaskMemory(StaticTask_t** ppxIdleTaskTCBBuffer,
                                   StackType_t**  ppxIdleTaskStackBuffer,
                                   uint32_t*      pulIdleTaskStackSize) {
  static StaticTask_t xIdleTaskTCB;
  static StackType_t  xIdleTaskStack[configMINIMAL_STACK_SIZE];

  *ppxIdleTaskTCBBuffer   = &xIdleTaskTCB;
  *ppxIdleTaskStackBuffer = xIdleTaskStack;
  *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t** ppxTimerTaskTCBBuffer,
                                    StackType_t**  ppxTimerTaskStackBuffer,
                                    uint32_t*      pulTimerTaskStackSize) {
  static StaticTask_t xTimerTaskTCB;
  static StackType_t  xTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

  *ppxTimerTaskTCBBuffer   = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = xTimerTaskStack;
  *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

/* Stack Overflow Hook */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
  /* Handle stack overflow here */
  (void)xTask;
  (void)pcTaskName;
  /* e.g., Log error, reset system */
  while (1);
}

/* Idle Hook */
void vApplicationIdleHook(void) { /* Called when the idle task runs */ }

/* Tick Hook */
void vApplicationTickHook(void) { /* Called every tick interrupt */ }

/* Main Entry Point */
int main(void) {
  /* Initialize Hardware (BSP) */
  /* BSP_Init(); */

  /* Create Main Task */
  xMainTaskHandle =
      xTaskCreateStatic(vMainTask, "Main", MAIN_TASK_STACK_SIZE, NULL,
                        tskIDLE_PRIORITY + 1, xMainTaskStack, &xMainTaskTCB);

  /* Start Scheduler */
  vTaskStartScheduler();

  /* Should never reach here */
  while (1);
  return 0;
}

/* Main Task */
void vMainTask(void* pvParameters) {
  (void)pvParameters;

  /* Initialize sub-systems */
  xTaskCreateStatic(vLoggingTask, "Log", LOG_TASK_STACK_SIZE, NULL,
                    tskIDLE_PRIORITY + 1, xLogTaskStack, &xLogTaskTCB);

  xTaskCreateStatic(vModbusTask, "Modbus", MODBUS_TASK_STACK_SIZE, NULL,
                    tskIDLE_PRIORITY + 2, xModbusTaskStack, &xModbusTaskTCB);

  xTaskCreateStatic(vFotaTask, "Fota", FOTA_TASK_STACK_SIZE, NULL,
                    tskIDLE_PRIORITY + 1, xFotaTaskStack, &xFotaTaskTCB);

  xTaskCreateStatic(vMonitorTask, "Monitor", MONITOR_TASK_STACK_SIZE, NULL,
                    tskIDLE_PRIORITY + 1, xMonitorTaskStack, &xMonitorTaskTCB);

  for (;;) {
    /* Main loop */
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

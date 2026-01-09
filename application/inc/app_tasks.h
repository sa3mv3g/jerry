#ifndef APP_TASKS_H
#define APP_TASKS_H

/* Task Function Prototypes */
void vLoggingTask(void* pvParameters);
void vModbusTask(void* pvParameters);
void vFotaTask(void* pvParameters);
void vMonitorTask(void* pvParameters);

#endif /* APP_TASKS_H */

/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "FreeRTOS.h"
#include "event_groups.h"

#define APPTASK_LOGGING_TASK_EVENT_ID   (0U)
#define APPTASK_MODBUS_TASK_EVENT_ID    (1U)
#define APPTASK_FOTA_TASK_EVENT_ID      (2U)
#define APPTASK_MONITOR_TASK_EVENT_ID   (3U)
#define APPTASK_TCPECHO_TASK_EVENT_ID   (4U)
#define APPTASK_LCDMANAGE_TASK_EVENT_ID (5U)
#define APPTASK_MAIN_TASK_EVENT_ID      (6U)

#define APPTASK_LOGGING_TASK_EVENT_MASK (1U << APPTASK_LOGGING_TASK_EVENT_ID)
#define APPTASK_MODBUS_TASK_EVENT_MASK  (1U << APPTASK_MODBUS_TASK_EVENT_ID)
#define APPTASK_FOTA_TASK_EVENT_MASK    (1U << APPTASK_FOTA_TASK_EVENT_ID)
#define APPTASK_MONITOR_TASK_EVENT_MASK (1U << APPTASK_MONITOR_TASK_EVENT_ID)
#define APPTASK_TCPECHO_TASK_EVENT_MASK (1U << APPTASK_TCPECHO_TASK_EVENT_ID)
#define APPTASK_MAIN_TASK_EVENT_MASK    (1U << APPTASK_MAIN_TASK_EVENT_ID)
#define APPTASK_LCDMANAGE_TASK_EVENT_MASK \
    (1U << APPTASK_LCDMANAGE_TASK_EVENT_ID)

#define APPTASK_ALL_TASK_EVENT_MASK                                        \
    (APPTASK_LOGGING_TASK_EVENT_MASK | APPTASK_MODBUS_TASK_EVENT_MASK |    \
     APPTASK_FOTA_TASK_EVENT_MASK | APPTASK_MONITOR_TASK_EVENT_MASK |      \
     APPTASK_TCPECHO_TASK_EVENT_MASK | APPTASK_LCDMANAGE_TASK_EVENT_MASK | \
     APPTASK_MAIN_TASK_EVENT_MASK)

extern EventGroupHandle_t xSyncEventGroup;

/* Task Function Prototypes */
void vLoggingTask(void* pvParameters);
void vModbusTask(void* pvParameters);
void vFotaTask(void* pvParameters);
void vMonitorTask(void* pvParameters);
void vTcpEchoTask(void* pvParameters);
void vLcdManageTask(void* pvParameters);

#endif /* APP_TASKS_H */

#ifndef SYSLOG_TASK_H
#define SYSLOG_TASK_H

#include <stddef.h>
#include "FreeRTOS.h"
#include "task.h"

#define SYSLOG_MAX_MSG_LEN 256

/**
 * @brief FreeRTOS Task for sending syslogs over UDP
 *
 * @param pvParameters Task parameters
 */
void vSyslogTask(void *pvParameters);

/**
 * @brief Initialize the Syslog task synchronization primitives
 */
void SyslogTask_Init(void);

/**
 * @brief Enqueue a log string to the Syslog MessageBuffer
 *
 * @param log_str The null-terminated log string
 * @param len The length of the string
 */
void SyslogTask_SendLog(const char *log_str, size_t len);

#endif /* SYSLOG_TASK_H */

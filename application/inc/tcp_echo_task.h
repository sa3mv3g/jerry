/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#ifndef TCP_ECHO_TASK_H
#define TCP_ECHO_TASK_H

#include <stdint.h>

#define LWIP_STATUS_UP   (0U)
#define LWIP_STATUS_INIT (1U)

void vTcpEchoTask(void *pvParameters);

#endif /* TCP_ECHO_TASK_H */

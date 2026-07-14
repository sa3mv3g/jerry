#ifndef APP_NETWORK_SYNC_H
#define APP_NETWORK_SYNC_H

#include "FreeRTOS.h"
#include "event_groups.h"

#define TCP_STACK_READY_BIT (1 << 0)

void NetworkSync_Init(void);
void NetworkSync_SignalTcpReady(void);
void NetworkSync_WaitForTcpReady(void);

#endif  // APP_NETWORK_SYNC_H
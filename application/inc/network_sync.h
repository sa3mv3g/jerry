#ifndef APP_NETWORK_SYNC_H
#define APP_NETWORK_SYNC_H

#include "FreeRTOS.h"
#include "event_groups.h"

void NetworkSync_Init(void);

void       NetworkSync_SignalTcpReady(void);
BaseType_t NetworkSync_WaitForTcpReady(void);

void       NetworkSync_SignalDhcpReady(void);
BaseType_t NetworkSync_WaitForDhcpReady(void);

#endif  // APP_NETWORK_SYNC_H
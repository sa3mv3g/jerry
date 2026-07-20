// network_sync.c

#include "network_sync.h"

#define NETWORK_SYNC_TIMEOUT (1000U)
#define TCP_STACK_READY_BIT  (1 << 0)
#define DHCP_READY_BIT       (1 << 1)

static EventGroupHandle_t xNetworkEvents = NULL;
static StaticEventGroup_t xNetworkEventsBuff;

void NetworkSync_Init(void)
{
    xNetworkEvents = xEventGroupCreateStatic(&xNetworkEventsBuff);
}

void NetworkSync_SignalTcpReady(void)
{
    xEventGroupSetBits(xNetworkEvents, TCP_STACK_READY_BIT);
}

BaseType_t NetworkSync_WaitForTcpReady(void)
{
    // Convert 1000ms to the appropriate number of system ticks
    TickType_t xTicksToWait = pdMS_TO_TICKS(NETWORK_SYNC_TIMEOUT);

    EventBits_t uxBits = xEventGroupWaitBits(
        xNetworkEvents, TCP_STACK_READY_BIT,
        pdFALSE,      // Don't clear bit so others can also see it
        pdTRUE,       // Wait for all bits (only one bit requested here)
        xTicksToWait  // 1 second timeout
    );

    // Check if the TCP_STACK_READY_BIT is actually set in the returned bits
    if ((uxBits & TCP_STACK_READY_BIT) != 0)
    {
        return pdTRUE;  // Success: TCP Stack is ready
    }
    else
    {
        return pdFALSE;  // Timeout: TCP Stack did not become ready in 1s
    }
}

void NetworkSync_SignalDhcpReady(void)
{
    xEventGroupSetBits(xNetworkEvents, DHCP_READY_BIT);
}

BaseType_t NetworkSync_WaitForDhcpReady(void)
{
    TickType_t xTicksToWait = pdMS_TO_TICKS(NETWORK_SYNC_TIMEOUT);

    EventBits_t uxBits = xEventGroupWaitBits(
        xNetworkEvents, DHCP_READY_BIT,
        pdFALSE,      // Don't clear bit so others can also see it
        pdTRUE,       // Wait for all bits (only one bit requested here)
        xTicksToWait  // 1 second timeout
    );

    // Check if the DHCP_READY_BIT is actually set in the returned bits
    if ((uxBits & DHCP_READY_BIT) != 0)
    {
        return pdTRUE;  // Success: DHCP is ready
    }
    else
    {
        return pdFALSE;  // Timeout: DHCP did not become ready in 1s
    }
}
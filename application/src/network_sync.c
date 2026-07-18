// network_sync.c
#include "network_sync.h"

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

void NetworkSync_WaitForTcpReady(void)
{
    xEventGroupWaitBits(xNetworkEvents, TCP_STACK_READY_BIT,
                        pdFALSE,  // don't clear bit (others can wait too)
                        pdTRUE, portMAX_DELAY);
}

void NetworkSync_SignalDhcpReady(void)
{
    xEventGroupSetBits(xNetworkEvents, DHCP_READY_BIT);
}

void NetworkSync_WaitForDhcpReady(void)
{
    xEventGroupWaitBits(xNetworkEvents, DHCP_READY_BIT,
                        pdFALSE,  // don't clear bit (others can wait too)
                        pdTRUE, portMAX_DELAY);
}
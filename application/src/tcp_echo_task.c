/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#include "tcp_echo_task.h"

#include "app_log.h"
#include "app_main.h"
#include "app_tasks.h"
#include "bsp.h"
#include "ethernetif.h"
#include "ip_specs.h"
#include "lcd_manager.h"
#include "lwip/api.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/stats.h"
#include "lwip/tcpip.h"
#include "network_sync.h"

/* ========================================================================== */
/*                 Private Definitions and Macros                             */
/* ========================================================================== */

/* ========================================================================== */
/*                 Private Typedefs                                           */
/* ========================================================================== */

/* ========================================================================== */
/*                 Private Function Prototype                                 */
/* ========================================================================== */
extern err_t ethernetif_init(struct netif *netif);
static void  vEthernetTask(void *pvParameters);
#if LWIP_NETIF_LINK_CALLBACK
static void link_callback(struct netif *netif);
#endif
static void tcp_echo_thread(void *arg);
static void tcpip_init_done_callback(void *arg);

/* ========================================================================== */
/*                 Private Variable Declaration                               */
/* ========================================================================== */
struct netif             gnetif;
static StaticTask_t      xEthernetTaskTCB;
static StackType_t       xEthernetTaskStack[512];
static uint8_t           gLwipStackUp = LWIP_STATUS_INIT;
static SemaphoreHandle_t gEthernetLinkUpSem;
static StaticSemaphore_t gEthernetLinkUpSemBuff;

/* ========================================================================== */
/*                 Public Functions                                           */
/* ========================================================================== */
void vTcpEchoTask(void *pvParameters)
{
    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gw;

    (void)pvParameters;

    LOG_INF("TCP Echo Task Started");

    /* Initialize the LwIP stack */
    LOG_INF("Initializing LwIP...");
    tcpip_init(tcpip_init_done_callback, NULL);

    gEthernetLinkUpSem = xSemaphoreCreateBinaryStatic(&gEthernetLinkUpSemBuff);

#if CMAKE_ENABLE_DHCP
    /* Initialize IP addresses to zero for DHCP */
    IP4_ADDR(&ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);
#else
    /* Read device address from DEVADDR pins */
    uint8_t dev_addr     = BSP_GetDeviceAddress();
    uint8_t ip_last_byte = STATIC_IP_ADDR3_BASE + dev_addr;

    LOG_INF("Network: Device address from DEVADDR pins: %u", dev_addr);
    LOG_INF("Network: IP last octet: %u (base %u + DEVADDR %u)", ip_last_byte,
            STATIC_IP_ADDR3_BASE, dev_addr);

    /* Use static IP configuration with DEVADDR-based last octet */
    IP4_ADDR(&ipaddr, STATIC_IP_ADDR0, STATIC_IP_ADDR1, STATIC_IP_ADDR2,
             ip_last_byte);
    IP4_ADDR(&netmask, STATIC_NETMASK0, STATIC_NETMASK1, STATIC_NETMASK2,
             STATIC_NETMASK3);
    IP4_ADDR(&gw, STATIC_GW_ADDR0, STATIC_GW_ADDR1, STATIC_GW_ADDR2,
             STATIC_GW_ADDR3);
#endif /* USE_DHCP */

    /* Add the network interface */
    LOG_INF("Adding Network Interface...");

    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init,
              &tcpip_input);
    /* Set this to default interface. Required by DHCP */
    netif_set_default(&gnetif);

    /* Register link callback to log status changes */
#if LWIP_NETIF_LINK_CALLBACK
    netif_set_link_callback(&gnetif, link_callback);
#endif

    /* Create Ethernet Task to drive the interface */
    xTaskCreateStatic(vEthernetTask, "Ethernet", 512, &gnetif,
                      tskIDLE_PRIORITY + 2, xEthernetTaskStack,
                      &xEthernetTaskTCB);

    /* Always bring the interface up administratively so DHCP can start */
    netif_set_up(&gnetif);

    if (netif_is_link_up(&gnetif))
    {
        LOG_INF("Initial Link status: UP");
    }
    else
    {
        LOG_INF("Initial Link status: DOWN");
    }

    /* Print initial netif configuration */
    LOG_INF("=== Network Interface Configuration ===");
    LOG_INF("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", gnetif.hwaddr[0],
            gnetif.hwaddr[1], gnetif.hwaddr[2], gnetif.hwaddr[3],
            gnetif.hwaddr[4], gnetif.hwaddr[5]);
    LOG_INF("MTU: %u", gnetif.mtu);
    LOG_INF("Flags: 0x%02X (UP=%d, LINK_UP=%d, ETHARP=%d, BCAST=%d, IGMP=%d)",
            gnetif.flags, (gnetif.flags & NETIF_FLAG_UP) ? 1 : 0,
            (gnetif.flags & NETIF_FLAG_LINK_UP) ? 1 : 0,
            (gnetif.flags & NETIF_FLAG_ETHARP) ? 1 : 0,
            (gnetif.flags & NETIF_FLAG_BROADCAST) ? 1 : 0,
            (gnetif.flags & NETIF_FLAG_IGMP) ? 1 : 0);

#if CMAKE_ENABLE_DHCP
    /* Start DHCP to obtain IP address automatically */
    HAL_IWDG_Refresh(&hiwdg);

    dhcp_start(&gnetif);

    /* Wait for DHCP to obtain an IP address */
    LOG_INF("Waiting for DHCP to obtain IP address...");

    NetworkSync_WaitForDhcpReady();
#else
    /* Print static IP configuration */
    LOG_INF("=== Static IP Configuration ===");
    LOG_INF("IP Address: %s", ip4addr_ntoa(netif_ip4_addr(&gnetif)));
    LOG_INF("Netmask: %s", ip4addr_ntoa(netif_ip4_netmask(&gnetif)));
    LOG_INF("Gateway: %s", ip4addr_ntoa(netif_ip4_gw(&gnetif)));
    LOG_INF("===============================");
#endif /* USE_DHCP */

    LcdManager_UpdateIpv4Address(ip4addr_ntoa(netif_ip4_addr(&gnetif)));

    HAL_IWDG_Refresh(&hiwdg);

    NetworkSync_SignalTcpReady();

    xEventGroupSync(xSyncEventGroup, APPTASK_TCPECHO_TASK_EVENT_MASK,
                    APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);

    /* Create the TCP Echo Server thread */
    tcp_echo_thread(NULL);

    /* Should not reach here */
    vTaskDelete(NULL);
}

/* ========================================================================== */
/*                 Private Functions                                          */
/* ========================================================================== */

static void vEthernetTask(void *pvParameters)
{
    struct netif *netif = (struct netif *)pvParameters;
    int           count = 0;
    while (1)
    {
        /* Check link status periodically */
        ethernetif_check_link(netif);

        /* NOTE: ethernetif_poll() removed - packet reception is handled by
         * the interrupt-driven ethernetif_input_task in ethernetif.c.
         * Having both polling and interrupt-driven reception caused race
         * conditions and intermittent packet loss. */

        count++;
        if (count >= 10)
        { /* 5 seconds */
            count = 0;
            LOG_INF("Stats - RX: %d, TX: %d, DROP: %d, RX_INT: %u",
                    (int)lwip_stats.link.recv, (int)lwip_stats.link.xmit,
                    (int)lwip_stats.link.drop,
                    (unsigned int)ethernetif_get_rx_int_count());
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

#if LWIP_NETIF_LINK_CALLBACK
static void link_callback(struct netif *netif)
{
    if (netif_is_link_up(netif))
    {
        LOG_INF("Link status changed: UP");
    }
    else
    {
        LOG_INF("Link status changed: DOWN");
    }
}
#endif

static void tcp_echo_thread(void *arg)
{
    LWIP_UNUSED_ARG(arg);

    struct netconn *conn, *newconn;
    err_t           err;
    gLwipStackUp = LWIP_STATUS_INIT;

    /* Create a new connection identifier */
    conn = netconn_new(NETCONN_TCP);

    if (conn != NULL)
    {
        /* Bind connection to well known port 7 */
        err = netconn_bind(conn, NULL, 7);

        if (err == ERR_OK)
        {
            /* Tell connection to go into listening mode */
            netconn_listen(conn);
            LOG_INF("TCP Echo Server listening on port 7");

            while (1)
            {
                /* Grab new connection. */
                err = netconn_accept(conn, &newconn);

                /* Process the new connection. */
                if (err == ERR_OK)
                {
                    LOG_INF("New connection accepted");
                    struct netbuf *buf;
                    void          *data;
                    u16_t          len;

                    while ((err = netconn_recv(newconn, &buf)) == ERR_OK)
                    {
                        do
                        {
                            netbuf_data(buf, &data, &len);
                            err =
                                netconn_write(newconn, data, len, NETCONN_COPY);
                            if (err != ERR_OK)
                            {
                                LOG_ERR("TCP echo write error: %d", err);
                                /* Assume connection is broken, break inner loop
                                 */
                                break;
                            }
                        } while (netbuf_next(buf) >= 0);
                        netbuf_delete(buf);
                    }
                    /* Close connection and discard connection identifier. */
                    netconn_close(newconn);
                    netconn_delete(newconn);
                    LOG_INF("Connection closed");
                }
            }
        }
        else
        {
            LOG_ERR("Failed to bind connection: %d", err);
            netconn_delete(conn);
        }
    }
    else
    {
        LOG_ERR("Failed to create new connection");
    }
}

static void tcpip_init_done_callback(void *arg)
{
    (void)arg;
    LOG_INF("*** tcpip_thread is running! ***");
}

/* ========================================================================== */
/*                 Private Callback Handlers                                  */
/* ========================================================================== */

/* ========================================================================== */
/*                 Test/Debug/Other Sections                                  */
/* ========================================================================== */

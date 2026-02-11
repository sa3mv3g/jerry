/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 */

#include "tcp_echo_task.h"

#include <stdio.h>

#include "ethernetif.h"
#include "lwip/api.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"

/*---------------------------------------------------------------------------*/
/* IP Address Configuration                                                  */
/*---------------------------------------------------------------------------*/
/* Set USE_DHCP to 1 for dynamic IP (DHCP), 0 for static IP                  */
#define USE_DHCP 0

#if !USE_DHCP
/* Static IP Configuration - modify these values as needed */
#define STATIC_IP_ADDR0 169
#define STATIC_IP_ADDR1 254
#define STATIC_IP_ADDR2 4
#define STATIC_IP_ADDR3 100

#define STATIC_NETMASK0 255
#define STATIC_NETMASK1 255
#define STATIC_NETMASK2 255
#define STATIC_NETMASK3 0

#define STATIC_GW_ADDR0 169
#define STATIC_GW_ADDR1 254
#define STATIC_GW_ADDR2 4
#define STATIC_GW_ADDR3 1
#endif /* !USE_DHCP */
/*---------------------------------------------------------------------------*/

/* Define the network interface */
struct netif gnetif;

/* Ethernet Task resources */
static StaticTask_t xEthernetTaskTCB;
static StackType_t  xEthernetTaskStack[512];

/* External ethernetif initialization function */
/* This function is typically provided by the LwIP port or BSP */
extern err_t ethernetif_init(struct netif *netif);

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
        if (count >= 500)
        { /* 5 seconds */
            count = 0;
            (void)printf("Stats - RX: %d, TX: %d, DROP: %d, RX_INT: %u\n",
                         (int)lwip_stats.link.recv, (int)lwip_stats.link.xmit,
                         (int)lwip_stats.link.drop,
                         (unsigned int)ethernetif_get_rx_int_count());
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

#if LWIP_NETIF_LINK_CALLBACK
static void link_callback(struct netif *netif)
{
    if (netif_is_link_up(netif))
    {
        printf("Link status changed: UP\n");
        /* Optional: Re-trigger DHCP or other actions if needed */
    }
    else
    {
        printf("Link status changed: DOWN\n");
    }
}
#endif

static void tcp_echo_thread(void *arg)
{
    struct netconn *conn, *newconn;
    err_t           err;
    LWIP_UNUSED_ARG(arg);

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
            printf("TCP Echo Server listening on port 7\n");

            while (1)
            {
                /* Grab new connection. */
                err = netconn_accept(conn, &newconn);

                /* Process the new connection. */
                if (err == ERR_OK)
                {
                    printf("New connection accepted\n");
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
                        } while (netbuf_next(buf) >= 0);
                        netbuf_delete(buf);
                    }
                    /* Close connection and discard connection identifier. */
                    netconn_close(newconn);
                    netconn_delete(newconn);
                    printf("Connection closed\n");
                }
            }
        }
        else
        {
            printf("Failed to bind connection: %d\n", err);
            netconn_delete(conn);
        }
    }
    else
    {
        printf("Failed to create new connection\n");
    }
}

static void tcpip_init_done_callback(void *arg)
{
    (void)arg;
    printf("*** tcpip_thread is running! ***\n");
}

void vTcpEchoTask(void *pvParameters)
{
    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gw;

    (void)pvParameters;

    printf("TCP Echo Task Started\n");

    /* Initialize the LwIP stack */
    printf("Initializing LwIP...\n");
    tcpip_init(tcpip_init_done_callback, NULL);

#if USE_DHCP
    /* Initialize IP addresses to zero for DHCP */
    IP4_ADDR(&ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);
#else
    /* Use static IP configuration */
    IP4_ADDR(&ipaddr, STATIC_IP_ADDR0, STATIC_IP_ADDR1, STATIC_IP_ADDR2,
             STATIC_IP_ADDR3);
    IP4_ADDR(&netmask, STATIC_NETMASK0, STATIC_NETMASK1, STATIC_NETMASK2,
             STATIC_NETMASK3);
    IP4_ADDR(&gw, STATIC_GW_ADDR0, STATIC_GW_ADDR1, STATIC_GW_ADDR2,
             STATIC_GW_ADDR3);
#endif /* USE_DHCP */

    /* Add the network interface */
    printf("Adding Network Interface...\n");
    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init,
              &tcpip_input);

    /* Registers the default network interface */
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
        printf("Initial Link status: UP\n");
    }
    else
    {
        printf("Initial Link status: DOWN\n");
    }

    /* Print initial netif configuration */
    printf("=== Network Interface Configuration ===\n");
    printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", gnetif.hwaddr[0],
           gnetif.hwaddr[1], gnetif.hwaddr[2], gnetif.hwaddr[3],
           gnetif.hwaddr[4], gnetif.hwaddr[5]);
    printf("MTU: %u\n", gnetif.mtu);
    printf("Flags: 0x%02X (UP=%d, LINK_UP=%d, ETHARP=%d, BCAST=%d)\n",
           gnetif.flags, (gnetif.flags & NETIF_FLAG_UP) ? 1 : 0,
           (gnetif.flags & NETIF_FLAG_LINK_UP) ? 1 : 0,
           (gnetif.flags & NETIF_FLAG_ETHARP) ? 1 : 0,
           (gnetif.flags & NETIF_FLAG_BROADCAST) ? 1 : 0);
    printf("========================================\n");

#if USE_DHCP
    /* Start DHCP to obtain IP address automatically */
    printf("Starting DHCP...\n");
    dhcp_start(&gnetif);

    /* Wait for DHCP to obtain an IP address */
    printf("Waiting for DHCP to obtain IP address...\n");
    uint32_t dhcp_timeout = 0;
    while (!dhcp_supplied_address(&gnetif))
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        dhcp_timeout++;
        if (dhcp_timeout >= 300)
        { /* 30 seconds timeout */
            printf("DHCP timeout! Using link-local address.\n");
            break;
        }
    }

    /* Print obtained IP address */
    printf("=== DHCP Complete ===\n");
    printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(&gnetif)));
    printf("Netmask: %s\n", ip4addr_ntoa(netif_ip4_netmask(&gnetif)));
    printf("Gateway: %s\n", ip4addr_ntoa(netif_ip4_gw(&gnetif)));
    printf("=====================\n");
#else
    /* Print static IP configuration */
    printf("=== Static IP Configuration ===\n");
    printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(&gnetif)));
    printf("Netmask: %s\n", ip4addr_ntoa(netif_ip4_netmask(&gnetif)));
    printf("Gateway: %s\n", ip4addr_ntoa(netif_ip4_gw(&gnetif)));
    printf("===============================\n");
#endif /* USE_DHCP */

    /* Create the TCP Echo Server thread */
    tcp_echo_thread(NULL);

    /* Should not reach here */
    for (;;);
}

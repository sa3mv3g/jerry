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

/* Define the network interface */
struct netif gnetif;

/* Ethernet Task resources */
static StaticTask_t xEthernetTaskTCB;
static StackType_t  xEthernetTaskStack[512];

/* External ethernetif initialization function */
/* This function is typically provided by the LwIP port or BSP */
extern err_t ethernetif_init(struct netif *netif);

static void vEthernetTask(void *pvParameters) {
    struct netif *netif = (struct netif *)pvParameters;
    int           count = 0;
    while (1) {
        ethernetif_check_link(netif);
        ethernetif_poll(netif);

        count++;
        if (count >= 500) { /* 5 seconds */
            count = 0;
            printf("Stats - RX: %d, TX: %d, DROP: %d\n",
                   (int)lwip_stats.link.recv, (int)lwip_stats.link.xmit,
                   (int)lwip_stats.link.drop);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void link_callback(struct netif *netif) {
    if (netif_is_link_up(netif)) {
        printf("Link status changed: UP\n");
        /* Optional: Re-trigger DHCP or other actions if needed */
    } else {
        printf("Link status changed: DOWN\n");
    }
}

static void tcp_echo_thread(void *arg) {
    struct netconn *conn, *newconn;
    err_t           err;
    LWIP_UNUSED_ARG(arg);

    /* Create a new connection identifier */
    conn = netconn_new(NETCONN_TCP);

    if (conn != NULL) {
        /* Bind connection to well known port 7 */
        err = netconn_bind(conn, NULL, 7);

        if (err == ERR_OK) {
            /* Tell connection to go into listening mode */
            netconn_listen(conn);
            printf("TCP Echo Server listening on port 7\n");

            while (1) {
                /* Grab new connection. */
                err = netconn_accept(conn, &newconn);

                /* Process the new connection. */
                if (err == ERR_OK) {
                    printf("New connection accepted\n");
                    struct netbuf *buf;
                    void          *data;
                    u16_t          len;

                    while ((err = netconn_recv(newconn, &buf)) == ERR_OK) {
                        do {
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
        } else {
            printf("Failed to bind connection: %d\n", err);
            netconn_delete(conn);
        }
    } else {
        printf("Failed to create new connection\n");
    }
}

void vTcpEchoTask(void *pvParameters) {
    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gw;

    (void)pvParameters;

    printf("TCP Echo Task Started\n");

    /* Initialize the LwIP stack */
    printf("Initializing LwIP...\n");
    tcpip_init(NULL, NULL);

    /* IP address setting (Static IP: 192.168.2.100) */
    IP4_ADDR(&ipaddr, 192, 168, 2, 100);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 2, 1);

    /* Add the network interface */
    printf("Adding Network Interface...\n");
    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init,
              &tcpip_input);

    /* Registers the default network interface */
    netif_set_default(&gnetif);

    /* Register link callback to log status changes */
    netif_set_link_callback(&gnetif, link_callback);

    /* Create Ethernet Task to drive the interface */
    xTaskCreateStatic(vEthernetTask, "Ethernet", 512, &gnetif,
                      tskIDLE_PRIORITY + 2, xEthernetTaskStack,
                      &xEthernetTaskTCB);

    /* Always bring the interface up administratively so DHCP can start */
    netif_set_up(&gnetif);

    if (netif_is_link_up(&gnetif)) {
        printf("Initial Link status: UP\n");
    } else {
        printf("Initial Link status: DOWN\n");
    }

    /* Start DHCP (Disabled for Static IP) */
    /* printf("Starting DHCP...\n"); */
    /* dhcp_start(&gnetif); */

    /* Wait for IP address (Not needed for Static IP) */
    /* while (!dhcp_supplied_address(&gnetif)) */
    /* { */
    /*     vTaskDelay(pdMS_TO_TICKS(100)); */
    /* } */

    printf("Static IP address set: %s\n", ip4addr_ntoa(&ipaddr));

    /* Create the TCP Echo Server thread */
    tcp_echo_thread(NULL);

    /* Should not reach here */
    for (;;);
}

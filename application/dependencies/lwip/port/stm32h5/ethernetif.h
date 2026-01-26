/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * STM32H5 LwIP Ethernet Interface Header
 */

#ifndef ETHERNETIF_H
#define ETHERNETIF_H

#include "lwip/err.h"
#include "lwip/netif.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * MAC Address Configuration
 *
 * IMPORTANT: This is the SINGLE SOURCE OF TRUTH for the MAC address.
 * Both ethernetif.c and main.c (MX_ETH_Init) must use these values.
 * If they don't match, unicast packets will be filtered by hardware!
 *
 * The MAC address 00:80:E1:xx:xx:xx is in the locally administered range.  
 ******************************************************************************/
#define ETH_MAC_ADDR0   0x00U
#define ETH_MAC_ADDR1   0x80U
#define ETH_MAC_ADDR2   0xE1U
#define ETH_MAC_ADDR3   0x00U
#define ETH_MAC_ADDR4   0x00U
#define ETH_MAC_ADDR5   0x01U

/**
 * @brief Initialize the ethernetif and add it to a netif
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if initialization was successful
 */
err_t ethernetif_init(struct netif *netif);

/**
 * @brief Check and update the Ethernet link status
 * @param netif the lwip network interface structure
 */
void ethernetif_check_link(struct netif *netif);

/**
 * @brief Poll for received Ethernet frames
 * @param netif the lwip network interface structure
 */
void ethernetif_poll(struct netif *netif);

/**
 * @brief Process received Ethernet frames (call from main loop or task)
 * @param netif the lwip network interface structure
 */
void ethernetif_input(struct netif *netif);

/**
 * @brief Ethernet link monitoring thread (for FreeRTOS)
 * @param argument pointer to netif structure
 */
void ethernet_link_thread(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* ETHERNETIF_H */

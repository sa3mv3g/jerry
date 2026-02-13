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
extern "C"
{
#endif

    /*******************************************************************************
     * MAC Address Configuration
     *
     * The MAC address is dynamically generated from the MCU's Unique Device ID
     * (UID). The generated MAC address will be in the locally administered
     * range.
     ******************************************************************************/

    /**
     * @brief Get the MAC address derived from the MCU's Unique ID.
     * @param mac_addr Pointer to a 6-byte array to store the MAC address.
     */
    void ethernetif_get_mac_addr(uint8_t *mac_addr);

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

    /**
     * @brief Get the RX interrupt count (for debugging)
     * @return Number of RX interrupts received
     */
    uint32_t ethernetif_get_rx_int_count(void);

#ifdef __cplusplus
}
#endif

#endif /* ETHERNETIF_H */

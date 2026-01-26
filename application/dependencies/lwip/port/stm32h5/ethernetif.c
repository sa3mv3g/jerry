/**
  ******************************************************************************
  * @file    ethernetif.c
  * @brief   STM32H5 LwIP Ethernet Interface with HAL callback model
  * @note    Clean implementation with proper buffer tracking to prevent RBU errors.
  *          Each buffer can only be assigned to ONE descriptor at a time.
  ******************************************************************************
  */

/* Enable debug logging - set to 1 to enable, 0 to disable */
#define ETH_DEBUG_ENABLE  1

#if ETH_DEBUG_ENABLE
#include <stdio.h>
#define ETH_DEBUG(fmt, ...) printf("[ETH %lu] " fmt "\r\n", HAL_GetTick(), ##__VA_ARGS__)
#else
#define ETH_DEBUG(fmt, ...) ((void)0)
#endif

#include "stm32h5xx_hal.h"
#include "lwip/opt.h"
#include "lwip/timeouts.h"
#include "lwip/tcpip.h"
#include "lwip/etharp.h"
#include "lwip/stats.h"
#include "lwip/pbuf.h"
#include "lwip/memp.h"
#include "netif/ethernet.h"
#include "ethernetif.h"
#include "lan8742.h"
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#define ETHIF_TX_TIMEOUT               (2000U)
#define INTERFACE_THREAD_STACK_SIZE    (1024)
#define IFNAME0 's'
#define IFNAME1 't'
#define ETH_RX_BUFFER_SIZE             (1536U)

/* MAC address is defined in ethernetif.h - single source of truth */

/* DMA Descriptors in SRAM3 (non-cacheable) - defined in main.c */
extern ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT];
extern ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];

/* Use the ETH handle from main.c */
extern ETH_HandleTypeDef heth;

/* TxConfig from main.c */
extern ETH_TxPacketConfigTypeDef TxConfig;

/* RX buffers in SRAM3 for cache coherency
 * IMPORTANT: We need MORE buffers than descriptors to handle the case where
 * some buffers are being processed by the application while DMA needs new ones.
 * Using 2x the descriptor count gives us headroom.
 */
#define RX_BUFFER_COUNT  (ETH_RX_DESC_CNT * 2)
static uint8_t RxBuff[RX_BUFFER_COUNT][ETH_RX_BUFFER_SIZE]
    __attribute__((section(".driver.eth_mac0_rx_buf"), aligned(32)));

/* Buffer tracking:
 * - RxBuffAssigned: Bitmask tracking which buffers are currently assigned to DMA descriptors
 * - A buffer is "assigned" from when RxAllocateCallback returns it until RxLinkCallback processes it
 * - After RxLinkCallback, the HAL clears BackupAddr0, and the next RxAllocateCallback can reuse it
 */
static volatile uint32_t RxBuffAssigned = 0;  /* Bitmask: bit N = buffer N assigned to DMA */
static volatile uint32_t CurrentRxBuffIdx = 0;

static SemaphoreHandle_t RxPktSemaphore = NULL;
static SemaphoreHandle_t TxPktSemaphore = NULL;
static TaskHandle_t EthIfThread = NULL;

/* Static semaphore buffers for FreeRTOS */
static StaticSemaphore_t RxSemaphoreBuffer;
static StaticSemaphore_t TxSemaphoreBuffer;

static lan8742_Object_t LAN8742;

/* Forward declarations */
static void ethernetif_input_task(void *argument);
static int32_t ETH_PHY_IO_Init(void);
static int32_t ETH_PHY_IO_DeInit(void);
static int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal);
static int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal);
static int32_t ETH_PHY_IO_GetTick(void);

static lan8742_IOCtx_t LAN8742_IOCtx = {
    ETH_PHY_IO_Init, ETH_PHY_IO_DeInit, ETH_PHY_IO_WriteReg, ETH_PHY_IO_ReadReg, ETH_PHY_IO_GetTick
};

static int32_t ETH_PHY_IO_Init(void) { HAL_ETH_SetMDIOClockRange(&heth); return 0; }
static int32_t ETH_PHY_IO_DeInit(void) { return 0; }
static int32_t ETH_PHY_IO_ReadReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t *pRegVal) {
    return (HAL_ETH_ReadPHYRegister(&heth, DevAddr, RegAddr, pRegVal) == HAL_OK) ? 0 : -1;
}
static int32_t ETH_PHY_IO_WriteReg(uint32_t DevAddr, uint32_t RegAddr, uint32_t RegVal) {
    return (HAL_ETH_WritePHYRegister(&heth, DevAddr, RegAddr, RegVal) == HAL_OK) ? 0 : -1;
}
static int32_t ETH_PHY_IO_GetTick(void) { return (int32_t)HAL_GetTick(); }

/**
 * @brief  Find buffer index from buffer address
 * @param  buff: Buffer address to find
 * @retval Buffer index or -1 if not found
 */
static int32_t find_rx_buffer_index(uint8_t *buff)
{
    for (uint32_t i = 0; i < RX_BUFFER_COUNT; i++) {
        if (buff == &RxBuff[i][0]) {
            return (int32_t)i;
        }
    }
    return -1;
}

/**
 * @brief  HAL ETH RX Allocate Callback - Allocates buffer for DMA descriptor
 * @param  buff: Pointer to receive buffer pointer
 * @note   This is the KEY function for preventing RBU errors!
 *         We track which buffers are assigned to DMA and only return free ones.
 *         A buffer is marked as "assigned" here and freed in RxLinkCallback.
 */
void HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
    uint32_t startIdx = CurrentRxBuffIdx;

    /* Find a free buffer using round-robin search */
    for (uint32_t i = 0; i < RX_BUFFER_COUNT; i++) {
        uint32_t idx = (startIdx + i) % RX_BUFFER_COUNT;
        uint32_t mask = (1U << idx);

        /* Check if buffer is free (not assigned to any descriptor) */
        if ((RxBuffAssigned & mask) == 0) {
            /* Mark buffer as assigned to DMA */
            RxBuffAssigned |= mask;
            CurrentRxBuffIdx = (idx + 1) % RX_BUFFER_COUNT;
            *buff = &RxBuff[idx][0];
            return;
        }
    }

    /* No buffer available - this will cause RBU error! */
    ETH_DEBUG("RxAlloc FAILED! No free buffers");
    *buff = NULL;
}

/**
 * @brief  HAL ETH RX Link Callback - Links received data to pbuf chain
 * @param  pStart: Pointer to start of pbuf chain
 * @param  pEnd: Pointer to end of pbuf chain
 * @param  buff: Pointer to received data buffer
 * @param  Length: Length of received data
 * @note   We copy data from DMA buffer to a new pbuf, then FREE the buffer
 *         so it can be reused by RxAllocateCallback.
 */
void HAL_ETH_RxLinkCallback(void **pStart, void **pEnd, uint8_t *buff, uint16_t Length)
{
    struct pbuf **ppStart = (struct pbuf **)pStart;
    struct pbuf **ppEnd = (struct pbuf **)pEnd;
    struct pbuf *p;
    int32_t bufIdx;

    /* Find buffer index and mark it as free */
    bufIdx = find_rx_buffer_index(buff);
    if (bufIdx >= 0) {
        /* Clear the assigned bit - buffer is now free for reuse */
        RxBuffAssigned &= ~(1U << bufIdx);
    }

    /* Allocate a pbuf for the received frame segment */
    p = pbuf_alloc(PBUF_RAW, Length, PBUF_POOL);

    if (p != NULL) {
        /* Data Synchronization Barrier before reading DMA buffer */
        __DSB();
        pbuf_take(p, buff, Length);

        /* Chain the pbuf */
        if (*ppStart == NULL) {
            *ppStart = p;
        } else if (*ppEnd != NULL) {
            pbuf_cat(*ppEnd, p);
        }
        *ppEnd = p;
    } else {
        ETH_DEBUG("pbuf_alloc FAILED!");
    }
}

static void low_level_init(struct netif *netif)
{
    uint32_t duplex, speed = 0U;
    int32_t PHYLinkState;
    ETH_MACConfigTypeDef MACConf = {0};
    HAL_StatusTypeDef hal_status;

    ETH_DEBUG("low_level_init: Starting");

    /* Initialize buffer tracking - all buffers free */
    RxBuffAssigned = 0;
    CurrentRxBuffIdx = 0;

    /* Set MAC address from centralized definition in ethernetif.h */
    netif->hwaddr_len = ETH_HWADDR_LEN;
    netif->hwaddr[0] = ETH_MAC_ADDR0;
    netif->hwaddr[1] = ETH_MAC_ADDR1;
    netif->hwaddr[2] = ETH_MAC_ADDR2;
    netif->hwaddr[3] = ETH_MAC_ADDR3;
    netif->hwaddr[4] = ETH_MAC_ADDR4;
    netif->hwaddr[5] = ETH_MAC_ADDR5;
    netif->mtu = ETH_MAX_PAYLOAD;
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

    ETH_DEBUG("MAC: %02X:%02X:%02X:%02X:%02X:%02X",
              netif->hwaddr[0], netif->hwaddr[1], netif->hwaddr[2],
              netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]);

    memset(&TxConfig, 0, sizeof(ETH_TxPacketConfigTypeDef));
    /* Disable hardware checksum offload - LwIP calculates checksums in software
     * (CHECKSUM_GEN_* = 1 in lwipopts.h). Only enable CRC/PAD insertion. */
    TxConfig.Attributes = ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl = ETH_CHECKSUM_DISABLE;
    TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

    /* Create semaphores */
    RxPktSemaphore = xSemaphoreCreateBinaryStatic(&RxSemaphoreBuffer);
    TxPktSemaphore = xSemaphoreCreateBinaryStatic(&TxSemaphoreBuffer);

    /* Create RX task */
    static StaticTask_t xTaskBuffer;
    static StackType_t xStack[INTERFACE_THREAD_STACK_SIZE];
    EthIfThread = xTaskCreateStatic(ethernetif_input_task, "EthIf", INTERFACE_THREAD_STACK_SIZE,
                                    netif, configMAX_PRIORITIES - 1, xStack, &xTaskBuffer);

    /* Initialize PHY */
    LAN8742_RegisterBusIO(&LAN8742, &LAN8742_IOCtx);
    if (LAN8742_Init(&LAN8742) != LAN8742_STATUS_OK) {
        ETH_DEBUG("PHY init FAILED!");
        netif_set_link_down(netif);
        netif_set_down(netif);
        return;
    }

    PHYLinkState = LAN8742_GetLinkState(&LAN8742);

    /* Don't start ETH if link is down - wait for link to come up */
    if (PHYLinkState <= LAN8742_STATUS_LINK_DOWN) {
        ETH_DEBUG("Link DOWN, deferring ETH start");
        netif_set_up(netif);
        netif_set_link_down(netif);
        return;
    }

    /* Configure speed/duplex based on PHY state */
    switch (PHYLinkState) {
        case LAN8742_STATUS_100MBITS_FULLDUPLEX: duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_100M; break;
        case LAN8742_STATUS_100MBITS_HALFDUPLEX: duplex = ETH_HALFDUPLEX_MODE; speed = ETH_SPEED_100M; break;
        case LAN8742_STATUS_10MBITS_FULLDUPLEX:  duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_10M;  break;
        case LAN8742_STATUS_10MBITS_HALFDUPLEX:  duplex = ETH_HALFDUPLEX_MODE; speed = ETH_SPEED_10M;  break;
        default: duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_100M; break;
    }

    HAL_ETH_GetMACConfig(&heth, &MACConf);
    MACConf.DuplexMode = duplex;
    MACConf.Speed = speed;
    HAL_ETH_SetMACConfig(&heth, &MACConf);

    /* Memory barriers before starting DMA */
    __DSB();
    __ISB();

    hal_status = HAL_ETH_Start_IT(&heth);

    if (hal_status == HAL_OK) {
        netif_set_up(netif);
        netif_set_link_up(netif);
        ETH_DEBUG("ETH started, Link UP");
    } else {
        ETH_DEBUG("HAL_ETH_Start_IT FAILED!");
        netif_set_down(netif);
        netif_set_link_down(netif);
    }
}

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    (void)netif;
    uint32_t i = 0U;
    struct pbuf *q;
    ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT];
    HAL_StatusTypeDef tx_status;

    ETH_DEBUG("TX: len=%u", (unsigned)p->tot_len);

    memset(Txbuffer, 0, ETH_TX_DESC_CNT * sizeof(ETH_BufferTypeDef));
    for (q = p; q != NULL; q = q->next) {
        if (i >= ETH_TX_DESC_CNT) return ERR_IF;
        Txbuffer[i].buffer = q->payload;
        Txbuffer[i].len = q->len;
        if (i > 0) Txbuffer[i - 1].next = &Txbuffer[i];
        if (q->next == NULL) Txbuffer[i].next = NULL;
        i++;
    }

    TxConfig.Length = p->tot_len;
    TxConfig.TxBuffer = Txbuffer;
    TxConfig.pData = p;
    pbuf_ref(p);

    /* Memory barrier before TX */
    __DSB();

    tx_status = HAL_ETH_Transmit_IT(&heth, &TxConfig);
    while (tx_status != HAL_OK) {
        if (HAL_ETH_GetError(&heth) & HAL_ETH_ERROR_BUSY) {
            (void)xSemaphoreTake(TxPktSemaphore, pdMS_TO_TICKS(ETHIF_TX_TIMEOUT));
            HAL_ETH_ReleaseTxPacket(&heth);
            tx_status = HAL_ETH_Transmit_IT(&heth, &TxConfig);
        } else {
            ETH_DEBUG("TX FAILED!");
            pbuf_free(p);
            return ERR_IF;
        }
    }
    return ERR_OK;
}

static struct pbuf *low_level_input(struct netif *netif)
{
    (void)netif;
    struct pbuf *p = NULL;
    HAL_StatusTypeDef status;

    status = HAL_ETH_ReadData(&heth, (void **)&p);
    if (status == HAL_OK && p != NULL) {
        __DSB();
        ETH_DEBUG("RX: len=%u", (unsigned)p->tot_len);
    }
    return p;
}

void ethernetif_input(struct netif *netif)
{
    struct pbuf *p;
    p = low_level_input(netif);
    if (p != NULL) {
        if (netif->input(p, netif) != ERR_OK) {
            pbuf_free(p);
        }
    }
}

static void ethernetif_input_task(void *argument)
{
    struct pbuf *p;
    struct netif *netif = (struct netif *)argument;

    ETH_DEBUG("ethernetif_input_task: Started");

    for (;;) {
        if (xSemaphoreTake(RxPktSemaphore, pdMS_TO_TICKS(5000)) == pdTRUE) {
            do {
                p = low_level_input(netif);
                if (p != NULL) {
                    if (netif->input(p, netif) != ERR_OK) {
                        ETH_DEBUG("netif->input FAILED!");
                        pbuf_free(p);
                    }
                }
            } while (p != NULL);
        }
    }
}

err_t ethernetif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
#if LWIP_NETIF_HOSTNAME
    netif->hostname = "lwip";
#endif
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    netif->output = etharp_output;
    netif->linkoutput = low_level_output;
    low_level_init(netif);
    return ERR_OK;
}

void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth_param)
{
    (void)heth_param;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (RxPktSemaphore) {
        xSemaphoreGiveFromISR(RxPktSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth_param)
{
    (void)heth_param;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (TxPktSemaphore) {
        xSemaphoreGiveFromISR(TxPktSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void HAL_ETH_ErrorCallback(ETH_HandleTypeDef *heth_param)
{
    uint32_t error = HAL_ETH_GetDMAError(heth_param);

    if (error & ETH_DMACSR_RBU) {
        ETH_DEBUG("RBU Error - restarting RX DMA");
        /* Restart RX DMA by writing tail pointer */
        __DMB();
        WRITE_REG(heth_param->Instance->DMACRDTPR,
                  (uint32_t)(heth_param->Init.RxDesc + (ETH_RX_DESC_CNT - 1U)));
    }

    if (error & ETH_DMACSR_TBU) {
        ETH_DEBUG("TBU Error - restarting TX DMA");
        __DMB();
        WRITE_REG(heth_param->Instance->DMACTDTPR,
                  (uint32_t)(heth_param->Init.TxDesc + (ETH_TX_DESC_CNT - 1U)));
    }
}

/**
 * @brief  Check if PHY link state indicates link is truly UP
 */
static inline int is_phy_link_up(int32_t state)
{
    return (state >= LAN8742_STATUS_100MBITS_FULLDUPLEX &&
            state <= LAN8742_STATUS_10MBITS_HALFDUPLEX);
}

void ethernetif_check_link(struct netif *netif)
{
    int32_t PHYLinkState = LAN8742_GetLinkState(&LAN8742);

    /* Link was up, now it's down */
    if (netif_is_link_up(netif) && !is_phy_link_up(PHYLinkState)) {
        ETH_DEBUG("Link DOWN");
        HAL_ETH_Stop_IT(&heth);
        netif_set_down(netif);
        netif_set_link_down(netif);
    }
    /* Link was down, now it's up */
    else if (!netif_is_link_up(netif) && is_phy_link_up(PHYLinkState)) {
        ETH_MACConfigTypeDef MACConf = {0};
        uint32_t duplex = ETH_FULLDUPLEX_MODE, speed = ETH_SPEED_100M;

        ETH_DEBUG("Link UP");

        switch (PHYLinkState) {
            case LAN8742_STATUS_100MBITS_FULLDUPLEX: duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_100M; break;
            case LAN8742_STATUS_100MBITS_HALFDUPLEX: duplex = ETH_HALFDUPLEX_MODE; speed = ETH_SPEED_100M; break;
            case LAN8742_STATUS_10MBITS_FULLDUPLEX:  duplex = ETH_FULLDUPLEX_MODE; speed = ETH_SPEED_10M;  break;
            case LAN8742_STATUS_10MBITS_HALFDUPLEX:  duplex = ETH_HALFDUPLEX_MODE; speed = ETH_SPEED_10M;  break;
            default: break;
        }
        HAL_ETH_GetMACConfig(&heth, &MACConf);
        MACConf.DuplexMode = duplex;
        MACConf.Speed = speed;
        HAL_ETH_SetMACConfig(&heth, &MACConf);

        /* Reset buffer tracking before starting ETH */
        RxBuffAssigned = 0;
        CurrentRxBuffIdx = 0;

        /* Memory barriers before starting DMA */
        __DSB();
        __ISB();

        /* Start ETH - let HAL handle descriptor initialization */
        if (HAL_ETH_Start_IT(&heth) == HAL_OK) {
            netif_set_up(netif);
            netif_set_link_up(netif);
            ETH_DEBUG("ETH started");
        } else {
            ETH_DEBUG("HAL_ETH_Start_IT FAILED!");
        }
    }
}

void ethernetif_poll(struct netif *netif)
{
    ethernetif_input(netif);
}

void ethernet_link_thread(void *argument)
{
    struct netif *netif = (struct netif *)argument;

    for (;;) {
        ethernetif_check_link(netif);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

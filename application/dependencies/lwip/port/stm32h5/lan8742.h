/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * LAN8742A PHY Driver Header
 */

#ifndef LAN8742_H
#define LAN8742_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* LAN8742 PHY Address (NUCLEO-H563ZI uses address 0) */
#define LAN8742_PHY_ADDRESS 0x00U

/* LAN8742 Registers */
#define LAN8742_BCR     0x00U /* Basic Control Register */
#define LAN8742_BSR     0x01U /* Basic Status Register */
#define LAN8742_PHYI1R  0x02U /* PHY Identifier 1 */
#define LAN8742_PHYI2R  0x03U /* PHY Identifier 2 */
#define LAN8742_ANAR    0x04U /* Auto-Negotiation Advertisement */
#define LAN8742_ANLPAR  0x05U /* Auto-Negotiation Link Partner Ability */
#define LAN8742_ANER    0x06U /* Auto-Negotiation Expansion */
#define LAN8742_ANNPTR  0x07U /* Auto-Negotiation Next Page TX */
#define LAN8742_ANNPRR  0x08U /* Auto-Negotiation Next Page RX */
#define LAN8742_MMDACR  0x0DU /* MMD Access Control */
#define LAN8742_MMDAADR 0x0EU /* MMD Access Address Data */
#define LAN8742_ENCTR   0x10U /* EDPD NLP / Crossover Time */
#define LAN8742_MCSR    0x11U /* Mode Control/Status */
#define LAN8742_SMR     0x12U /* Special Modes */
#define LAN8742_TPDCR   0x18U /* TDR Patterns/Delay Control */
#define LAN8742_TCSR    0x19U /* TDR Control/Status */
#define LAN8742_SECR    0x1AU /* Symbol Error Counter */
#define LAN8742_SCSIR   0x1BU /* Special Control/Status Indications */
#define LAN8742_CLR     0x1CU /* Cable Length */
#define LAN8742_ISFR    0x1DU /* Interrupt Source Flag */
#define LAN8742_IMR     0x1EU /* Interrupt Mask */
#define LAN8742_PHYSCSR 0x1FU /* PHY Special Control/Status */

/* BCR Register Bits */
#define LAN8742_BCR_SOFT_RESET       0x8000U
#define LAN8742_BCR_LOOPBACK         0x4000U
#define LAN8742_BCR_SPEED_SELECT     0x2000U
#define LAN8742_BCR_AUTONEGO_EN      0x1000U
#define LAN8742_BCR_POWER_DOWN       0x0800U
#define LAN8742_BCR_ISOLATE          0x0400U
#define LAN8742_BCR_RESTART_AUTONEGO 0x0200U
#define LAN8742_BCR_DUPLEX_MODE      0x0100U

/* BSR Register Bits */
#define LAN8742_BSR_100BASE_T4       0x8000U
#define LAN8742_BSR_100BASE_TX_FD    0x4000U
#define LAN8742_BSR_100BASE_TX_HD    0x2000U
#define LAN8742_BSR_10BASE_T_FD      0x1000U
#define LAN8742_BSR_10BASE_T_HD      0x0800U
#define LAN8742_BSR_MF_PREAMBLE      0x0040U
#define LAN8742_BSR_AUTONEGO_CPLT    0x0020U
#define LAN8742_BSR_REMOTE_FAULT     0x0010U
#define LAN8742_BSR_AUTONEGO_ABILITY 0x0008U
#define LAN8742_BSR_LINK_STATUS      0x0004U
#define LAN8742_BSR_JABBER_DETECT    0x0002U
#define LAN8742_BSR_EXTENDED_CAP     0x0001U

/* PHYSCSR Register Bits */
#define LAN8742_PHYSCSR_AUTONEGO_DONE 0x1000U
#define LAN8742_PHYSCSR_HCDSPEEDMASK  0x001CU
#define LAN8742_PHYSCSR_10BT_HD       0x0004U
#define LAN8742_PHYSCSR_10BT_FD       0x0014U
#define LAN8742_PHYSCSR_100BTX_HD     0x0008U
#define LAN8742_PHYSCSR_100BTX_FD     0x0018U

/* Link State */
#define LAN8742_STATUS_READ_ERROR          -5
#define LAN8742_STATUS_WRITE_ERROR         -4
#define LAN8742_STATUS_ADDRESS_ERROR       -3
#define LAN8742_STATUS_RESET_TIMEOUT       -2
#define LAN8742_STATUS_ERROR               -1
#define LAN8742_STATUS_OK                  0
#define LAN8742_STATUS_LINK_DOWN           1
#define LAN8742_STATUS_100MBITS_FULLDUPLEX 2
#define LAN8742_STATUS_100MBITS_HALFDUPLEX 3
#define LAN8742_STATUS_10MBITS_FULLDUPLEX  4
#define LAN8742_STATUS_10MBITS_HALFDUPLEX  5
#define LAN8742_STATUS_AUTONEGO_NOTDONE    6

    /* IO Context Structure */
    typedef int32_t (*lan8742_Init_Func)(void);
    typedef int32_t (*lan8742_DeInit_Func)(void);
    typedef int32_t (*lan8742_WriteReg_Func)(uint32_t DevAddr, uint32_t RegAddr,
                                             uint32_t RegVal);
    typedef int32_t (*lan8742_ReadReg_Func)(uint32_t DevAddr, uint32_t RegAddr,
                                            uint32_t *pRegVal);
    typedef int32_t (*lan8742_GetTick_Func)(void);

    typedef struct
    {
        lan8742_Init_Func     Init;
        lan8742_DeInit_Func   DeInit;
        lan8742_WriteReg_Func WriteReg;
        lan8742_ReadReg_Func  ReadReg;
        lan8742_GetTick_Func  GetTick;
    } lan8742_IOCtx_t;

    /* Object Structure */
    typedef struct
    {
        uint32_t        DevAddr;
        uint32_t        Is_Initialized;
        lan8742_IOCtx_t IO;
    } lan8742_Object_t;

    /* Function Prototypes */
    int32_t LAN8742_RegisterBusIO(lan8742_Object_t *pObj,
                                  lan8742_IOCtx_t  *ioctx);
    int32_t LAN8742_Init(lan8742_Object_t *pObj);
    int32_t LAN8742_DeInit(lan8742_Object_t *pObj);
    int32_t LAN8742_DisablePowerDownMode(lan8742_Object_t *pObj);
    int32_t LAN8742_EnablePowerDownMode(lan8742_Object_t *pObj);
    int32_t LAN8742_StartAutoNego(lan8742_Object_t *pObj);
    int32_t LAN8742_GetLinkState(lan8742_Object_t *pObj);
    int32_t LAN8742_SetLinkState(lan8742_Object_t *pObj, uint32_t LinkState);
    int32_t LAN8742_EnableLoopbackMode(lan8742_Object_t *pObj);
    int32_t LAN8742_DisableLoopbackMode(lan8742_Object_t *pObj);
    int32_t LAN8742_EnableIT(lan8742_Object_t *pObj, uint32_t Interrupt);
    int32_t LAN8742_DisableIT(lan8742_Object_t *pObj, uint32_t Interrupt);
    int32_t LAN8742_ClearIT(lan8742_Object_t *pObj, uint32_t Interrupt);
    int32_t LAN8742_GetITStatus(lan8742_Object_t *pObj, uint32_t Interrupt);

#ifdef __cplusplus
}
#endif

#endif /* LAN8742_H */

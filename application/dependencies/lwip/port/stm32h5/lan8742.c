/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * LAN8742 PHY Driver
 */

#include "lan8742.h"

#include <stddef.h>

#define LAN8742_SW_RESET_TO  500U
#define LAN8742_INIT_TO      2000U
#define LAN8742_MAX_DEV_ADDR 31U

int32_t LAN8742_RegisterBusIO(lan8742_Object_t *pObj, lan8742_IOCtx_t *ioctx)
{
    if (pObj == NULL)
    {
        return LAN8742_STATUS_ERROR;
    }

    pObj->IO.Init     = ioctx->Init;
    pObj->IO.DeInit   = ioctx->DeInit;
    pObj->IO.WriteReg = ioctx->WriteReg;
    pObj->IO.ReadReg  = ioctx->ReadReg;
    pObj->IO.GetTick  = ioctx->GetTick;

    return LAN8742_STATUS_OK;
}

int32_t LAN8742_Init(lan8742_Object_t *pObj)
{
    uint32_t tickstart;
    uint32_t regvalue;
    uint32_t addr   = 0;
    int32_t  status = LAN8742_STATUS_OK;

    if (pObj->Is_Initialized == 0U)
    {
        if (pObj->IO.Init != NULL)
        {
            pObj->IO.Init();
        }

        /* Set default PHY address */
        pObj->DevAddr = LAN8742_PHY_ADDRESS;

        /* Read PHY ID to verify communication */
        if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_PHYI1R, &regvalue) < 0)
        {
            /* Try to find the PHY address */
            for (addr = 0; addr <= LAN8742_MAX_DEV_ADDR; addr++)
            {
                if (pObj->IO.ReadReg(addr, LAN8742_PHYI1R, &regvalue) >= 0)
                {
                    pObj->DevAddr = addr;
                    status        = LAN8742_STATUS_OK;
                    break;
                }
            }
            if (addr > LAN8742_MAX_DEV_ADDR)
            {
                return LAN8742_STATUS_ADDRESS_ERROR;
            }
        }

        /* Soft reset */
        if (pObj->IO.WriteReg(pObj->DevAddr, LAN8742_BCR,
                              LAN8742_BCR_SOFT_RESET) < 0)
        {
            return LAN8742_STATUS_WRITE_ERROR;
        }

        /* Wait for reset complete */
        tickstart = pObj->IO.GetTick();
        do
        {
            if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_BCR, &regvalue) < 0)
            {
                return LAN8742_STATUS_READ_ERROR;
            }

            if ((regvalue & LAN8742_BCR_SOFT_RESET) == 0U)
            {
                break;
            }
        } while ((pObj->IO.GetTick() - tickstart) <= LAN8742_SW_RESET_TO);

        if ((regvalue & LAN8742_BCR_SOFT_RESET) != 0U)
        {
            return LAN8742_STATUS_RESET_TIMEOUT;
        }

        /* Enable auto-negotiation */
        if (pObj->IO.WriteReg(
                pObj->DevAddr, LAN8742_BCR,
                LAN8742_BCR_AUTONEGO_EN | LAN8742_BCR_RESTART_AUTONEGO) < 0)
        {
            return LAN8742_STATUS_WRITE_ERROR;
        }

        pObj->Is_Initialized = 1U;
    }

    return status;
}

int32_t LAN8742_DeInit(lan8742_Object_t *pObj)
{
    if (pObj->Is_Initialized != 0U)
    {
        if (pObj->IO.DeInit != NULL)
        {
            pObj->IO.DeInit();
        }
        pObj->Is_Initialized = 0U;
    }

    return LAN8742_STATUS_OK;
}

int32_t LAN8742_DisablePowerDownMode(lan8742_Object_t *pObj)
{
    uint32_t regvalue;

    if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_BCR, &regvalue) < 0)
    {
        return LAN8742_STATUS_READ_ERROR;
    }

    regvalue &= ~LAN8742_BCR_POWER_DOWN;

    if (pObj->IO.WriteReg(pObj->DevAddr, LAN8742_BCR, regvalue) < 0)
    {
        return LAN8742_STATUS_WRITE_ERROR;
    }

    return LAN8742_STATUS_OK;
}

int32_t LAN8742_EnablePowerDownMode(lan8742_Object_t *pObj)
{
    uint32_t regvalue;

    if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_BCR, &regvalue) < 0)
    {
        return LAN8742_STATUS_READ_ERROR;
    }

    regvalue |= LAN8742_BCR_POWER_DOWN;

    if (pObj->IO.WriteReg(pObj->DevAddr, LAN8742_BCR, regvalue) < 0)
    {
        return LAN8742_STATUS_WRITE_ERROR;
    }

    return LAN8742_STATUS_OK;
}

int32_t LAN8742_StartAutoNego(lan8742_Object_t *pObj)
{
    uint32_t regvalue;

    if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_BCR, &regvalue) < 0)
    {
        return LAN8742_STATUS_READ_ERROR;
    }

    regvalue |= LAN8742_BCR_AUTONEGO_EN | LAN8742_BCR_RESTART_AUTONEGO;

    if (pObj->IO.WriteReg(pObj->DevAddr, LAN8742_BCR, regvalue) < 0)
    {
        return LAN8742_STATUS_WRITE_ERROR;
    }

    return LAN8742_STATUS_OK;
}

int32_t LAN8742_GetLinkState(lan8742_Object_t *pObj)
{
    uint32_t regvalue;

    /* Read BSR register */
    if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_BSR, &regvalue) < 0)
    {
        return LAN8742_STATUS_READ_ERROR;
    }

    /* Check link status */
    if ((regvalue & LAN8742_BSR_LINK_STATUS) == 0U)
    {
        /* Link is down - Read again to confirm (BSR bit is latching) */
        if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_BSR, &regvalue) < 0)
        {
            return LAN8742_STATUS_READ_ERROR;
        }

        if ((regvalue & LAN8742_BSR_LINK_STATUS) == 0U)
        {
            return LAN8742_STATUS_LINK_DOWN;
        }
    }

    /* Link is up - Check auto-negotiation */
    if ((regvalue & LAN8742_BSR_AUTONEGO_CPLT) == 0U)
    {
        return LAN8742_STATUS_AUTONEGO_NOTDONE;
    }

    /* Read PHYSCSR to determine speed and duplex */
    if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_PHYSCSR, &regvalue) < 0)
    {
        return LAN8742_STATUS_READ_ERROR;
    }

    if ((regvalue & LAN8742_PHYSCSR_AUTONEGO_DONE) == 0U)
    {
        return LAN8742_STATUS_AUTONEGO_NOTDONE;
    }

    switch (regvalue & LAN8742_PHYSCSR_HCDSPEEDMASK)
    {
        case LAN8742_PHYSCSR_100BTX_FD:
            return LAN8742_STATUS_100MBITS_FULLDUPLEX;

        case LAN8742_PHYSCSR_100BTX_HD:
            return LAN8742_STATUS_100MBITS_HALFDUPLEX;

        case LAN8742_PHYSCSR_10BT_FD:
            return LAN8742_STATUS_10MBITS_FULLDUPLEX;

        case LAN8742_PHYSCSR_10BT_HD:
            return LAN8742_STATUS_10MBITS_HALFDUPLEX;

        default:
            return LAN8742_STATUS_LINK_DOWN;
    }
}

int32_t LAN8742_SetLinkState(lan8742_Object_t *pObj, uint32_t LinkState)
{
    uint32_t regvalue;

    if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_BCR, &regvalue) < 0)
    {
        return LAN8742_STATUS_READ_ERROR;
    }

    /* Disable auto-negotiation */
    regvalue &= ~(LAN8742_BCR_AUTONEGO_EN | LAN8742_BCR_SPEED_SELECT |
                  LAN8742_BCR_DUPLEX_MODE);

    switch (LinkState)
    {
        case LAN8742_STATUS_100MBITS_FULLDUPLEX:
            regvalue |= LAN8742_BCR_SPEED_SELECT | LAN8742_BCR_DUPLEX_MODE;
            break;

        case LAN8742_STATUS_100MBITS_HALFDUPLEX:
            regvalue |= LAN8742_BCR_SPEED_SELECT;
            break;

        case LAN8742_STATUS_10MBITS_FULLDUPLEX:
            regvalue |= LAN8742_BCR_DUPLEX_MODE;
            break;

        case LAN8742_STATUS_10MBITS_HALFDUPLEX:
            /* All bits already cleared */
            break;

        default:
            return LAN8742_STATUS_ERROR;
    }

    if (pObj->IO.WriteReg(pObj->DevAddr, LAN8742_BCR, regvalue) < 0)
    {
        return LAN8742_STATUS_WRITE_ERROR;
    }

    return LAN8742_STATUS_OK;
}

int32_t LAN8742_EnableLoopbackMode(lan8742_Object_t *pObj)
{
    uint32_t regvalue;

    if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_BCR, &regvalue) < 0)
    {
        return LAN8742_STATUS_READ_ERROR;
    }

    regvalue |= LAN8742_BCR_LOOPBACK;

    if (pObj->IO.WriteReg(pObj->DevAddr, LAN8742_BCR, regvalue) < 0)
    {
        return LAN8742_STATUS_WRITE_ERROR;
    }

    return LAN8742_STATUS_OK;
}

int32_t LAN8742_DisableLoopbackMode(lan8742_Object_t *pObj)
{
    uint32_t regvalue;

    if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_BCR, &regvalue) < 0)
    {
        return LAN8742_STATUS_READ_ERROR;
    }

    regvalue &= ~LAN8742_BCR_LOOPBACK;

    if (pObj->IO.WriteReg(pObj->DevAddr, LAN8742_BCR, regvalue) < 0)
    {
        return LAN8742_STATUS_WRITE_ERROR;
    }

    return LAN8742_STATUS_OK;
}

int32_t LAN8742_EnableIT(lan8742_Object_t *pObj, uint32_t Interrupt)
{
    uint32_t regvalue;

    if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_IMR, &regvalue) < 0)
    {
        return LAN8742_STATUS_READ_ERROR;
    }

    regvalue |= Interrupt;

    if (pObj->IO.WriteReg(pObj->DevAddr, LAN8742_IMR, regvalue) < 0)
    {
        return LAN8742_STATUS_WRITE_ERROR;
    }

    return LAN8742_STATUS_OK;
}

int32_t LAN8742_DisableIT(lan8742_Object_t *pObj, uint32_t Interrupt)
{
    uint32_t regvalue;

    if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_IMR, &regvalue) < 0)
    {
        return LAN8742_STATUS_READ_ERROR;
    }

    regvalue &= ~Interrupt;

    if (pObj->IO.WriteReg(pObj->DevAddr, LAN8742_IMR, regvalue) < 0)
    {
        return LAN8742_STATUS_WRITE_ERROR;
    }

    return LAN8742_STATUS_OK;
}

int32_t LAN8742_ClearIT(lan8742_Object_t *pObj, uint32_t Interrupt)
{
    (void)Interrupt;
    uint32_t regvalue;

    /* ISFR is clear-on-read */
    if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_ISFR, &regvalue) < 0)
    {
        return LAN8742_STATUS_READ_ERROR;
    }

    return LAN8742_STATUS_OK;
}

int32_t LAN8742_GetITStatus(lan8742_Object_t *pObj, uint32_t Interrupt)
{
    uint32_t regvalue;
    int32_t  status = LAN8742_STATUS_OK;

    if (pObj->IO.ReadReg(pObj->DevAddr, LAN8742_ISFR, &regvalue) < 0)
    {
        return LAN8742_STATUS_READ_ERROR;
    }

    if ((regvalue & Interrupt) != Interrupt)
    {
        status = LAN8742_STATUS_ERROR;
    }

    return status;
}

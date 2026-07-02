/**
 * @file digital_output.c
 * @brief Digital Output (DO) module implementation.
 *
 * This module provides a high-level API for controlling the 16 digital
 * outputs connected via I2C expanders (PCF8574/PCF8574A). It maintains
 * an internal shadow register for efficiency and consistency, and handles
 * updating the LCD display status of each channel.
 *
 * @copyright Copyright (c) 2026
 */

#include "digital_output.h"

#include <stdio.h>

#include "bsp.h"
#include "lcd_manager.h"
#include "log.h"

/* ==========================================================================
 * Private Variable Declarations
 * ========================================================================== */

/**
 * @brief Internal shadow register for the 16 digital output states.
 *
 * This variable stores the last successfully written 16-bit value to the
 * I2C digital output expanders. It serves as a cache to avoid redundant
 * I2C reads and to track changes for LCD updates.
 */
static uint16_t g_shadow = 0x0000U;

/* ==========================================================================
 * Public Function Implementations
 * ========================================================================== */

bsp_error_t DigitalOutput_Init(void)
{
    bsp_error_t err = BSP_OK;

    /* Write 0x0000 to hardware to ensure all outputs are off */
    err = BSP_I2CDO_Write(0x0000U);

    if (BSP_OK == err)
    {
        g_shadow = 0x0000U; /* Initialize shadow register to match hardware */

        LOG_INF("[DO] Module initialized; all outputs OFF");
    }
    else
    {
        LOG_INF("[DO] Module initialization failed: I2C write error %d",
                (int)err);
    }

    return err;
}

void DigitalOutput_SyncLcd(void)
{
    for (uint16_t i = 0U; i < DIGITAL_OUTPUT_NUM_CHANNELS; i++)
    {
        bool val = ((g_shadow >> i) & 0x01U) != 0U;
        LcdManager_UpdateDigitalOutputStatus(i, val);
    }
    LOG_INF("[DO] LCD synced to shadow register 0x%04X", g_shadow);
}

/* ==========================================================================
 * Public Function Implementations
 * ========================================================================== */

bsp_error_t DigitalOutput_SetChannel(uint16_t channel, bool value)
{
    if (channel >= DIGITAL_OUTPUT_NUM_CHANNELS)
    {
        return BSP_INVALID_ARG;
    }

    bsp_error_t err       = BSP_OK;
    uint16_t    oldShadow = g_shadow;
    uint16_t    newShadow = g_shadow;
    uint16_t    mask      = BSP_I2CDO_CONSTRUCT_MASK(channel);

    if (value == true)
    {
        newShadow = newShadow | mask;
    }
    else
    {
        newShadow = newShadow & (~mask);
    }

    if (newShadow != oldShadow)
    {
        err = BSP_I2CDO_Write(newShadow);

        if (BSP_OK == err)
        {
            g_shadow = newShadow;
            LcdManager_UpdateDigitalOutputStatus(channel, value);
        }
        else
        {
        }
    }
    else
    {
        /* No change needed, state already matches */
    }

    return err;
}

bsp_error_t DigitalOutput_GetChannel(uint16_t channel, bool *value)
{
    if (channel >= DIGITAL_OUTPUT_NUM_CHANNELS || value == NULL)
    {
        return BSP_INVALID_ARG;
    }

    uint16_t mask = BSP_I2CDO_CONSTRUCT_MASK(channel);

    *value = ((g_shadow & mask) != 0U);

    return BSP_OK;
}

bsp_error_t DigitalOutput_ReadHardware(uint16_t channel, bool *value)
{
    if (channel >= DIGITAL_OUTPUT_NUM_CHANNELS || value == NULL)
    {
        return BSP_INVALID_ARG;
    }

    bsp_error_t err           = BSP_OK;
    uint16_t    hardwareState = 0U;
    uint16_t    mask          = BSP_I2CDO_CONSTRUCT_MASK(channel);

    err = BSP_I2CDO_Read(&hardwareState);

    if (BSP_OK == err)
    {
        *value = ((hardwareState & mask) != 0U);
    }
    else
    {
        LOG_ERR("[DO] ReadHardware channel %d: FAILED (I2C error %d)", channel,
                (int)err);
    }

    return err;
}

bsp_error_t DigitalOutput_WriteAll(uint16_t mask)
{
    bsp_error_t err       = BSP_OK;
    uint16_t    oldShadow = g_shadow;
    uint16_t    newShadow = mask;

    if (newShadow != oldShadow)
    {
        err = BSP_I2CDO_Write(newShadow);

        if (BSP_OK == err)
        {
            g_shadow = newShadow;

            /* Update LCD for all channels that changed */
            for (uint16_t i = 0U; i < DIGITAL_OUTPUT_NUM_CHANNELS; i++)
            {
                bool oldVal = ((oldShadow >> i) & 0x01U) != 0U;
                bool newVal = ((newShadow >> i) & 0x01U) != 0U;
                if (oldVal != newVal)
                {
                    LcdManager_UpdateDigitalOutputStatus(i, newVal);
                }
            }
            LOG_INF("[DO] WriteAll to 0x%04X: OK", newShadow);
        }
        else
        {
            LOG_ERR("[DO] WriteAll to 0x%04X: FAILED (I2C error %d)", newShadow,
                    (int)err);
        }
    }
    else
    {
        LOG_INF("[DO] WriteAll to 0x%04X: No change needed", newShadow);
    }

    return err;
}

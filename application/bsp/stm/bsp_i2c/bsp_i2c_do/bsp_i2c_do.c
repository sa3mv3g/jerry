#include "bsp.h"
#include "bsp_i2c.h"
#include "log.h"

/* ========================================================================== */
/*                 Private Definitions and Macros                             */
/* ========================================================================== */
#define RETRY_COUNTS (3U)

/* ========================================================================== */
/*                 Private Typedefs                                           */
/* ========================================================================== */

/* ========================================================================== */
/*                 Private Function Prototype                                 */
/* ========================================================================== */
static inline bsp_error_t BSP_I2C_MapStatus(HAL_StatusTypeDef status);

/* ========================================================================== */
/*                 Private Variable Declaration                               */
/* ========================================================================== */
extern I2C_HandleTypeDef      hi2c4;
static BSP_I2C_Target_State_t gPCF8574ConnState;
static BSP_I2C_Target_State_t gPCF8574AConnState;

/* ========================================================================== */
/*                 Public Functions                                           */
/* ========================================================================== */

bsp_error_t BSP_I2CDO_init(void)
{
    bsp_error_t ret     = BSP_OK;
    bool        pcfcon  = false;
    bool        pcfacon = false;

    BSP_I2C_Target_Init(&gPCF8574ConnState);
    BSP_I2C_Target_Init(&gPCF8574AConnState);

    if (HAL_I2C_IsDeviceReady(&hi2c4, BSP_I2CDO_PCF8574_ADDR, RETRY_COUNTS,
                              BSP_I2CDO_TIMEOUT) == HAL_OK)
    {
        BSP_I2C_Target_Connect(&gPCF8574ConnState);
        pcfcon = true;

        if (HAL_I2C_IsDeviceReady(&hi2c4, BSP_I2CDO_PCF8574A_ADDR, RETRY_COUNTS,
                                  BSP_I2CDO_TIMEOUT) == HAL_OK)
        {
            BSP_I2C_Target_Connect(&gPCF8574AConnState);
            pcfacon = true;
        }
        else
        {
            BSP_I2C_Target_Disconnect(&gPCF8574AConnState);
            pcfacon = false;
        }
    }
    else
    {
        BSP_I2C_Target_Disconnect(&gPCF8574ConnState);
        pcfcon = false;
    }

    if ((true == pcfcon) && (true == pcfacon))
    {
        ret = BSP_I2CDO_Write(0x0000U);
    }
    else
    {
        ret = BSP_ERROR;
    }

    return ret;
}

bsp_error_t BSP_I2CDO_Write(uint16_t value)
{
    bsp_error_t ret = BSP_OK;

    if (BSP_I2C_Controller_MutexLock() != pdTRUE)
    {
        LOG_ERR("[BSP_I2CDO_Write] Failed to acquire I2C4 mutex");
        ret = BSP_BUSY;
    }
    else
    {
        uint32_t pcfState;
        uint32_t pcfaState;

        /* check if devices are connected */
        BSP_I2C_Target_GetState(&gPCF8574ConnState, &pcfState);
        BSP_I2C_Target_GetState(&gPCF8574AConnState, &pcfaState);

        if ((BSP_I2C_TARGET_STATE_CONN == pcfState) &&
            (BSP_I2C_TARGET_STATE_CONN == pcfaState))
        {
            HAL_StatusTypeDef status;
            uint8_t           output_byte;

            // Write lower 8 bits to PCF8574
            output_byte = (uint8_t)(value & 0xFFU);
            status =
                HAL_I2C_Master_Transmit(&hi2c4, BSP_I2CDO_PCF8574_ADDR,
                                        &output_byte, 1, BSP_I2CDO_TIMEOUT);
            ret = BSP_I2C_MapStatus(status);

            // Write upper 8 bits to PCF8574A
            if (ret == BSP_OK)
            {
                output_byte = (uint8_t)((value >> 8U) & 0xFFU);
                status =
                    HAL_I2C_Master_Transmit(&hi2c4, BSP_I2CDO_PCF8574A_ADDR,
                                            &output_byte, 1, BSP_I2CDO_TIMEOUT);
                ret = BSP_I2C_MapStatus(status);
                if (ret != BSP_OK)
                {
                    BSP_I2C_Target_Disconnect(&gPCF8574AConnState);
                }
            }
            else
            {
                BSP_I2C_Target_Disconnect(&gPCF8574ConnState);
            }
        }
        else
        {
            ret = BSP_ERROR;
        }

        BSP_I2C_Controller_MutexUnlock();
    }

    return ret;
}

bsp_error_t BSP_I2CDO_Read(uint16_t *value)
{
    bsp_error_t ret = BSP_OK;

    if (value == NULL)
    {
        ret = BSP_INVALID_ARG;
    }
    else if (BSP_I2C_Controller_MutexLock() != pdTRUE)
    {
        LOG_ERR("[BSP_I2CDO_Read] Failed to acquire I2C4 mutex");
        ret = BSP_BUSY;
    }
    else
    {
        uint32_t pcfState  = 0;
        uint32_t pcfaState = 0;

        BSP_I2C_Target_GetState(&gPCF8574ConnState, &pcfState);
        BSP_I2C_Target_GetState(&gPCF8574AConnState, &pcfaState);

        if ((BSP_I2C_TARGET_STATE_CONN == pcfState) &&
            (BSP_I2C_TARGET_STATE_CONN == pcfaState))
        {
            uint8_t read_byte_pcf8574 = 0;
            /* 1. Read from PCF8574 (lower 8 bits) */
            ret = BSP_I2C_MapStatus(HAL_I2C_Master_Receive(
                &hi2c4, BSP_I2CDO_PCF8574_ADDR, &read_byte_pcf8574, 1,
                BSP_I2CDO_TIMEOUT));

            if (ret != BSP_OK)
            {
                BSP_I2C_Target_Disconnect(&gPCF8574ConnState);
            }
            else
            {
                uint8_t read_byte_pcf8574a = 0;
                /* 2. Read from PCF8574A (upper 8 bits) */
                ret = BSP_I2C_MapStatus(HAL_I2C_Master_Receive(
                    &hi2c4, BSP_I2CDO_PCF8574A_ADDR, &read_byte_pcf8574a, 1,
                    BSP_I2CDO_TIMEOUT));

                if (ret != BSP_OK)
                {
                    BSP_I2C_Target_Disconnect(&gPCF8574AConnState);
                }
                else
                {
                    /* Both reads successful, update user pointer */
                    *value = (uint16_t)read_byte_pcf8574 |
                             ((uint16_t)read_byte_pcf8574a << 8U);
                }
            }
        }
        else
        {
            ret = BSP_ERROR;
        }

        BSP_I2C_Controller_MutexUnlock();
    }

    return ret;
}

/* ========================================================================== */
/*                 Private Functions                                          */
/* ========================================================================== */
/**
 * @brief Helper to map HAL I2C status to BSP error codes.
 */
static inline bsp_error_t BSP_I2C_MapStatus(HAL_StatusTypeDef status)
{
    bsp_error_t ret = BSP_OK;

    if (status == HAL_TIMEOUT)
    {
        ret = BSP_TIMEOUT;
    }
    else if (status != HAL_OK)
    {
        ret = BSP_ERROR;
    }

    return ret;
}

/* ========================================================================== */
/*                 Private Callback Handlers                                  */
/* ========================================================================== */

/* ========================================================================== */
/*                 Test/Debug/Other Sections                                  */
/* ========================================================================== */
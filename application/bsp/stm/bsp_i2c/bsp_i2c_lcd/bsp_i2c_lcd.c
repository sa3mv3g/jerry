#include "bsp.h"
#include "bsp_i2c.h"
#include "stm32h5xx_hal_i2c.h"

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
static BSP_I2C_Target_State_t gLcdConnState;

/* ========================================================================== */
/*                 Public Functions                                           */
/* ========================================================================== */
bsp_error_t BSP_I2C_LcdInit()
{
    BSP_I2C_Target_Init(&gLcdConnState);

    if (HAL_I2C_IsDeviceReady(&hi2c4, BSP_I2CLCD_ADDRESS, RETRY_COUNTS,
                              BSP_I2CLCD_TIMEOUT) == HAL_OK)
    {
        BSP_I2C_Target_Connect(&gLcdConnState);
    }
    else
    {
        BSP_I2C_Target_Disconnect(&gLcdConnState);
    }

    return BSP_OK;
}

bsp_error_t BSP_I2C_LcdRead(uint8_t address, uint8_t *buff, uint16_t len,
                            uint32_t timeout)
{
    bsp_error_t ret = BSP_OK;

    if (NULL == buff)
    {
        ret = BSP_INVALID_ARG;
    }
    else if (BSP_I2C_Controller_MutexLock() != pdTRUE)
    {
        ret = BSP_BUSY;
    }
    else
    {
        uint32_t lcdStatus = 0;

        BSP_I2C_Target_GetState(&gLcdConnState, &lcdStatus);

        if (BSP_I2C_TARGET_STATE_CONN == lcdStatus)
        {
            /* Perform transaction */
            ret = BSP_I2C_MapStatus(
                HAL_I2C_Master_Receive(&hi2c4, address, buff, len, timeout));

            /* Update state machine based on transaction result */
            if (ret != BSP_OK)
            {
                BSP_I2C_Target_Disconnect(&gLcdConnState);
            }
            else
            {
                /* If successful, ensure state is marked as connected */
                BSP_I2C_Target_Connect(&gLcdConnState);
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

bsp_error_t BSP_I2C_LcdWrite(uint8_t address, uint8_t *buff, uint16_t len,
                             uint32_t timeout)
{
    (void)timeout;
    bsp_error_t ret = BSP_OK;

    if (NULL == buff)
    {
        ret = BSP_INVALID_ARG;
    }
    else if (BSP_I2C_Controller_MutexLock() != pdTRUE)
    {
        ret = BSP_BUSY;
    }
    else
    {
        uint32_t lcdStatus = 0;

        BSP_I2C_Target_GetState(&gLcdConnState, &lcdStatus);

        if (BSP_I2C_TARGET_STATE_CONN == lcdStatus)
        {
            ret = BSP_I2C_MapStatus(
                I2C_WriteData_Async(&hi2c4, address, buff, len));

            if (ret != BSP_OK)
            {
                BSP_I2C_Target_Disconnect(&gLcdConnState);
            }
            else
            {
                BSP_I2C_Target_Connect(&gLcdConnState);
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

static inline bsp_error_t BSP_I2C_MapStatus(HAL_StatusTypeDef status)
{
    bsp_error_t ret;

    switch (status)
    {
        case HAL_OK:
            ret = BSP_OK;
            break;
        case HAL_BUSY:
            ret = BSP_BUSY;
            break;
        case HAL_TIMEOUT:
            ret = BSP_TIMEOUT;
            break;
        case HAL_ERROR:
        default:
            ret = BSP_ERROR;
            break;
    }
    return ret;
}

/* ========================================================================== */
/*                 Private Callback Handlers                                  */
/* ========================================================================== */

/* ========================================================================== */
/*                 Test/Debug/Other Sections                                  */
/* ========================================================================== */
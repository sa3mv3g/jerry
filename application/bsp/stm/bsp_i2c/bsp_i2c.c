#include "bsp_i2c.h"

#include <stddef.h>
#include <string.h>

#include "bsp.h"
#include "main.h"

/* ========================================================================== */
/*                 Private Definitions and Macros                             */
/* ========================================================================== */
#define I2C_TIMEOUT (100U)

/* ========================================================================== */
/*                 Private Typedefs                                           */
/* ========================================================================== */

/* ========================================================================== */
/*                 Private Function Prototype                                 */
/* ========================================================================== */

/* ========================================================================== */
/*                 Private Variable Declaration                               */
/* ========================================================================== */
static BSP_I2C_Target_State_t gI2C4BusStatus;
static volatile bool          g_i2c4_tx_complete = false;

/* ========================================================================== */
/*                 Public Functions                                           */
/* ========================================================================== */

void BSP_I2C_Controller_MutexInit(void)
{
    /* Bare-metal: no mutex needed or implemented via basic flags if required */
}

BaseType_t BSP_I2C_Controller_MutexLock(void)
{
    /* Bare-metal: return success immediately or implement spinlock */
    return 1;  // pdTRUE equivalent
}

void BSP_I2C_Controller_MutexUnlock(void) { /* Bare-metal: no action */ }

bsp_error_t BSP_I2C_Target_ParamsInit(BSP_I2C_Target_State_t *statePtr)
{
    bsp_error_t ret = BSP_OK;

    if (statePtr == NULL)
    {
        ret = BSP_ERROR;
    }
    else
    {
        memset(statePtr, 0, sizeof(BSP_I2C_Target_State_t));
    }

    return ret;
}

bsp_error_t BSP_I2C_Target_Init(BSP_I2C_Target_State_t *statePtr)
{
    bsp_error_t ret = BSP_OK;

    if (statePtr == NULL)
    {
        ret = BSP_ERROR;
    }
    else
    {
        statePtr->presentState = BSP_I2C_TARGET_STATE_INIT;
    }

    return ret;
}

bsp_error_t BSP_I2C_Target_Connect(BSP_I2C_Target_State_t *statePtr)
{
    bsp_error_t ret = BSP_OK;

    if (statePtr == NULL)
    {
        ret = BSP_ERROR;
    }
    else
    {
        statePtr->presentState = BSP_I2C_TARGET_STATE_CONN;
    }

    return ret;
}

bsp_error_t BSP_I2C_Target_Disconnect(BSP_I2C_Target_State_t *statePtr)
{
    bsp_error_t ret = BSP_OK;

    if (statePtr == NULL)
    {
        ret = BSP_ERROR;
    }
    else
    {
        statePtr->presentState = BSP_I2C_TARGET_STATE_DISCON;
    }

    return ret;
}

bsp_error_t BSP_I2C_Target_GetState(BSP_I2C_Target_State_t *statePtr,
                                    uint32_t               *valuePtr)
{
    bsp_error_t ret = BSP_OK;

    if ((statePtr == NULL) || (valuePtr == NULL))
    {
        ret = BSP_ERROR;
    }
    else
    {
        *valuePtr = statePtr->presentState;
    }

    return ret;
}

bsp_error_t BSP_I2C_Init(void)
{
    bsp_error_t   ret;
    GPIO_PinState sclState;
    GPIO_PinState sdaState;

    BSP_I2C_Target_Init(&gI2C4BusStatus);

    /* Manually check if SDA/SCL are HIGH (idle)
       If they are LOW, the bus is grounded or damaged.
    */
    sclState = HAL_GPIO_ReadPin(I2C4_SCL_GPIO_Port, I2C4_SCL_Pin);
    sdaState = HAL_GPIO_ReadPin(I2C4_SDA_GPIO_Port, I2C4_SDA_Pin);
    if ((sclState == GPIO_PIN_RESET) || (sdaState == GPIO_PIN_RESET))
    {
        BSP_I2C_Target_Disconnect(&gI2C4BusStatus);
        ret = BSP_ERROR;  // Bus is pulled low (shorted)
    }
    else
    {
        BSP_I2C_Target_Connect(&gI2C4BusStatus);
        BSP_I2C_Controller_MutexInit();
        ret = BSP_I2CDO_init();
        if (ret == BSP_OK)
        {
            ret = BSP_I2C_LcdInit();
        }
    }

    return ret;
}

uint32_t BSP_I2C_GetBusStatus(void)
{
    uint32_t ret = 0;
    BSP_I2C_Target_GetState(&gI2C4BusStatus, &ret);
    return ret;
}

HAL_StatusTypeDef I2C_WriteData_Async(I2C_HandleTypeDef *hi2c,
                                      uint16_t DevAddress, uint8_t *pData,
                                      uint16_t Size)
{
    HAL_StatusTypeDef status;
    uint32_t          tickstart;

    g_i2c4_tx_complete = false;

    // Start the interrupt-driven hardware transmission
    status = HAL_I2C_Master_Transmit_IT(hi2c, DevAddress, pData, Size);

    if (status == HAL_OK)
    {
        tickstart = HAL_GetTick();

        // Bare-metal busy-wait loop waiting for interrupt to complete
        while (!g_i2c4_tx_complete)
        {
            if ((HAL_GetTick() - tickstart) > I2C_TIMEOUT)
            {
                // Timeout hit - hardware might be hung.
                return HAL_TIMEOUT;
            }
        }

        return HAL_OK;
    }

    return status;
}

// 3. The interrupt callback
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C4)
    {
        g_i2c4_tx_complete = true;
    }
}

// Catch I2C errors (NACK, Bus Error)
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C4)
    {
        g_i2c4_tx_complete = true;
    }
}
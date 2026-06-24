#include "bsp_i2c.h"

#include <stddef.h>
#include <string.h>

#include "bsp.h"
#include "main.h"
#include "portmacrocommon.h"

/* ========================================================================== */
/*                 Private Definitions and Macros                             */
/* ========================================================================== */
#define SEMPHR_TIMEOUT (100U)

/* ========================================================================== */
/*                 Private Typedefs                                           */
/* ========================================================================== */

/* ========================================================================== */
/*                 Private Function Prototype                                 */
/* ========================================================================== */

/* ========================================================================== */
/*                 Private Variable Declaration                               */
/* ========================================================================== */
static SemaphoreHandle_t gI2C4Mutex = NULL;
static StaticSemaphore_t gI2C4MutexBuf;

/* ========================================================================== */
/*                 Public Functions                                           */
/* ========================================================================== */

void BSP_I2C_Controller_MutexInit(void)
{
    /* Create the I2C4 bus mutex (static allocation — safe before scheduler) */
    gI2C4Mutex = xSemaphoreCreateMutexStatic(&gI2C4MutexBuf);
}

BaseType_t BSP_I2C_Controller_MutexLock(void)
{
    return xSemaphoreTake(gI2C4Mutex, pdMS_TO_TICKS(SEMPHR_TIMEOUT));
}

void BSP_I2C_Controller_MutexUnlock(void) { xSemaphoreGive(gI2C4Mutex); }

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

bsp_error_t BSP_I2C_Init()
{
    bsp_error_t   ret;
    GPIO_PinState sclState;
    GPIO_PinState sdaState;

    /* Manually check if SDA/SCL are HIGH (idle)
       If they are LOW, the bus is grounded or damaged.
    */
    sclState = HAL_GPIO_ReadPin(I2C4_SCL_GPIO_Port, I2C4_SCL_Pin);
    sdaState = HAL_GPIO_ReadPin(I2C4_SDA_GPIO_Port, I2C4_SDA_Pin);
    if ((sclState == GPIO_PIN_RESET) || (sdaState == GPIO_PIN_RESET))
    {
        ret = BSP_ERROR;  // Bus is pulled low (shorted)
    }
    else
    {
        BSP_I2C_Controller_MutexInit();
        ret = BSP_I2CDO_init();
        if (ret == BSP_OK)
        {
            ret = BSP_I2C_LcdInit();
        }
    }

    return ret;
}
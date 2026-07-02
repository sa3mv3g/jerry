#include "bsp_i2c.h"

#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "bsp.h"
#include "main.h"
#include "portmacrocommon.h"
#include "semphr.h"

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
static SemaphoreHandle_t      gI2C4Mutex = NULL;
static StaticSemaphore_t      gI2C4MutexBuf;
static BSP_I2C_Target_State_t gI2C4BusStatus;
static SemaphoreHandle_t      xI2cTxSemaphore = NULL;
static StaticSemaphore_t      xI2cTxSemaphoreBuffer;

/* ========================================================================== */
/*                 Public Functions                                           */
/* ========================================================================== */

void BSP_I2C_Controller_MutexInit(void)
{
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

bsp_error_t BSP_I2C_Init(void)
{
    bsp_error_t   ret;
    GPIO_PinState sclState;
    GPIO_PinState sdaState;

    xI2cTxSemaphore = xSemaphoreCreateBinaryStatic(&xI2cTxSemaphoreBuffer);

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

    status = HAL_I2C_Master_Transmit_IT(hi2c, DevAddress, pData, Size);

    if (status == HAL_OK)
    {
        if (xSemaphoreTake(xI2cTxSemaphore, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            status = HAL_OK;
        }
        else
        {
            status = HAL_TIMEOUT;
        }
    }

    return status;
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hi2c->Instance == I2C4)
    {
        xSemaphoreGiveFromISR(xI2cTxSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (hi2c->Instance == I2C4)
    {
        xSemaphoreGiveFromISR(xI2cTxSemaphore, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
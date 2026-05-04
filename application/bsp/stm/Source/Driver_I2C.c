/*
 * Copyright (c) 2026
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Driver_I2C.h"

#include "stm32h5xx_hal.h"

#define I2C_NUM 4U /* STM32H5 has I2C1, I2C2, I2C3, I2C4 */

/* I2C Driver state */
static ARM_DRIVER_STATE DriverState = ARM_DRIVER_STATE_UNINITIALIZED;

/* I2C handle pointers */
static I2C_HandleTypeDef *i2c_handles[I2C_NUM] = {NULL};

/* I2C clock enable functions */
static void (*i2c_clk_enable[I2C_NUM])(void) = {
    __HAL_RCC_I2C1_CLK_ENABLE, __HAL_RCC_I2C2_CLK_ENABLE,
    __HAL_RCC_I2C3_CLK_ENABLE, __HAL_RCC_I2C4_CLK_ENABLE};

/* I2C clock disable functions */
static void (*i2c_clk_disable[I2C_NUM])(void) = {
    __HAL_RCC_I2C1_CLK_DISABLE, __HAL_RCC_I2C2_CLK_DISABLE,
    __HAL_RCC_I2C3_CLK_DISABLE, __HAL_RCC_I2C4_CLK_DISABLE};

/* I2C IRQ numbers */
static const IRQn_Type i2c_irq[I2C_NUM] = {I2C1_EV_IRQn, I2C2_EV_IRQn,
                                           I2C3_EV_IRQn, I2C4_EV_IRQn};

/* I2C peripheral base addresses */
static I2C_TypeDef *const i2c_reg[I2C_NUM] = {I2C1, I2C2, I2C3, I2C4};

/**
  \fn          ARM_DRIVER_VERSION I2C_GetVersion (void)
  \brief       Get driver version.
  \return      \ref ARM_DRIVER_VERSION
*/
static ARM_DRIVER_VERSION I2C_GetVersion(void)
{
    static const ARM_DRIVER_VERSION version = {
        .api = ARM_DRIVER_VERSION_MAJOR_MINOR(2, 0), /* API version */
        .drv = ARM_DRIVER_VERSION_MAJOR_MINOR(1, 0)  /* Driver version */
    };
    return version;
}

/**
  \fn          ARM_I2C_CAPABILITIES I2C_GetCapabilities (void)
  \brief       Get driver capabilities.
  \return      \ref ARM_I2C_CAPABILITIES
*/
static ARM_I2C_CAPABILITIES I2C_GetCapabilities(void)
{
    static const ARM_I2C_CAPABILITIES capabilities = {
        .address_10_bit = 1, /* Supports 10-bit addressing */
        .slave          = 0, /* Slave mode available */
        .callbacks      = 0  /* Callbacks available */
    };
    return capabilities;
}

/**
  \fn          int32_t I2C_Initialize (ARM_I2C_SignalEvent_t cb_event)
  \brief       Initialize I2C Interface.
  \param[in]   cb_event  Pointer to \ref ARM_I2C_SignalEvent
  \return      \ref execution_status
*/
static int32_t I2C_Initialize(ARM_I2C_SignalEvent_t cb_event)
{
    if (DriverState == ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_OK;
    }

    DriverState = ARM_DRIVER_STATE_READY;
    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t I2C_Uninitialize (void)
  \brief       De-initialize I2C Interface.
  \return      \ref execution_status
*/
static int32_t I2C_Uninitialize(void)
{
    uint32_t i;

    /* Disable all I2C peripherals */
    for (i = 0; i < I2C_NUM; i++)
    {
        if (i2c_handles[i] != NULL)
        {
            HAL_I2C_DeInit(i2c_handles[i]);
            i2c_handles[i] = NULL;
        }
    }

    DriverState = ARM_DRIVER_STATE_UNINITIALIZED;
    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t I2C_PowerControl (ARM_POWER_STATE state)
  \brief       Control I2C Interface Power.
  \param[in]   state  Power state
  \return      \ref execution_status
*/
static int32_t I2C_PowerControl(ARM_POWER_STATE state)
{
    switch (state)
    {
        case ARM_POWER_OFF:
            /* Disable all I2C peripherals */
            I2C_Uninitialize();
            DriverState = ARM_DRIVER_STATE_OFF;
            break;

        case ARM_POWER_LOW:
            return ARM_DRIVER_ERROR_UNSUPPORTED;

        case ARM_POWER_FULL:
            DriverState = ARM_DRIVER_STATE_READY;
            break;

        default:
            return ARM_DRIVER_ERROR_PARAMETER;
    }
    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t I2C_MasterTransmit (uint32_t addr, const void *data,
  uint32_t num, bool xfer_pending)
  \brief       Start transmitting data as I2C Master.
  \param[in]   addr          Slave address (7-bit or 10-bit)
  \param[in]   data          Pointer to buffer with data to transmit
  \param[in]   num           Number of data bytes to transmit
  \param[in]   xfer_pending  Transfer operation is pending - Stop condition will
  not be generated
  \return      \ref execution_status
*/
static int32_t I2C_MasterTransmit(uint32_t addr, const void *data, uint32_t num,
                                  bool xfer_pending)
{
    /* For simplicity, we'll use I2C1 as default */
    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    if (i2c_handles[0] == NULL)
    {
        return ARM_DRIVER_ERROR;
    }

    uint32_t flags = xfer_pending ? I2C_FIRST_AND_NEXT_FRAME : I2C_FIRST_FRAME;

    if (HAL_I2C_Master_Seq_Transmit(i2c_handles[0], (uint16_t)addr,
                                    (uint8_t *)data, num, flags) != HAL_OK)
    {
        return ARM_DRIVER_ERROR;
    }

    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t I2C_MasterReceive (uint32_t addr, void *data, uint32_t
  num, bool xfer_pending)
  \brief       Start receiving data as I2C Master.
  \param[in]   addr          Slave address (7-bit or 10-bit)
  \param[out]  data          Pointer to buffer for data to receive
  \param[in]   num           Number of data bytes to receive
  \param[in]   xfer_pending  Transfer operation is pending - Stop condition will
  not be generated
  \return      \ref execution_status
*/
static int32_t I2C_MasterReceive(uint32_t addr, void *data, uint32_t num,
                                 bool xfer_pending)
{
    /* For simplicity, we'll use I2C1 as default */
    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    if (i2c_handles[0] == NULL)
    {
        return ARM_DRIVER_ERROR;
    }

    uint32_t flags = xfer_pending ? I2C_FIRST_AND_NEXT_FRAME : I2C_FIRST_FRAME;

    if (HAL_I2C_Master_Seq_Receive(i2c_handles[0], (uint16_t)addr,
                                   (uint8_t *)data, num, flags) != HAL_OK)
    {
        return ARM_DRIVER_ERROR;
    }

    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t I2C_SlaveTransmit (const void *data, uint32_t num)
  \brief       Start transmitting data as I2C Slave.
  \param[in]   data  Pointer to buffer with data to transmit
  \param[in]   num   Number of data bytes to transmit
  \return      \ref execution_status
*/
static int32_t I2C_SlaveTransmit(const void *data, uint32_t num)
{
    /* Slave mode not implemented in this basic version */
    (void)data;
    (void)num;
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

/**
  \fn          int32_t I2C_SlaveReceive (void *data, uint32_t num)
  \brief       Start receiving data as I2C Slave.
  \param[out]  data  Pointer to buffer for data to receive
  \param[in]   num   Number of data bytes to receive
  \return      \ref execution_status
*/
static int32_t I2C_SlaveReceive(void *data, uint32_t num)
{
    /* Slave mode not implemented in this basic version */
    (void)data;
    (void)num;
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

/**
  \fn          int32_t I2C_GetDataCount (void)
  \brief       Get transferred data count.
  \return      number of data bytes transferred
*/
static int32_t I2C_GetDataCount(void)
{
    /* For simplicity, we'll use I2C1 as default */
    if (i2c_handles[0] == NULL)
    {
        return 0;
    }

    return i2c_handles[0]->XferCount;
}

/**
  \fn          int32_t I2C_Control (uint32_t control, uint32_t arg)
  \brief       Control I2C Interface.
  \param[in]   control  Operation
  \param[in]   arg      Argument of operation (optional)
  \return      \ref execution_status
*/
static int32_t I2C_Control(uint32_t control, uint32_t arg)
{
    I2C_HandleTypeDef *hi2c;
    uint32_t           i2c_idx = 0; /* Default to I2C1 */

    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    if (i2c_handles[i2c_idx] == NULL)
    {
        return ARM_DRIVER_ERROR;
    }

    hi2c = i2c_handles[i2c_idx];

    switch (control)
    {
        case ARM_I2C_OWN_ADDRESS:
            /* Set own slave address */
            hi2c->Init.OwnAddress1 = (uint16_t)arg;
            if (HAL_I2C_Init(hi2c) != HAL_OK)
            {
                return ARM_DRIVER_ERROR;
            }
            break;

        case ARM_I2C_BUS_SPEED:
            /* Set bus speed */
            hi2c->Init.Timing = arg;
            if (HAL_I2C_Init(hi2c) != HAL_OK)
            {
                return ARM_DRIVER_ERROR;
            }
            break;

        case ARM_I2C_BUS_CLEAR:
            /* Bus clear not implemented */
            return ARM_DRIVER_ERROR_UNSUPPORTED;

        case ARM_I2C_ABORT_TRANSFER:
            /* Abort transfer */
            HAL_I2C_Abort_IT(hi2c);
            break;

        default:
            return ARM_DRIVER_ERROR_UNSUPPORTED;
    }
    return ARM_DRIVER_OK;
}

/**
  \fn          ARM_I2C_STATUS I2C_GetStatus (void)
  \brief       Get I2C status.
  \return      I2C status \ref ARM_I2C_STATUS
*/
static ARM_I2C_STATUS I2C_GetStatus(void)
{
    ARM_I2C_STATUS status = {0};

    /* For simplicity, we'll use I2C1 as default */
    if (i2c_handles[0] != NULL)
    {
        if (HAL_I2C_GetState(i2c_handles[0]) == HAL_I2C_STATE_READY)
        {
            status.busy = 0;
        }
        else
        {
            status.busy = 1;
        }

        status.mode             = 0; /* Master mode */
        status.direction        = 0; /* Transmitter */
        status.general_call     = 0; /* Not received */
        status.arbitration_lost = 0; /* Not detected */
        status.bus_error        = 0; /* Not detected */
    }

    return status;
}

/**
  \fn          void I2C_SignalEvent (uint32_t event)
  \brief       Signal I2C Events.
  \param[in]   event  \ref I2C_EVENT notification mask
  \return      none
*/
static void I2C_SignalEvent(uint32_t event)
{
    /* Event signaling not implemented in this basic version */
    (void)event;
}

/* I2C0 Driver access structure */
ARM_DRIVER_I2C Driver_I2C0 = {
    I2C_GetVersion,   I2C_GetCapabilities, I2C_Initialize,    I2C_Uninitialize,
    I2C_PowerControl, I2C_MasterTransmit,  I2C_MasterReceive, I2C_SlaveTransmit,
    I2C_SlaveReceive, I2C_GetDataCount,    I2C_Control,       I2C_GetStatus,
    I2C_SignalEvent};
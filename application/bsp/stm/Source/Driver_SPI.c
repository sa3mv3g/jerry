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

#include "Driver_SPI.h"

#include "stm32h5xx_hal.h"

#define SPI_NUM 3U /* STM32H5 has SPI1, SPI2, SPI3 */

/* SPI Driver state */
static ARM_DRIVER_STATE DriverState = ARM_DRIVER_STATE_UNINITIALIZED;

/* SPI handle pointers */
static SPI_HandleTypeDef *spi_handles[SPI_NUM] = {NULL};

/* SPI clock enable functions */
static void (*spi_clk_enable[SPI_NUM])(void) = {__HAL_RCC_SPI1_CLK_ENABLE,
                                                __HAL_RCC_SPI2_CLK_ENABLE,
                                                __HAL_RCC_SPI3_CLK_ENABLE};

/* SPI clock disable functions */
static void (*spi_clk_disable[SPI_NUM])(void) = {__HAL_RCC_SPI1_CLK_DISABLE,
                                                 __HAL_RCC_SPI2_CLK_DISABLE,
                                                 __HAL_RCC_SPI3_CLK_DISABLE};

/* SPI IRQ numbers */
static const IRQn_Type spi_irq[SPI_NUM] = {SPI1_IRQn, SPI2_IRQn, SPI3_IRQn};

/* SPI peripheral base addresses */
static SPI_TypeDef *const spi_reg[SPI_NUM] = {SPI1, SPI2, SPI3};

/**
  \fn          ARM_DRIVER_VERSION SPI_GetVersion (void)
  \brief       Get driver version.
  \return      \ref ARM_DRIVER_VERSION
*/
static ARM_DRIVER_VERSION SPI_GetVersion(void)
{
    static const ARM_DRIVER_VERSION version = {
        .api = ARM_DRIVER_VERSION_MAJOR_MINOR(2, 0), /* API version */
        .drv = ARM_DRIVER_VERSION_MAJOR_MINOR(1, 0)  /* Driver version */
    };
    return version;
}

/**
  \fn          ARM_SPI_CAPABILITIES SPI_GetCapabilities (void)
  \brief       Get driver capabilities.
  \return      \ref ARM_SPI_CAPABILITIES
*/
static ARM_SPI_CAPABILITIES SPI_GetCapabilities(void)
{
    static const ARM_SPI_CAPABILITIES capabilities = {
        .event_mode_fault = 0,  /* Supports Mode Fault Event */
        .data_bits        = 16, /* Maximum Data Bits supported */
        .ss_input         = 0,  /* Supports SSEL input pin */
        .ti_ssi           = 0,  /* Supports TI Synchronous Serial Interface */
        .microwire        = 0   /* Supports Microwire Interface */
    };
    return capabilities;
}

/**
  \fn          int32_t SPI_Initialize (ARM_SPI_SignalEvent_t cb_event)
  \brief       Initialize SPI Interface.
  \param[in]   cb_event  Pointer to \ref ARM_SPI_SignalEvent
  \return      \ref execution_status
*/
static int32_t SPI_Initialize(ARM_SPI_SignalEvent_t cb_event)
{
    if (DriverState == ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_OK;
    }

    DriverState = ARM_DRIVER_STATE_READY;
    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t SPI_Uninitialize (void)
  \brief       De-initialize SPI Interface.
  \return      \ref execution_status
*/
static int32_t SPI_Uninitialize(void)
{
    uint32_t i;

    /* Disable all SPI peripherals */
    for (i = 0; i < SPI_NUM; i++)
    {
        if (spi_handles[i] != NULL)
        {
            HAL_SPI_DeInit(spi_handles[i]);
            spi_handles[i] = NULL;
        }
    }

    DriverState = ARM_DRIVER_STATE_UNINITIALIZED;
    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t SPI_PowerControl (ARM_POWER_STATE state)
  \brief       Control SPI Interface Power.
  \param[in]   state  Power state
  \return      \ref execution_status
*/
static int32_t SPI_PowerControl(ARM_POWER_STATE state)
{
    switch (state)
    {
        case ARM_POWER_OFF:
            /* Disable all SPI peripherals */
            SPI_Uninitialize();
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
  \fn          int32_t SPI_Send (const void *data, uint32_t num)
  \brief       Start sending data to SPI transmitter.
  \param[in]   data  Pointer to buffer with data to send
  \param[in]   num   Number of data items to send
  \return      \ref execution_status
*/
static int32_t SPI_Send(const void *data, uint32_t num)
{
    /* For simplicity, we'll use SPI1 as default */
    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    if (spi_handles[0] == NULL)
    {
        return ARM_DRIVER_ERROR;
    }

    if (HAL_SPI_Transmit(spi_handles[0], (uint8_t *)data, num, 1000) != HAL_OK)
    {
        return ARM_DRIVER_ERROR;
    }

    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t SPI_Receive (void *data, uint32_t num)
  \brief       Start receiving data from SPI receiver.
  \param[out]  data  Pointer to buffer for data to receive
  \param[in]   num   Number of data items to receive
  \return      \ref execution_status
*/
static int32_t SPI_Receive(void *data, uint32_t num)
{
    /* For simplicity, we'll use SPI1 as default */
    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    if (spi_handles[0] == NULL)
    {
        return ARM_DRIVER_ERROR;
    }

    if (HAL_SPI_Receive(spi_handles[0], (uint8_t *)data, num, 1000) != HAL_OK)
    {
        return ARM_DRIVER_ERROR;
    }

    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t SPI_Transfer (const void *data_out, void *data_in,
  uint32_t num)
  \brief       Start sending/receiving data to/from SPI transmitter/receiver.
  \param[in]   data_out  Pointer to buffer with data to send
  \param[out]  data_in   Pointer to buffer for data to receive
  \param[in]   num       Number of data items to transfer
  \return      \ref execution_status
*/
static int32_t SPI_Transfer(const void *data_out, void *data_in, uint32_t num)
{
    /* For simplicity, we'll use SPI1 as default */
    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    if (spi_handles[0] == NULL)
    {
        return ARM_DRIVER_ERROR;
    }

    if (HAL_SPI_TransmitReceive(spi_handles[0], (uint8_t *)data_out,
                                (uint8_t *)data_in, num, 1000) != HAL_OK)
    {
        return ARM_DRIVER_ERROR;
    }

    return ARM_DRIVER_OK;
}

/**
  \fn          uint32_t SPI_GetDataCount (void)
  \brief       Get transmitted/received data count.
  \return      number of data items transferred
*/
static uint32_t SPI_GetDataCount(void)
{
    /* For simplicity, we'll use SPI1 as default */
    if (spi_handles[0] == NULL)
    {
        return 0;
    }

    return spi_handles[0]
        ->TxXferSize; /* In full-duplex mode, Tx and Rx counts are the same */
}

/**
  \fn          int32_t SPI_Control (uint32_t control, uint32_t arg)
  \brief       Control SPI Interface.
  \param[in]   control  Operation
  \param[in]   arg      Argument of operation (optional)
  \return      \ref execution_status
*/
static int32_t SPI_Control(uint32_t control, uint32_t arg)
{
    SPI_HandleTypeDef *hspi;
    uint32_t           spi_idx = 0; /* Default to SPI1 */

    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    if (spi_handles[spi_idx] == NULL)
    {
        return ARM_DRIVER_ERROR;
    }

    hspi = spi_handles[spi_idx];

    switch (control)
    {
        case ARM_SPI_CONTROL_SS:
            /* Slave select not implemented in this basic version */
            (void)arg;
            return ARM_DRIVER_ERROR_UNSUPPORTED;

        case ARM_SPI_CONTROL_BITS:
            /* Set number of bits - would require reinitialization */
            return ARM_DRIVER_ERROR_UNSUPPORTED;

        case ARM_SPI_CONTROL_MODE:
            /* Set master/slave mode - would require reinitialization */
            (void)arg;
            return ARM_DRIVER_ERROR_UNSUPPORTED;

        default:
            return ARM_DRIVER_ERROR_UNSUPPORTED;
    }
    return ARM_DRIVER_OK;
}

/**
  \fn          ARM_SPI_STATUS SPI_GetStatus (void)
  \brief       Get SPI status.
  \return      SPI status \ref ARM_SPI_STATUS
*/
static ARM_SPI_STATUS SPI_GetStatus(void)
{
    ARM_SPI_STATUS status = {0};

    /* For simplicity, we'll use SPI1 as default */
    if (spi_handles[0] != NULL)
    {
        if (HAL_SPI_GetState(spi_handles[0]) == HAL_SPI_STATE_READY)
        {
            status.busy = 0;
        }
        else
        {
            status.busy = 1;
        }

        status.data_lost  = 0; /* Not detected */
        status.mode_fault = 0; /* Not detected */
    }

    return status;
}

/**
  \fn          void SPI_SignalEvent (uint32_t event)
  \brief       Signal SPI Events.
  \param[in]   event  \ref SPI_EVENT notification mask
  \return      none
*/
static void SPI_SignalEvent(uint32_t event)
{
    /* Event signaling not implemented in this basic version */
    (void)event;
}

/* SPI0 Driver access structure */
ARM_DRIVER_SPI Driver_SPI0 = {
    SPI_GetVersion,   SPI_GetCapabilities, SPI_Initialize,
    SPI_Uninitialize, SPI_PowerControl,    SPI_Send,
    SPI_Receive,      SPI_Transfer,        SPI_GetDataCount,
    SPI_Control,      SPI_GetStatus,       SPI_SignalEvent};
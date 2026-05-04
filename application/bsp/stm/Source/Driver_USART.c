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

#include "Driver_USART.h"

#include "stm32h5xx_hal.h"

#define USART_NUM 3U /* STM32H5 has USART1, USART2, USART3 */

/* USART Driver state */
static ARM_DRIVER_STATE DriverState = ARM_DRIVER_STATE_UNINITIALIZED;

/* USART handle pointers */
static UART_HandleTypeDef *usart_handles[USART_NUM] = {NULL};

/* USART clock enable functions */
static void (*usart_clk_enable[USART_NUM])(void) = {
    __HAL_RCC_USART1_CLK_ENABLE, __HAL_RCC_USART2_CLK_ENABLE,
    __HAL_RCC_USART3_CLK_ENABLE};

/* USART clock disable functions */
static void (*usart_clk_disable[USART_NUM])(void) = {
    __HAL_RCC_USART1_CLK_DISABLE, __HAL_RCC_USART2_CLK_DISABLE,
    __HAL_RCC_USART3_CLK_DISABLE};

/* USART IRQ numbers */
static const IRQn_Type usart_irq[USART_NUM] = {USART1_IRQn, USART2_IRQn,
                                               USART3_IRQn};

/* USART peripheral base addresses */
static USART_TypeDef *const usart_reg[USART_NUM] = {USART1, USART2, USART3};

/**
  \fn          ARM_DRIVER_VERSION USART_GetVersion (void)
  \brief       Get driver version.
  \return      \ref ARM_DRIVER_VERSION
*/
static ARM_DRIVER_VERSION USART_GetVersion(void)
{
    static const ARM_DRIVER_VERSION version = {
        .api = ARM_DRIVER_VERSION_MAJOR_MINOR(2, 0), /* API version */
        .drv = ARM_DRIVER_VERSION_MAJOR_MINOR(1, 0)  /* Driver version */
    };
    return version;
}

/**
  \fn          ARM_USART_CAPABILITIES USART_GetCapabilities (void)
  \brief       Get driver capabilities.
  \return      \ref ARM_USART_CAPABILITIES
*/
static ARM_USART_CAPABILITIES USART_GetCapabilities(void)
{
    static const ARM_USART_CAPABILITIES capabilities = {
        .uart             = 1, /* supports UART (Asynchronous) mode */
        .smart_card       = 0, /* supports Smart Card mode */
        .smart_card_clock = 0, /* Smart Card Clock generator available */
        .irda             = 0, /* supports UART IrDA mode */
        .rs485            = 0, /* supports UART RS485 mode */
        .half_duplex      = 0, /* supports UART Half-duplex mode */
        .lin              = 0, /* supports UART LIN mode */
        .flow_control_rts = 0, /* RTS Flow Control available */
        .flow_control_cts = 0, /* CTS Flow Control available */
        .event_tx_complete =
            0, /* Transmit completed event: \ref ARM_USART_EVENT_TX_COMPLETE */
        .event_rx_timeout = 0, /* Signal receive character timeout event: \ref
                                  ARM_USART_EVENT_RX_TIMEOUT */
        .rts       = 0,        /* RTS Line: 0=not available, 1=available */
        .cts       = 0,        /* CTS Line: 0=not available, 1=available */
        .dtr       = 0,        /* DTR Line: 0=not available, 1=available */
        .dsr       = 0,        /* DSR Line: 0=not available, 1=available */
        .dcd       = 0,        /* DCD Line: 0=not available, 1=available */
        .ri        = 0,        /* RI Line: 0=not available, 1=available */
        .event_cts = 0, /* Signal CTS change event: \ref ARM_USART_EVENT_CTS */
        .event_dsR = 0, /* Signal DSR change event: \ref ARM_USART_EVENT_DSR */
        .event_dcd = 0, /* Signal DCD change event: \ref ARM_USART_EVENT_DCD */
        .event_ri  = 0, /* Signal RI change event: \ref ARM_USART_EVENT_RI */
        .reserved  = 0  /* Reserved (must be zero) */
    };
    return capabilities;
}

/**
  \fn          int32_t USART_Initialize (ARM_USART_SignalEvent_t cb_event)
  \brief       Initialize USART Interface.
  \param[in]   cb_event  Pointer to \ref ARM_USART_SignalEvent
  \return      \ref execution_status
*/
static int32_t USART_Initialize(ARM_USART_SignalEvent_t cb_event)
{
    if (DriverState == ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_OK;
    }

    DriverState = ARM_DRIVER_STATE_READY;
    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USART_Uninitialize (void)
  \brief       De-initialize USART Interface.
  \return      \ref execution_status
*/
static int32_t USART_Uninitialize(void)
{
    uint32_t i;

    /* Disable all USART peripherals */
    for (i = 0; i < USART_NUM; i++)
    {
        if (usart_handles[i] != NULL)
        {
            HAL_UART_DeInit(usart_handles[i]);
            usart_handles[i] = NULL;
        }
    }

    DriverState = ARM_DRIVER_STATE_UNINITIALIZED;
    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USART_PowerControl (ARM_POWER_STATE state)
  \brief       Control USART Interface Power.
  \param[in]   state  Power state
  \return      \ref execution_status
*/
static int32_t USART_PowerControl(ARM_POWER_STATE state)
{
    switch (state)
    {
        case ARM_POWER_OFF:
            /* Disable all USART peripherals */
            USART_Uninitialize();
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
  \fn          int32_t USART_Send (const void *data, uint32_t num)
  \brief       Start sending data to USART transmitter.
  \param[in]   data  Pointer to buffer with data to send
  \param[in]   num   Number of data items to send
  \return      \ref execution_status
*/
static int32_t USART_Send(const void *data, uint32_t num)
{
    /* For simplicity, we'll use USART1 as default */
    /* In a full implementation, this would need to specify which USART instance
     */
    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    if (usart_handles[0] == NULL)
    {
        return ARM_DRIVER_ERROR;
    }

    if (HAL_UART_Transmit(usart_handles[0], (uint8_t *)data, num, 1000) !=
        HAL_OK)
    {
        return ARM_DRIVER_ERROR;
    }

    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USART_Receive (void *data, uint32_t num)
  \brief       Start receiving data from USART receiver.
  \param[out]  data  Pointer to buffer for data to receive
  \param[in]   num   Number of data items to receive
  \return      \ref execution_status
*/
static int32_t USART_Receive(void *data, uint32_t num)
{
    /* For simplicity, we'll use USART1 as default */
    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    if (usart_handles[0] == NULL)
    {
        return ARM_DRIVER_ERROR;
    }

    if (HAL_UART_Receive(usart_handles[0], (uint8_t *)data, num, 1000) !=
        HAL_OK)
    {
        return ARM_DRIVER_ERROR;
    }

    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t USART_Transfer (const void *data_out, void *data_in,
  uint32_t num)
  \brief       Start sending/receiving data to/from USART transmitter/receiver.
  \param[in]   data_out  Pointer to buffer with data to send
  \param[out]  data_in   Pointer to buffer for data to receive
  \param[in]   num       Number of data items to transfer
  \return      \ref execution_status
*/
static int32_t USART_Transfer(const void *data_out, void *data_in, uint32_t num)
{
    /* For simplicity, we'll use USART1 as default */
    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    if (usart_handles[0] == NULL)
    {
        return ARM_DRIVER_ERROR;
    }

    if (HAL_UART_TransmitReceive(usart_handles[0], (uint8_t *)data_out,
                                 (uint8_t *)data_in, num, 1000) != HAL_OK)
    {
        return ARM_DRIVER_ERROR;
    }

    return ARM_DRIVER_OK;
}

/**
  \fn          uint32_t USART_GetTxCount (void)
  \brief       Get transmitted data count.
  \return      number of data items transmitted
*/
static uint32_t USART_GetTxCount(void)
{
    /* For simplicity, we'll use USART1 as default */
    if (usart_handles[0] == NULL)
    {
        return 0;
    }

    return usart_handles[0]->TxXferCount;
}

/**
  \fn          uint32_t USART_GetRxCount (void)
  \brief       Get received data count.
  \return      number of data items received
*/
static uint32_t USART_GetRxCount(void)
{
    /* For simplicity, we'll use USART1 as default */
    if (usart_handles[0] == NULL)
    {
        return 0;
    }

    return usart_handles[0]->RxXferCount;
}

/**
  \fn          int32_t USART_Control (uint32_t control, uint32_t arg)
  \brief       Control USART Interface.
  \param[in]   control  Operation
  \param[in]   arg      Argument of operation (optional)
  \return      \ref execution_status
*/
static int32_t USART_Control(uint32_t control, uint32_t arg)
{
    UART_HandleTypeDef *huart;
    uint32_t            usart_idx = 0; /* Default to USART1 */

    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    if (usart_handles[usart_idx] == NULL)
    {
        return ARM_DRIVER_ERROR;
    }

    huart = usart_handles[usart_idx];

    switch (control)
    {
        case ARM_USART_CONTROL_TX:
            if (arg)
            {
                __HAL_UART_ENABLE_IT(huart, UART_IT_TXE);
            }
            else
            {
                __HAL_UART_DISABLE_IT(huart, UART_IT_TXE);
            }
            break;

        case ARM_USART_CONTROL_RX:
            if (arg)
            {
                __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);
            }
            else
            {
                __HAL_UART_DISABLE_IT(huart, UART_IT_RXNE);
            }
            break;

        case ARM_USART_CONTROL_TX_ENABLE:
            __HAL_UART_ENABLE(huart);
            break;

        case ARM_USART_CONTROL_RX_ENABLE:
            __HAL_UART_ENABLE(huart);
            break;

        case ARM_USART_CONTROL_TX_DISABLE:
            __HAL_UART_DISABLE(huart);
            break;

        case ARM_USART_CONTROL_RX_DISABLE:
            __HAL_UART_DISABLE(huart);
            break;

        default:
            return ARM_DRIVER_ERROR_UNSUPPORTED;
    }
    return ARM_DRIVER_OK;
}

/**
  \fn          ARM_USART_STATUS USART_GetStatus (void)
  \brief       Get USART status.
  \return      USART status \ref ARM_USART_STATUS
*/
static ARM_USART_STATUS USART_GetStatus(void)
{
    ARM_USART_STATUS status = {0};

    /* For simplicity, we'll use USART1 as default */
    if (usart_handles[0] != NULL)
    {
        if ((usart_handles[0]->Instance->ISR & USART_ISR_TXE) != 0)
        {
            status.tx_busy = 0;
        }
        else
        {
            status.tx_busy = 1;
        }

        if ((usart_handles[0]->Instance->ISR & USART_ISR_RXNE) != 0)
        {
            status.rx_busy = 1;
        }
        else
        {
            status.rx_busy = 0;
        }

        status.tx_underflow     = 0; /* Not detected */
        status.rx_overflow      = 0; /* Not detected */
        status.rx_framing_error = 0; /* Not detected */
        status.rx_parity_error  = 0; /* Not detected */
    }

    return status;
}

/**
  \fn          int32_t USART_SetModemControl (ARM_USART_MODEM_CONTROL control)
  \brief       Set USART Modem Control line state.
  \param[in]   control  \ref ARM_USART_MODEM_CONTROL
  \return      \ref execution_status
*/
static int32_t USART_SetModemControl(ARM_USART_MODEM_CONTROL control)
{
    /* Modem control not implemented for basic USART */
    (void)control;
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

/**
  \fn          ARM_USART_MODEM_STATUS USART_GetModemStatus (void)
  \brief       Get USART Modem Control lines state.
  \return      \ref ARM_USART_MODEM_STATUS
*/
static ARM_USART_MODEM_STATUS USART_GetModemStatus(void)
{
    /* Modem status not implemented for basic USART */
    static ARM_USART_MODEM_STATUS status = {0};
    return status;
}

/**
  \fn          void USART_SignalEvent (uint32_t event)
  \brief       Signal USART Events.
  \param[in]   event  \ref USART_EVENT notification mask
  \return      none
*/
static void USART_SignalEvent(uint32_t event)
{
    /* Event signaling not implemented in this basic version */
    (void)event;
}

/* USART0 Driver access structure */
ARM_DRIVER_USART Driver_USART0 = {
    USART_GetVersion,      USART_GetCapabilities, USART_Initialize,
    USART_Uninitialize,    USART_PowerControl,    USART_Send,
    USART_Receive,         USART_Transfer,        USART_GetTxCount,
    USART_GetRxCount,      USART_Control,         USART_GetStatus,
    USART_SetModemControl, USART_GetModemStatus,  USART_SignalEvent};
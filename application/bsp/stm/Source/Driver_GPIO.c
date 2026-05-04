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

#include "Driver_GPIO.h"

#include "stm32h5xx_hal.h"

#define GPIO_PINS_NUM 16U
#define GPIO_PORT_NUM 7U /* STM32H5 has GPIOA-GPIOG */

/* GPIO Driver state */
static ARM_DRIVER_STATE DriverState = ARM_DRIVER_STATE_UNINITIALIZED;

/* Pin mapping table */
static const uint16_t GPIO_Pin[16] = {
    GPIO_PIN_0,  GPIO_PIN_1,  GPIO_PIN_2,  GPIO_PIN_3, GPIO_PIN_4,  GPIO_PIN_5,
    GPIO_PIN_6,  GPIO_PIN_7,  GPIO_PIN_8,  GPIO_PIN_9, GPIO_PIN_10, GPIO_PIN_11,
    GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14, GPIO_PIN_15};

/* GPIO port mapping table */
static GPIO_TypeDef* const GPIO_Port[7] = {GPIOA, GPIOB, GPIOC, GPIOD,
                                           GPIOE, GPIOF, GPIOG};

/**
  \fn          ARM_DRIVER_VERSION GPIO_GetVersion (void)
  \brief       Get driver version.
  \return      \ref ARM_DRIVER_VERSION
*/
static ARM_DRIVER_VERSION GPIO_GetVersion(void)
{
    static const ARM_DRIVER_VERSION version = {
        .api = ARM_DRIVER_VERSION_MAJOR_MINOR(2, 0), /* API version */
        .drv = ARM_DRIVER_VERSION_MAJOR_MINOR(1, 0)  /* Driver version */
    };
    return version;
}

/**
  \fn          ARM_GPIO_CAPABILITIES GPIO_GetCapabilities (void)
  \brief       Get driver capabilities.
  \return      \ref ARM_GPIO_CAPABILITIES
*/
static ARM_GPIO_CAPABILITIES GPIO_GetCapabilities(void)
{
    static const ARM_GPIO_CAPABILITIES capabilities = {
        .pin_direction     = 1, /* Supports setting pin direction */
        .pin_output_mode   = 1, /* Supports setting output mode */
        .pin_pull_resistor = 1, /* Supports setting pull resistor */
        .pin_event         = 1  /* Supports pin events */
    };
    return capabilities;
}

/**
  \fn          int32_t GPIO_Initialize (ARM_GPIO_SignalEvent_t cb_event)
  \brief       Initialize GPIO Interface.
  \param[in]   cb_event  Pointer to \ref ARM_GPIO_SignalEvent
  \return      \ref execution_status
*/
static int32_t GPIO_Initialize(ARM_GPIO_SignalEvent_t cb_event)
{
    if (DriverState == ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_OK;
    }

    DriverState = ARM_DRIVER_STATE_READY;
    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t GPIO_Uninitialize (void)
  \brief       De-initialize GPIO Interface.
  \return      \ref execution_status
*/
static int32_t GPIO_Uninitialize(void)
{
    DriverState = ARM_DRIVER_STATE_UNINITIALIZED;
    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t GPIO_PowerControl (ARM_POWER_STATE state)
  \brief       Control GPIO Interface Power.
  \param[in]   state  Power state
  \return      \ref execution_status
*/
static int32_t GPIO_PowerControl(ARM_POWER_STATE state)
{
    switch (state)
    {
        case ARM_POWER_OFF:
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
  \fn          int32_t GPIO_PinConfigure (ARM_GPIO_Pin_t pin,
                                          ARM_GPIO_DIRECTION  direction,
                                          ARM_GPIO_OUTPUT_MODE mode,
                                          ARM_GPIO_PULL_RESISTOR  pull,
                                          ARM_GPIO_EVENT_TRIGGER  event)
  \brief       Configure GPIO Pin.
  \param[in]   pin      Port Pin (encoded as port*16 + pin_number)
  \param[in]   direction  Pin direction
  \param[in]   mode     Output mode
  \param[in]   pull     Pull resistor
  \param[in]   event    Pin event
  \return      \ref execution_status
*/
static int32_t GPIO_PinConfigure(ARM_GPIO_Pin_t         pin,
                                 ARM_GPIO_DIRECTION     direction,
                                 ARM_GPIO_OUTPUT_MODE   mode,
                                 ARM_GPIO_PULL_RESISTOR pull,
                                 ARM_GPIO_EVENT_TRIGGER event)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint32_t         port_num, pin_num;

    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    /* Decode pin number: upper 4 bits = port, lower 4 bits = pin */
    port_num = (pin >> 4) & 0x0F;
    pin_num  = pin & 0x0F;

    if (port_num >= GPIO_PORT_NUM || pin_num >= GPIO_PINS_NUM)
    {
        return ARM_GPIO_ERROR_PIN;
    }

    /* Enable GPIO clock */
    switch (port_num)
    {
        case 0:
            __HAL_RCC_GPIOA_CLK_ENABLE();
            break;
        case 1:
            __HAL_RCC_GPIOB_CLK_ENABLE();
            break;
        case 2:
            __HAL_RCC_GPIOC_CLK_ENABLE();
            break;
        case 3:
            __HAL_RCC_GPIOD_CLK_ENABLE();
            break;
        case 4:
            __HAL_RCC_GPIOE_CLK_ENABLE();
            break;
        case 5:
            __HAL_RCC_GPIOF_CLK_ENABLE();
            break;
        case 6:
            __HAL_RCC_GPIOG_CLK_ENABLE();
            break;
        default:
            return ARM_DRIVER_ERROR;
    }

    /* Configure GPIO pin */
    GPIO_InitStruct.Pin = GPIO_Pin[pin_num];
    GPIO_InitStruct.Mode =
        (direction == ARM_GPIO_INPUT) ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull =
        (pull == ARM_GPIO_PULL_UP)
            ? GPIO_PULLUP
            : ((pull == ARM_GPIO_PULL_DOWN) ? GPIO_PULLDOWN : GPIO_NOPULL);
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    if (direction == ARM_GPIO_OUTPUT)
    {
        GPIO_InitStruct.Mode = (mode == ARM_GPIO_OPEN_DRAIN)
                                   ? GPIO_MODE_OUTPUT_OD
                                   : GPIO_MODE_OUTPUT_PP;
    }

    HAL_GPIO_Init(GPIO_Port[port_num], &GPIO_InitStruct);

    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t GPIO_PinGetDirection (ARM_GPIO_Pin_t pin)
  \brief       Get GPIO Pin Direction.
  \param[in]   pin  Port Pin (encoded as port*16 + pin_number)
  \return      direction \ref ARM_GPIO_DIRECTION
*/
static int32_t GPIO_PinGetDirection(ARM_GPIO_Pin_t pin)
{
    GPIO_TypeDef*    GPIOx;
    uint32_t         port_num, pin_num;
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    /* Decode pin number: upper 4 bits = port, lower 4 bits = pin */
    port_num = (pin >> 4) & 0x0F;
    pin_num  = pin & 0x0F;

    if (port_num >= GPIO_PORT_NUM || pin_num >= GPIO_PINS_NUM)
    {
        return ARM_GPIO_ERROR_PIN;
    }

    GPIOx = GPIO_Port[port_num];

    /* Read pin configuration */
    GPIO_InitStruct.Pin  = GPIO_Pin[pin_num];
    GPIO_InitStruct.Mode = GPIOx->MODER & (GPIO_MODER_MODE0 << (pin_num * 2));
    GPIO_InitStruct.Pull = GPIOx->PUPDR & (GPIO_PUPDR_PUPD0 << (pin_num * 2));
    GPIO_InitStruct.Speed =
        GPIOx->OSPEEDR & (GPIO_OSPEEDR_OSPEED0 << (pin_num * 2));

    /* Determine direction */
    if ((GPIO_InitStruct.Mode & (GPIO_MODE_INPUT << (pin_num * 2))) != 0)
    {
        return ARM_GPIO_INPUT;
    }
    else
    {
        return ARM_GPIO_OUTPUT;
    }
}

/**
  \fn          int32_t GPIO_PinSetOutputMode (ARM_GPIO_Pin_t pin,
  ARM_GPIO_OUTPUT_MODE mode)
  \brief       Set GPIO Pin Output Mode.
  \param[in]   pin   Port Pin (encoded as port*16 + pin_number)
  \param[in]   mode  \ref ARM_GPIO_OUTPUT_MODE
  \return      \ref execution_status
*/
static int32_t GPIO_PinSetOutputMode(ARM_GPIO_Pin_t       pin,
                                     ARM_GPIO_OUTPUT_MODE mode)
{
    GPIO_TypeDef* GPIOx;
    uint32_t      port_num, pin_num;

    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    /* Decode pin number: upper 4 bits = port, lower 4 bits = pin */
    port_num = (pin >> 4) & 0x0F;
    pin_num  = pin & 0x0F;

    if (port_num >= GPIO_PORT_NUM || pin_num >= GPIO_PINS_NUM)
    {
        return ARM_GPIO_ERROR_PIN;
    }

    GPIOx = GPIO_Port[port_num];

    /* Check if pin is configured as output */
    if ((GPIOx->MODER & (GPIO_MODER_MODE0 << (pin_num * 2))) ==
        (GPIO_MODE_INPUT << (pin_num * 2)))
    {
        return ARM_DRIVER_ERROR;
    }

    /* Set output mode */
    if (mode == ARM_GPIO_OPEN_DRAIN)
    {
        GPIOx->OTYPER |= (GPIO_OTYPER_OT_0 << pin_num);
    }
    else
    {
        GPIOx->OTYPER &= ~(GPIO_OTYPER_OT_0 << pin_num);
    }

    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t GPIO_PinSetPullResistor (ARM_GPIO_Pin_t pin,
  ARM_GPIO_PULL_RESISTOR resistor)
  \brief       Set GPIO Pin Pull Resistor.
  \param[in]   pin      Port Pin (encoded as port*16 + pin_number)
  \param[in]   resistor \ref ARM_GPIO_PULL_RESISTOR
  \return      \ref execution_status
*/
static int32_t GPIO_PinSetPullResistor(ARM_GPIO_Pin_t         pin,
                                       ARM_GPIO_PULL_RESISTOR resistor)
{
    GPIO_TypeDef* GPIOx;
    uint32_t      port_num, pin_num;

    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    /* Decode pin number: upper 4 bits = port, lower 4 bits = pin */
    port_num = (pin >> 4) & 0x0F;
    pin_num  = pin & 0x0F;

    if (port_num >= GPIO_PORT_NUM || pin_num >= GPIO_PINS_NUM)
    {
        return ARM_GPIO_ERROR_PIN;
    }

    GPIOx = GPIO_Port[port_num];

    /* Set pull resistor */
    GPIOx->PUPDR &= ~(GPIO_PUPDR_PUPD0 << (pin_num * 2));

    switch (resistor)
    {
        case ARM_GPIO_PULL_NONE:
            GPIOx->PUPDR |= GPIO_NOPULL << (pin_num * 2);
            break;

        case ARM_GPIO_PULL_UP:
            GPIOx->PUPDR |= GPIO_PULLUP << (pin_num * 2);
            break;

        case ARM_GPIO_PULL_DOWN:
            GPIOx->PUPDR |= GPIO_PULLDOWN << (pin_num * 2);
            break;

        default:
            return ARM_DRIVER_ERROR_PARAMETER;
    }

    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t GPIO_PinSetEventTrigger (ARM_GPIO_Pin_t pin,
  ARM_GPIO_EVENT_TRIGGER event)
  \brief       Set GPIO Pin Event Trigger.
  \param[in]   pin   Port Pin (encoded as port*16 + pin_number)
  \param[in]   event \ref ARM_GPIO_EVENT_TRIGGER
  \return      \ref execution_status
*/
static int32_t GPIO_PinSetEventTrigger(ARM_GPIO_Pin_t         pin,
                                       ARM_GPIO_EVENT_TRIGGER event)
{
    /* Event trigger configuration would go here */
    /* For simplicity, we're not implementing external interrupts in this basic
     * version */
    (void)pin;
    (void)event;
    return ARM_DRIVER_OK;
}

/**
  \fn          int32_t GPIO_PinRead (ARM_GPIO_Pin_t pin)
  \brief       Read GPIO Pin Value.
  \param[in]   pin  Port Pin (encoded as port*16 + pin_number)
  \return      Pin value
*/
static int32_t GPIO_PinRead(ARM_GPIO_Pin_t pin)
{
    GPIO_TypeDef* GPIOx;
    uint32_t      port_num, pin_num;

    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    /* Decode pin number: upper 4 bits = port, lower 4 bits = pin */
    port_num = (pin >> 4) & 0x0F;
    pin_num  = pin & 0x0F;

    if (port_num >= GPIO_PORT_NUM || pin_num >= GPIO_PINS_NUM)
    {
        return ARM_GPIO_ERROR_PIN;
    }

    GPIOx = GPIO_Port[port_num];

    /* Read pin value */
    if ((GPIOx->IDR & GPIO_Pin[pin_num]) != 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
  \fn          int32_t GPIO_PinWrite (ARM_GPIO_Pin_t pin, int32_t value)
  \brief       Write GPIO Pin Value.
  \param[in]   pin   Port Pin (encoded as port*16 + pin_number)
  \param[in]   value Pin value (0 or 1)
  \return      \ref execution_status
*/
static int32_t GPIO_PinWrite(ARM_GPIO_Pin_t pin, int32_t value)
{
    GPIO_TypeDef* GPIOx;
    uint32_t      port_num, pin_num;

    if (DriverState != ARM_DRIVER_STATE_READY)
    {
        return ARM_DRIVER_ERROR;
    }

    /* Decode pin number: upper 4 bits = port, lower 4 bits = pin */
    port_num = (pin >> 4) & 0x0F;
    pin_num  = pin & 0x0F;

    if (port_num >= GPIO_PORT_NUM || pin_num >= GPIO_PINS_NUM)
    {
        return ARM_GPIO_ERROR_PIN;
    }

    GPIOx = GPIO_Port[port_num];

    /* Check if pin is configured as output */
    if ((GPIOx->MODER & (GPIO_MODER_MODE0 << (pin_num * 2))) ==
        (GPIO_MODE_INPUT << (pin_num * 2)))
    {
        return ARM_DRIVER_ERROR;
    }

    /* Write pin value */
    if (value != 0)
    {
        GPIOx->BSRR = GPIO_Pin[pin_num];
    }
    else
    {
        GPIOx->BRR = GPIO_Pin[pin_num];
    }

    return ARM_DRIVER_OK;
}

/* GPIO Driver access structure */
ARM_DRIVER_GPIO Driver_GPIO0 = {
    GPIO_GetVersion,         GPIO_GetCapabilities,  GPIO_Initialize,
    GPIO_Uninitialize,       GPIO_PowerControl,     GPIO_PinConfigure,
    GPIO_PinGetDirection,    GPIO_PinSetOutputMode, GPIO_PinSetPullResistor,
    GPIO_PinSetEventTrigger, GPIO_PinRead,          GPIO_PinWrite};
/**
 * @file jerry_device_callbacks.c
 * @brief Modbus callback implementations for jerry_device
 *
 * IMPORTANT: This file is maintained by the user and will NOT be overwritten
 * by the code generator.
 *
 * Device: jerry_device
 * Description: STM32H5xx based industrial controller with digital I/O, ADC, and
 * PWM Version: 1.0.0
 *
 * @copyright Copyright (c) 2026
 */

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "bsp.h"
#include "jerry_device_registers.h"
#include "modbus_callbacks.h"
#include "task.h"

/* ==========================================================================
 * Helper Macros
 * ========================================================================== */

/**
 * Check if address is within range.
 * Note: For unsigned types, we only check upper bound when min is 0
 * to avoid "comparison is always true" warnings.
 */
#define ADDR_IN_RANGE_FROM_ZERO(addr, max) ((addr) <= (max))
#define ADDR_IN_RANGE_NONZERO(addr, min, max) \
    (((addr) >= (min)) && ((addr) <= (max)))

/**
 * @brief Update a Modbus register with a filtered ADC value in millivolts
 *
 * Reads the filtered ADC value from the specified channel, converts it from
 * volts to millivolts, and stores the result in both the register structure
 * field and the output array. The update only occurs if the ADC filter has
 * settled (reached steady state).
 *
 * @param[in]  channel      ADC1 channel index (e.g., BSP_ADC1_CHANNEL_A0)
 * @param[out] pStructField Pointer to the holding register structure field to
 * update
 * @param[out] pArr         Pointer to the Modbus response array element to
 * update
 *
 * @note If the filter has not settled or the ADC read fails, no update is
 * performed
 * @note The ADC value is converted from volts (float) to millivolts (uint16_t)
 *
 * @see BSP_ADC1_IsFilterSettled()
 * @see BSP_ADC1_GetFilteredValue()
 */
static void update_reg_with_adcval(uint16_t channel, uint16_t *pStructField,
                                   uint16_t *pArr)
{
    // by default we get values in volts and hence float
    float32_t adcValv = 0;
    // then we convert it to mv and save it in an integer
    uint16_t adcValmv = 0;
    if (BSP_ADC1_IsFilterSettled())
    {
        if (BSP_OK != BSP_ADC1_GetFilteredValue(channel, &adcValv))
        {
            adcValv = 0.0f;
        }

        adcValmv = (uint16_t)(adcValv * 1000.0);
        *pArr = *pStructField = adcValmv;
    }
}

/**
 * @brief Update both system tick registers from FreeRTOS tick count
 *
 * This function reads the current FreeRTOS tick count and updates both
 * the LOW and HIGH registers atomically. This ensures consistency when
 * either register is read - both will reflect the same tick value.
 *
 * @param regs Pointer to holding registers structure
 */
static void update_system_tick_registers(jerry_device_holding_registers_t *regs)
{
    TickType_t ticks       = xTaskGetTickCount();
    regs->system_tick_low  = (uint16_t)(ticks & 0xFFFFU);
    regs->system_tick_high = (uint16_t)((ticks >> 16U) & 0xFFFFU);
}

/**
 * @brief Update a digital output via I2C and sync the coil register
 *
 * Reads the current I2C expander state, modifies the specified channel bit,
 * writes the new state back, and updates the coil register field on success.
 *
 * @param[in]  channel Channel index (BSP_I2CDO_INDEX_D0 to BSP_I2CDO_INDEX_D15)
 * @param[in]  val     New output value (true = ON, false = OFF)
 * @param[out] pCoil   Pointer to coil register field to update (can be NULL)
 *
 * @note The coil register is only updated if the I2C write succeeds
 */
static void update_digital_output(uint16_t channel, bool val, bool *pCoil)
{
    if (channel < 16U)
    {
        uint16_t    initVal = 0U;
        bsp_error_t err     = BSP_I2CDO_Read(&initVal);

        if (BSP_OK == err)
        {
            uint16_t mask     = BSP_I2CDO_CONSTRUCT_MASK(channel);
            uint16_t finalVal = 0;

            if (val == true)
            {
                finalVal = initVal | mask;
            }
            else
            {
                finalVal = initVal & (~mask);
            }

            err = BSP_I2CDO_Write(finalVal);

            if ((BSP_OK == err) && (NULL != pCoil))
            {
                *pCoil = val;
            }
        }
    }
}

/**
 * @brief Read a GPIO digital input and update coil/discrete input registers
 *
 * Reads the current state of the specified GPIO digital input channel using
 * the BSP layer and optionally updates the coil and/or discrete input register
 * fields with the result.
 *
 * @param[in]  channel GPIO digital input channel index
 *                     (BSP_GPIODI_INDEX_0 through BSP_GPIODI_INDEX_7)
 * @param[out] pCoil   Pointer to coil register field to update (can be NULL)
 * @param[out] pDi     Pointer to discrete input register field to update (can
 *                     be NULL)
 *
 * @return bsp_error_t
 * @retval BSP_OK    Read operation successful, output pointers updated
 * @retval BSP_ERROR Invalid channel or BSP read failure
 */
static bsp_error_t update_digital_input(unsigned int channel, bool *pCoil,
                                        bool *pDi)
{
    bsp_error_t apiStatus;
    uint32_t    inVal = 0;

    apiStatus = BSP_GPIODI_Read(channel, &inVal);
    if (BSP_OK == apiStatus)
    {
        if (NULL != pCoil)
        {
            *pCoil = inVal != 0;
        }
        if (NULL != pDi)
        {
            *pDi = inVal != 0;
        }
    }

    return apiStatus;
}

/* ==========================================================================
 * Coil Callbacks (FC01, FC05, FC15)
 * ========================================================================== */

/**
 * @brief Read coils callback (FC01)
 */
modbus_exception_t modbus_cb_read_coils(uint16_t start_address,
                                        uint16_t quantity, uint8_t *coil_values)
{
    jerry_device_coils_t *coils       = jerry_device_get_coils();
    uint16_t              end_address = start_address + quantity - 1U;
    uint16_t              byte_index;
    uint16_t              bit_index;
    bool                  value;

    /* Validate address range */
    if (!ADDR_IN_RANGE_FROM_ZERO(start_address, JERRY_DEVICE_COIL_MAX_ADDR) ||
        !ADDR_IN_RANGE_FROM_ZERO(end_address, JERRY_DEVICE_COIL_MAX_ADDR))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    /* Clear output buffer */
    for (uint16_t i = 0U; i < ((quantity + 7U) / 8U); i++)
    {
        coil_values[i] = 0U;
    }

    /* Read each coil */
    for (uint16_t i = 0U; i < quantity; i++)
    {
        uint16_t addr = start_address + i;
        value         = false;

        switch (addr)
        {
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_0:
                value = coils->digital_output_0;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_1:
                value = coils->digital_output_1;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_2:
                value = coils->digital_output_2;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_3:
                value = coils->digital_output_3;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_4:
                value = coils->digital_output_4;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_5:
                value = coils->digital_output_5;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_6:
                value = coils->digital_output_6;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_7:
                value = coils->digital_output_7;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_8:
                value = coils->digital_output_8;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_9:
                value = coils->digital_output_9;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_10:
                value = coils->digital_output_10;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_11:
                value = coils->digital_output_11;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_12:
                value = coils->digital_output_12;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_13:
                value = coils->digital_output_13;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_14:
                value = coils->digital_output_14;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_15:
                value = coils->digital_output_15;
                break;
            case JERRY_DEVICE_COIL_DIGITAL_INPUT_0:
                if (BSP_OK != update_digital_input(BSP_GPIODI_INDEX_0,
                                                   &(coils->digital_input_0),
                                                   NULL))
                {
                    return MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
                }
                else
                {
                    value = coils->digital_input_0;
                }
                break;
            case JERRY_DEVICE_COIL_DIGITAL_INPUT_1:
                if (BSP_OK != update_digital_input(BSP_GPIODI_INDEX_1,
                                                   &(coils->digital_input_1),
                                                   NULL))
                {
                    return MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
                }
                else
                {
                    value = coils->digital_input_1;
                }
                break;
            case JERRY_DEVICE_COIL_DIGITAL_INPUT_2:
                if (BSP_OK != update_digital_input(BSP_GPIODI_INDEX_2,
                                                   &(coils->digital_input_2),
                                                   NULL))
                {
                    return MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
                }
                else
                {
                    value = coils->digital_input_2;
                }
                break;
            case JERRY_DEVICE_COIL_DIGITAL_INPUT_3:
                if (BSP_OK != update_digital_input(BSP_GPIODI_INDEX_3,
                                                   &(coils->digital_input_3),
                                                   NULL))
                {
                    return MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
                }
                else
                {
                    value = coils->digital_input_3;
                }
                break;
            case JERRY_DEVICE_COIL_DIGITAL_INPUT_4:
                if (BSP_OK != update_digital_input(BSP_GPIODI_INDEX_4,
                                                   &(coils->digital_input_4),
                                                   NULL))
                {
                    return MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
                }
                else
                {
                    value = coils->digital_input_4;
                }
                break;
            case JERRY_DEVICE_COIL_DIGITAL_INPUT_5:
                if (BSP_OK != update_digital_input(BSP_GPIODI_INDEX_5,
                                                   &(coils->digital_input_5),
                                                   NULL))
                {
                    return MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
                }
                else
                {
                    value = coils->digital_input_5;
                }
                break;
            case JERRY_DEVICE_COIL_DIGITAL_INPUT_6:
                if (BSP_OK != update_digital_input(BSP_GPIODI_INDEX_6,
                                                   &(coils->digital_input_6),
                                                   NULL))
                {
                    return MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
                }
                else
                {
                    value = coils->digital_input_6;
                }
                break;
            case JERRY_DEVICE_COIL_DIGITAL_INPUT_7:
                if (BSP_OK != update_digital_input(BSP_GPIODI_INDEX_7,
                                                   &(coils->digital_input_7),
                                                   NULL))
                {
                    return MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE;
                }
                else
                {
                    value = coils->digital_input_7;
                }
                break;
            case JERRY_DEVICE_COIL_PWM_0_ENABLE:
                value = coils->pwm_0_enable;
                break;
            case JERRY_DEVICE_COIL_PWM_1_ENABLE:
                value = coils->pwm_1_enable;
                break;
            case JERRY_DEVICE_COIL_PWM_2_ENABLE:
                value = coils->pwm_2_enable;
                break;
            case JERRY_DEVICE_COIL_PWM_3_ENABLE:
                value = coils->pwm_3_enable;
                break;
            default:
                return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        }

        /* Pack bit into output buffer */
        byte_index = i / 8U;
        bit_index  = i % 8U;
        if (value)
        {
            coil_values[byte_index] |= (uint8_t)(1U << bit_index);
        }
    }

    return MODBUS_EXCEPTION_NONE;
}

/**
 * @brief Write single coil callback (FC05)
 */
modbus_exception_t modbus_cb_write_single_coil(uint16_t address, bool value)
{
    jerry_device_coils_t *coils = jerry_device_get_coils();

    switch (address)
    {
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_0:
            update_digital_output(BSP_I2CDO_INDEX_D0, value,
                                  &(coils->digital_output_0));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_1:
            update_digital_output(BSP_I2CDO_INDEX_D1, value,
                                  &(coils->digital_output_1));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_2:
            update_digital_output(BSP_I2CDO_INDEX_D2, value,
                                  &(coils->digital_output_2));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_3:
            update_digital_output(BSP_I2CDO_INDEX_D3, value,
                                  &(coils->digital_output_3));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_4:
            update_digital_output(BSP_I2CDO_INDEX_D4, value,
                                  &(coils->digital_output_4));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_5:
            update_digital_output(BSP_I2CDO_INDEX_D5, value,
                                  &(coils->digital_output_5));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_6:
            update_digital_output(BSP_I2CDO_INDEX_D6, value,
                                  &(coils->digital_output_6));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_7:
            update_digital_output(BSP_I2CDO_INDEX_D7, value,
                                  &(coils->digital_output_7));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_8:
            update_digital_output(BSP_I2CDO_INDEX_D8, value,
                                  &(coils->digital_output_8));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_9:
            update_digital_output(BSP_I2CDO_INDEX_D9, value,
                                  &(coils->digital_output_9));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_10:
            update_digital_output(BSP_I2CDO_INDEX_D10, value,
                                  &(coils->digital_output_10));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_11:
            update_digital_output(BSP_I2CDO_INDEX_D11, value,
                                  &(coils->digital_output_11));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_12:
            update_digital_output(BSP_I2CDO_INDEX_D12, value,
                                  &(coils->digital_output_12));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_13:
            update_digital_output(BSP_I2CDO_INDEX_D13, value,
                                  &(coils->digital_output_13));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_14:
            update_digital_output(BSP_I2CDO_INDEX_D14, value,
                                  &(coils->digital_output_14));
            break;
        case JERRY_DEVICE_COIL_DIGITAL_OUTPUT_15:
            update_digital_output(BSP_I2CDO_INDEX_D15, value,
                                  &(coils->digital_output_15));
            break;
        case JERRY_DEVICE_COIL_PWM_0_ENABLE:
            coils->pwm_0_enable = value;
            break;
        case JERRY_DEVICE_COIL_PWM_1_ENABLE:
            coils->pwm_1_enable = value;
            break;
        case JERRY_DEVICE_COIL_PWM_2_ENABLE:
            coils->pwm_2_enable = value;
            break;
        case JERRY_DEVICE_COIL_PWM_3_ENABLE:
            coils->pwm_3_enable = value;
            break;
        default:
            return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    return MODBUS_EXCEPTION_NONE;
}

/**
 * @brief Write multiple coils callback (FC15)
 */
modbus_exception_t modbus_cb_write_multiple_coils(uint16_t       start_address,
                                                  uint16_t       quantity,
                                                  const uint8_t *coil_values)
{
    modbus_exception_t result;
    uint16_t           byte_index;
    uint16_t           bit_index;
    bool               value;

    for (uint16_t i = 0U; i < quantity; i++)
    {
        byte_index = i / 8U;
        bit_index  = i % 8U;
        value      = ((coil_values[byte_index] >> bit_index) & 0x01U) != 0U;

        result = modbus_cb_write_single_coil(start_address + i, value);
        if (result != MODBUS_EXCEPTION_NONE)
        {
            return result;
        }
    }

    return MODBUS_EXCEPTION_NONE;
}

/* ==========================================================================
 * Discrete Input Callbacks (FC02)
 * ========================================================================== */

/**
 * @brief Read discrete inputs callback (FC02)
 */
modbus_exception_t modbus_cb_read_discrete_inputs(uint16_t start_address,
                                                  uint16_t quantity,
                                                  uint8_t *input_values)
{
    jerry_device_discrete_inputs_t *inputs = jerry_device_get_discrete_inputs();
    uint16_t                        end_address = start_address + quantity - 1U;
    uint16_t                        byte_index;
    uint16_t                        bit_index;
    bool                            value;

    /* Validate address range */
    if (!ADDR_IN_RANGE_FROM_ZERO(start_address, JERRY_DEVICE_DI_MAX_ADDR) ||
        !ADDR_IN_RANGE_FROM_ZERO(end_address, JERRY_DEVICE_DI_MAX_ADDR))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    /* Clear output buffer */
    for (uint16_t i = 0U; i < ((quantity + 7U) / 8U); i++)
    {
        input_values[i] = 0U;
    }

    /* Read each input */
    for (uint16_t i = 0U; i < quantity; i++)
    {
        uint16_t addr = start_address + i;
        value         = false;

        switch (addr)
        {
            case JERRY_DEVICE_DI_DIGITAL_INPUT_0:
                value = inputs->digital_input_0;
                break;
            case JERRY_DEVICE_DI_DIGITAL_INPUT_1:
                value = inputs->digital_input_1;
                break;
            case JERRY_DEVICE_DI_DIGITAL_INPUT_2:
                value = inputs->digital_input_2;
                break;
            case JERRY_DEVICE_DI_DIGITAL_INPUT_3:
                value = inputs->digital_input_3;
                break;
            case JERRY_DEVICE_DI_DIGITAL_INPUT_4:
                value = inputs->digital_input_4;
                break;
            case JERRY_DEVICE_DI_DIGITAL_INPUT_5:
                value = inputs->digital_input_5;
                break;
            case JERRY_DEVICE_DI_DIGITAL_INPUT_6:
                value = inputs->digital_input_6;
                break;
            case JERRY_DEVICE_DI_DIGITAL_INPUT_7:
                value = inputs->digital_input_7;
                break;
            default:
                return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        }

        /* Pack bit into output buffer */
        byte_index = i / 8U;
        bit_index  = i % 8U;
        if (value)
        {
            input_values[byte_index] |= (uint8_t)(1U << bit_index);
        }
    }

    return MODBUS_EXCEPTION_NONE;
}

/* ==========================================================================
 * Holding Register Callbacks (FC03, FC06, FC16)
 * ========================================================================== */

/**
 * @brief Read holding registers callback (FC03)
 */
modbus_exception_t modbus_cb_read_holding_registers(uint16_t  start_address,
                                                    uint16_t  quantity,
                                                    uint16_t *register_values)
{
    jerry_device_holding_registers_t *regs =
        jerry_device_get_holding_registers();
    uint16_t end_address = start_address + quantity - 1U;

    /* Validate address range */
    if (!ADDR_IN_RANGE_FROM_ZERO(start_address, JERRY_DEVICE_HR_MAX_ADDR) ||
        !ADDR_IN_RANGE_FROM_ZERO(end_address, JERRY_DEVICE_HR_MAX_ADDR))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    /* Read each register */
    for (uint16_t i = 0U; i < quantity; i++)
    {
        uint16_t addr = start_address + i;

        switch (addr)
        {
            case JERRY_DEVICE_HR_PWM_0_DUTY_CYCLE:
                register_values[i] = (uint16_t)regs->pwm_0_duty_cycle;
                break;
            case JERRY_DEVICE_HR_PWM_0_FREQUENCY:
                /* High word of 32-bit value */
                register_values[i] =
                    (uint16_t)(((uint32_t)regs->pwm_0_frequency) >> 16U);
                break;
            case JERRY_DEVICE_HR_PWM_0_FREQUENCY + 1U:
                /* Low word of 32-bit value */
                register_values[i] =
                    (uint16_t)((uint32_t)regs->pwm_0_frequency & 0xFFFFU);
                break;
            case JERRY_DEVICE_HR_PWM_1_DUTY_CYCLE:
                register_values[i] = (uint16_t)regs->pwm_1_duty_cycle;
                break;
            case JERRY_DEVICE_HR_PWM_1_FREQUENCY:
                /* High word of 32-bit value */
                register_values[i] =
                    (uint16_t)((uint32_t)regs->pwm_1_frequency >> 16U);
                break;
            case JERRY_DEVICE_HR_PWM_1_FREQUENCY + 1U:
                /* Low word of 32-bit value */
                register_values[i] =
                    (uint16_t)((uint32_t)regs->pwm_1_frequency & 0xFFFFU);
                break;
            case JERRY_DEVICE_HR_PWM_2_DUTY_CYCLE:
                register_values[i] = (uint16_t)regs->pwm_2_duty_cycle;
                break;
            case JERRY_DEVICE_HR_PWM_2_FREQUENCY:
                /* High word of 32-bit value */
                register_values[i] =
                    (uint16_t)((uint32_t)regs->pwm_2_frequency >> 16U);
                break;
            case JERRY_DEVICE_HR_PWM_2_FREQUENCY + 1U:
                /* Low word of 32-bit value */
                register_values[i] =
                    (uint16_t)((uint32_t)regs->pwm_2_frequency & 0xFFFFU);
                break;
            case JERRY_DEVICE_HR_PWM_3_DUTY_CYCLE:
                register_values[i] = (uint16_t)regs->pwm_3_duty_cycle;
                break;
            case JERRY_DEVICE_HR_PWM_3_FREQUENCY:
                /* High word of 32-bit value */
                register_values[i] =
                    (uint16_t)((uint32_t)regs->pwm_3_frequency >> 16U);
                break;
            case JERRY_DEVICE_HR_PWM_3_FREQUENCY + 1U:
                /* Low word of 32-bit value */
                register_values[i] =
                    (uint16_t)((uint32_t)regs->pwm_3_frequency & 0xFFFFU);
                break;
            case JERRY_DEVICE_HR_ADC_0_VALUE:
                update_reg_with_adcval(BSP_ADC1_CHANNEL_A0,
                                       &(regs->adc_0_value),
                                       &register_values[i]);
                break;
            case JERRY_DEVICE_HR_ADC_1_VALUE:
                update_reg_with_adcval(BSP_ADC1_CHANNEL_A1,
                                       &(regs->adc_1_value),
                                       &register_values[i]);
                break;
            case JERRY_DEVICE_HR_ADC_2_VALUE:
                update_reg_with_adcval(BSP_ADC1_CHANNEL_A2,
                                       &(regs->adc_2_value),
                                       &register_values[i]);
                break;
            case JERRY_DEVICE_HR_ADC_3_VALUE:
                update_reg_with_adcval(BSP_ADC1_CHANNEL_A3,
                                       &(regs->adc_3_value),
                                       &register_values[i]);
                break;
            case JERRY_DEVICE_HR_SYSTEM_TICK_LOW:
                /* Update both tick registers before returning either one */
                update_system_tick_registers(regs);
                register_values[i] = (uint16_t)regs->system_tick_low;
                break;
            case JERRY_DEVICE_HR_SYSTEM_TICK_HIGH:
                /* Update both tick registers before returning either one */
                update_system_tick_registers(regs);
                register_values[i] = (uint16_t)regs->system_tick_high;
                break;
            case JERRY_DEVICE_HR_RTC_YEAR:
                register_values[i] = (uint16_t)regs->rtc_year;
                break;
            case JERRY_DEVICE_HR_RTC_MONTH:
                register_values[i] = (uint16_t)regs->rtc_month;
                break;
            case JERRY_DEVICE_HR_RTC_DAY:
                register_values[i] = (uint16_t)regs->rtc_day;
                break;
            case JERRY_DEVICE_HR_RTC_HOUR:
                register_values[i] = (uint16_t)regs->rtc_hour;
                break;
            case JERRY_DEVICE_HR_RTC_MINUTE:
                register_values[i] = (uint16_t)regs->rtc_minute;
                break;
            case JERRY_DEVICE_HR_RTC_SECOND:
                register_values[i] = (uint16_t)regs->rtc_second;
                break;
            case JERRY_DEVICE_HR_APP_VERSION_MAJOR:
                register_values[i] = (uint16_t)regs->app_version_major;
                break;
            case JERRY_DEVICE_HR_APP_VERSION_MINOR:
                register_values[i] = (uint16_t)regs->app_version_minor;
                break;
            case JERRY_DEVICE_HR_APP_VERSION_PATCH:
                register_values[i] = (uint16_t)regs->app_version_patch;
                break;
            case JERRY_DEVICE_HR_APP_BUILD_NUMBER:
                /* High word of 32-bit value */
                register_values[i] =
                    (uint16_t)((uint32_t)regs->app_build_number >> 16U);
                break;
            case JERRY_DEVICE_HR_APP_BUILD_NUMBER + 1U:
                /* Low word of 32-bit value */
                register_values[i] =
                    (uint16_t)((uint32_t)regs->app_build_number & 0xFFFFU);
                break;
            default:
                return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        }
    }

    return MODBUS_EXCEPTION_NONE;
}

/**
 * @brief Write single register callback (FC06)
 */
modbus_exception_t modbus_cb_write_single_register(uint16_t address,
                                                   uint16_t value)
{
    jerry_device_holding_registers_t *regs =
        jerry_device_get_holding_registers();

    switch (address)
    {
        case JERRY_DEVICE_HR_PWM_0_DUTY_CYCLE:
            /* Validate value range */
            if (value > 10000U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            regs->pwm_0_duty_cycle = value;
            break;
        case JERRY_DEVICE_HR_PWM_1_DUTY_CYCLE:
            /* Validate value range */
            if (value > 10000U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            regs->pwm_1_duty_cycle = value;
            break;
        case JERRY_DEVICE_HR_PWM_2_DUTY_CYCLE:
            /* Validate value range */
            if (value > 10000U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            regs->pwm_2_duty_cycle = value;
            break;
        case JERRY_DEVICE_HR_PWM_3_DUTY_CYCLE:
            /* Validate value range */
            if (value > 10000U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            regs->pwm_3_duty_cycle = value;
            break;
        case JERRY_DEVICE_HR_RTC_YEAR:
            /* Validate value range */
            if (value < 2000U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            if (value > 2099U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            regs->rtc_year = value;
            break;
        case JERRY_DEVICE_HR_RTC_MONTH:
            /* Validate value range */
            if (value < 1U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            if (value > 12U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            regs->rtc_month = value;
            break;
        case JERRY_DEVICE_HR_RTC_DAY:
            /* Validate value range */
            if (value < 1U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            if (value > 31U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            regs->rtc_day = value;
            break;
        case JERRY_DEVICE_HR_RTC_HOUR:
            /* Validate value range */
            if (value > 23U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            regs->rtc_hour = value;
            break;
        case JERRY_DEVICE_HR_RTC_MINUTE:
            /* Validate value range */
            if (value > 59U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            regs->rtc_minute = value;
            break;
        case JERRY_DEVICE_HR_RTC_SECOND:
            /* Validate value range */
            if (value > 59U)
            {
                return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
            }
            regs->rtc_second = value;
            break;
        default:
            return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    return MODBUS_EXCEPTION_NONE;
}

/**
 * @brief Write multiple registers callback (FC16)
 */
modbus_exception_t modbus_cb_write_multiple_registers(
    uint16_t start_address, uint16_t quantity, const uint16_t *register_values)
{
    modbus_exception_t result;

    for (uint16_t i = 0U; i < quantity; i++)
    {
        result = modbus_cb_write_single_register(start_address + i,
                                                 register_values[i]);
        if (result != MODBUS_EXCEPTION_NONE)
        {
            return result;
        }
    }

    return MODBUS_EXCEPTION_NONE;
}

/* ==========================================================================
 * Input Register Callbacks (FC04)
 * ========================================================================== */

/**
 * @brief Read input registers callback (FC04)
 */
modbus_exception_t modbus_cb_read_input_registers(uint16_t  start_address,
                                                  uint16_t  quantity,
                                                  uint16_t *register_values)
{
    jerry_device_input_registers_t *regs = jerry_device_get_input_registers();
    uint16_t                        end_address = start_address + quantity - 1U;

    /* Validate address range */
    if (!ADDR_IN_RANGE_FROM_ZERO(start_address, JERRY_DEVICE_IR_MAX_ADDR) ||
        !ADDR_IN_RANGE_FROM_ZERO(end_address, JERRY_DEVICE_IR_MAX_ADDR))
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    /* Read each register */
    for (uint16_t i = 0U; i < quantity; i++)
    {
        uint16_t addr = start_address + i;

        switch (addr)
        {
            case JERRY_DEVICE_IR_ADC_0_VALUE:
                register_values[i] = (uint16_t)regs->adc_0_value;
                break;
            case JERRY_DEVICE_IR_ADC_1_VALUE:
                register_values[i] = (uint16_t)regs->adc_1_value;
                break;
            case JERRY_DEVICE_IR_ADC_2_VALUE:
                register_values[i] = (uint16_t)regs->adc_2_value;
                break;
            case JERRY_DEVICE_IR_ADC_3_VALUE:
                register_values[i] = (uint16_t)regs->adc_3_value;
                break;
            case JERRY_DEVICE_IR_APP_VERSION_MAJOR:
                register_values[i] = (uint16_t)regs->app_version_major;
                break;
            case JERRY_DEVICE_IR_APP_VERSION_MINOR:
                register_values[i] = (uint16_t)regs->app_version_minor;
                break;
            case JERRY_DEVICE_IR_APP_VERSION_PATCH:
                register_values[i] = (uint16_t)regs->app_version_patch;
                break;
            case JERRY_DEVICE_IR_APP_BUILD_NUMBER:
                /* High word of 32-bit value */
                register_values[i] =
                    (uint16_t)((uint32_t)regs->app_build_number >> 16U);
                break;
            case JERRY_DEVICE_IR_APP_BUILD_NUMBER + 1U:
                /* Low word of 32-bit value */
                register_values[i] =
                    (uint16_t)((uint32_t)regs->app_build_number & 0xFFFFU);
                break;
            default:
                return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        }
    }

    return MODBUS_EXCEPTION_NONE;
}

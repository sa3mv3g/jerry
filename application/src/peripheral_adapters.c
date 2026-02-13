/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * Peripheral Adapters - Hardware abstraction layer
 *
 * This module provides implementations that interface with actual hardware
 * through the BSP layer. ADC functions use real filtered values from the
 * BSP_ADC1 subsystem.
 */

#include "peripheral_adapters.h"

#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "bsp.h"
#include "jerry_device_registers.h"
#include "main.h"               // Required for hi2c3 handle and Error_Handler()
#include "stm32h5xx_hal_i2c.h"  // Required for I2C HAL functions
#include "task.h"

/* ==========================================================================
 * Private Data - Simulated Hardware State
 * ========================================================================== */

/** Simulated digital input state (8 bits) */
static uint8_t s_digital_inputs = 0x00U;

/** Simulated digital output state (16 bits) */
static uint16_t s_digital_outputs = 0x0000U;

/** Simulated PWM enable state */
static bool s_pwm_enabled[4] = {false, false, false, false};

/** Simulated PWM duty cycles (0-10000) */
static uint16_t s_pwm_duty_cycle[4] = {5000U, 5000U, 5000U, 5000U};

/** Simulated PWM frequencies (Hz) */
static uint32_t s_pwm_frequency[4] = {1000U, 1000U, 1000U, 1000U};

/** Simulated RTC */
static peripheral_rtc_datetime_t s_rtc = {.year   = 2026U,
                                          .month  = 1U,
                                          .day    = 28U,
                                          .hour   = 12U,
                                          .minute = 0U,
                                          .second = 0U};

/** Application version (compile-time constants) */
static const peripheral_app_version_t s_app_version = {
    .major = 1U, .minor = 0U, .patch = 0U, .build_number = 1U};

/** Counter for simulating changing values */
static uint32_t s_simulation_counter = 0U;

/* ==========================================================================
 * Digital Input Adapter Implementation
 * ========================================================================== */

void peripheral_digital_input_init(void)
{
    /* Initialize with random pattern for testing */
    s_digital_inputs = 0xA5U; /* Alternating pattern */
}

bool peripheral_digital_input_read(uint8_t channel)
{
    bool result = false;

    if (channel < 8U)
    {
        result = ((s_digital_inputs >> channel) & 0x01U) != 0U;
    }

    return result;
}

uint8_t peripheral_digital_input_read_all(void)
{
    /* Simulate changing inputs - toggle bit 0 every 100 calls */
    s_simulation_counter++;
    if ((s_simulation_counter % 100U) == 0U)
    {
        s_digital_inputs ^= 0x01U;
    }

    return s_digital_inputs;
}

/* ==========================================================================
 * Digital Output Adapter Implementation
 * ========================================================================== */

void peripheral_digital_output_init(void)
{
    /*
        all the digital outputs are controlled via
        PCF8574 and PCF8574A, over i2c. I2C has been
        init in BSP_Init(). I2C3 has been used.
        Initialize the I2C Digital Output subsystem.
    */
    if (BSP_I2CDO_init() != BSP_OK)
    {
        Error_Handler();
    }
}

void peripheral_digital_output_write(uint8_t channel, bool value)
{
    if (channel < 16U)
    {
        if (value)
        {
            s_digital_outputs |= (uint16_t)(BSP_I2CDO_CONSTRUCT_MASK(channel));
        }
        else
        {
            s_digital_outputs &= (uint16_t)(~BSP_I2CDO_CONSTRUCT_MASK(channel));
        }

        if (BSP_I2CDO_Write(s_digital_outputs) != BSP_OK)
        {
            Error_Handler();
        }
    }
}

bool peripheral_digital_output_read(uint8_t channel)
{
    bool     result = false;
    uint16_t current_outputs;

    if (channel < 16U)
    {
        if (BSP_I2CDO_Read(&current_outputs) == BSP_OK)
        {
            s_digital_outputs = current_outputs;
            result            = ((s_digital_outputs >> channel) & 0x01U) != 0U;
        }
        else
        {
            Error_Handler();
        }
    }

    return result;
}

void peripheral_digital_output_write_all(uint16_t values)
{
    s_digital_outputs = values;
}

uint16_t peripheral_digital_output_read_all(void) { return s_digital_outputs; }

/* ==========================================================================
 * ADC Adapter Implementation
 * Uses real filtered ADC values from BSP layer
 * ========================================================================== */

void peripheral_adc_init(void)
{
    /* ADC is initialized by BSP_Init() which calls BSP_ADC1_FilterInit()
     * and BSP_ADC1_Start(). Nothing additional needed here. */
}

uint16_t peripheral_adc_read(uint8_t channel)
{
    uint16_t  result = 0U;
    float32_t value  = 0.0f;

    if (channel < 4U)
    {
        /* Return 0 if filter has not settled yet */
        if (!BSP_ADC1_IsFilterSettled())
        {
            return 0U;
        }

        /* Get filtered value from BSP (normalized 0.0-1.0) */
        if (BSP_ADC1_GetFilteredValue(channel, &value) == BSP_OK)
        {
            /* Convert normalized float to 12-bit ADC value (0-4095) */
            result = (uint16_t)(value * 4095.0f);

            /* Clamp to 12-bit range (safety check) */
            if (result > 4095U)
            {
                result = 4095U;
            }
        }
    }

    return result;
}

void peripheral_adc_read_all(uint16_t values[4])
{
    float32_t float_values[BSP_ADC1_NUM_CHANNELS];

    /* Check for ADC errors and restart if needed */
    (void)BSP_ADC1_CheckAndRestart();

    /* Return zeros if filter has not settled yet */
    if (!BSP_ADC1_IsFilterSettled())
    {
        for (uint8_t i = 0U; i < 4U; i++)
        {
            values[i] = 0U;
        }
        return;
    }

    /* Get all filtered values from BSP */
    if (BSP_ADC1_GetFilteredValuesAll(float_values) == BSP_OK)
    {
        /* Convert first 4 channels from normalized float to 12-bit ADC values
         */
        for (uint8_t i = 0U; i < 4U; i++)
        {
            uint16_t adc_value = (uint16_t)(float_values[i] * 4095.0f);

            /* Clamp to 12-bit range (safety check) */
            if (adc_value > 4095U)
            {
                adc_value = 4095U;
            }
            values[i] = adc_value;
        }
    }
    else
    {
        /* BSP call failed, return zeros */
        for (uint8_t i = 0U; i < 4U; i++)
        {
            values[i] = 0U;
        }
    }
}

/* ==========================================================================
 * PWM Adapter Implementation
 * ========================================================================== */

void peripheral_pwm_init(void)
{
    for (uint8_t i = 0U; i < 4U; i++)
    {
        s_pwm_enabled[i]    = false;
        s_pwm_duty_cycle[i] = 5000U; /* 50% */
        s_pwm_frequency[i]  = 1000U; /* 1 kHz */
    }
}

void peripheral_pwm_enable(uint8_t channel, bool enable)
{
    if (channel < 4U)
    {
        s_pwm_enabled[channel] = enable;
    }
}

bool peripheral_pwm_is_enabled(uint8_t channel)
{
    bool result = false;

    if (channel < 4U)
    {
        result = s_pwm_enabled[channel];
    }

    return result;
}

void peripheral_pwm_set_duty_cycle(uint8_t channel, uint16_t duty_cycle)
{
    if (channel < 4U)
    {
        /* Clamp to valid range */
        if (duty_cycle > 10000U)
        {
            duty_cycle = 10000U;
        }
        s_pwm_duty_cycle[channel] = duty_cycle;
    }
}

uint16_t peripheral_pwm_get_duty_cycle(uint8_t channel)
{
    uint16_t result = 0U;

    if (channel < 4U)
    {
        result = s_pwm_duty_cycle[channel];
    }

    return result;
}

void peripheral_pwm_set_frequency(uint8_t channel, uint32_t frequency)
{
    if (channel < 4U)
    {
        /* Clamp to reasonable range (1 Hz to 1 MHz) */
        if (frequency < 1U)
        {
            frequency = 1U;
        }
        else if (frequency > 1000000U)
        {
            frequency = 1000000U;
        }
        s_pwm_frequency[channel] = frequency;
    }
}

uint32_t peripheral_pwm_get_frequency(uint8_t channel)
{
    uint32_t result = 0U;

    if (channel < 4U)
    {
        result = s_pwm_frequency[channel];
    }

    return result;
}

/* ==========================================================================
 * RTC Adapter Implementation
 * ========================================================================== */

void peripheral_rtc_init(void)
{
    /* Initialize with a default date/time */
    s_rtc.year   = 2026U;
    s_rtc.month  = 1U;
    s_rtc.day    = 28U;
    s_rtc.hour   = 12U;
    s_rtc.minute = 0U;
    s_rtc.second = 0U;
}

void peripheral_rtc_get_datetime(peripheral_rtc_datetime_t *datetime)
{
    if (datetime != NULL)
    {
        /* Simulate time passing - increment seconds */
        s_rtc.second++;
        if (s_rtc.second >= 60U)
        {
            s_rtc.second = 0U;
            s_rtc.minute++;
            if (s_rtc.minute >= 60U)
            {
                s_rtc.minute = 0U;
                s_rtc.hour++;
                if (s_rtc.hour >= 24U)
                {
                    s_rtc.hour = 0U;
                    s_rtc.day++;
                    /* Simplified - don't handle month/year rollover */
                    if (s_rtc.day > 28U)
                    {
                        s_rtc.day = 1U;
                    }
                }
            }
        }

        *datetime = s_rtc;
    }
}

void peripheral_rtc_set_datetime(const peripheral_rtc_datetime_t *datetime)
{
    if (datetime != NULL)
    {
        s_rtc = *datetime;
    }
}

/* ==========================================================================
 * System Info Adapter Implementation
 * ========================================================================== */

void peripheral_get_app_version(peripheral_app_version_t *version)
{
    if (version != NULL)
    {
        *version = s_app_version;
    }
}

uint32_t peripheral_get_system_tick(void) { return xTaskGetTickCount(); }

/* ==========================================================================
 * Initialization and Update Functions
 * ========================================================================== */

void peripheral_adapters_init(void)
{
    peripheral_digital_input_init();
    peripheral_digital_output_init();
    peripheral_adc_init();
    peripheral_pwm_init();
    peripheral_rtc_init();
}

void peripheral_adapters_update_registers(void)
{
    /* Get pointers to register structures */
    jerry_device_coils_t             *coils = jerry_device_get_coils();
    jerry_device_discrete_inputs_t   *di = jerry_device_get_discrete_inputs();
    jerry_device_holding_registers_t *hr = jerry_device_get_holding_registers();
    jerry_device_input_registers_t   *ir = jerry_device_get_input_registers();

    /* Update discrete inputs from hardware */
    uint8_t inputs      = peripheral_digital_input_read_all();
    di->digital_input_0 = ((inputs >> 0U) & 0x01U) != 0U;
    di->digital_input_1 = ((inputs >> 1U) & 0x01U) != 0U;
    di->digital_input_2 = ((inputs >> 2U) & 0x01U) != 0U;
    di->digital_input_3 = ((inputs >> 3U) & 0x01U) != 0U;
    di->digital_input_4 = ((inputs >> 4U) & 0x01U) != 0U;
    di->digital_input_5 = ((inputs >> 5U) & 0x01U) != 0U;
    di->digital_input_6 = ((inputs >> 6U) & 0x01U) != 0U;
    di->digital_input_7 = ((inputs >> 7U) & 0x01U) != 0U;

    /* Mirror discrete inputs to coils */
    coils->digital_input_0 = di->digital_input_0;
    coils->digital_input_1 = di->digital_input_1;
    coils->digital_input_2 = di->digital_input_2;
    coils->digital_input_3 = di->digital_input_3;
    coils->digital_input_4 = di->digital_input_4;
    coils->digital_input_5 = di->digital_input_5;
    coils->digital_input_6 = di->digital_input_6;
    coils->digital_input_7 = di->digital_input_7;

    /* Update ADC input registers */
    uint16_t adc_values[4];
    peripheral_adc_read_all(adc_values);
    ir->adc_0_value = adc_values[0];
    ir->adc_1_value = adc_values[1];
    ir->adc_2_value = adc_values[2];
    ir->adc_3_value = adc_values[3];

    /* Mirror ADC values to holding registers */
    hr->adc_0_value = ir->adc_0_value;
    hr->adc_1_value = ir->adc_1_value;
    hr->adc_2_value = ir->adc_2_value;
    hr->adc_3_value = ir->adc_3_value;

    /* Update system tick */
    uint32_t tick        = peripheral_get_system_tick();
    hr->system_tick_low  = (uint16_t)(tick & 0xFFFFU);
    hr->system_tick_high = (uint16_t)((tick >> 16U) & 0xFFFFU);

    /* Update RTC */
    peripheral_rtc_datetime_t rtc;
    peripheral_rtc_get_datetime(&rtc);
    hr->rtc_year   = rtc.year;
    hr->rtc_month  = rtc.month;
    hr->rtc_day    = rtc.day;
    hr->rtc_hour   = rtc.hour;
    hr->rtc_minute = rtc.minute;
    hr->rtc_second = rtc.second;

    /* Update application version */
    peripheral_app_version_t version;
    peripheral_get_app_version(&version);
    ir->app_version_major = version.major;
    ir->app_version_minor = version.minor;
    ir->app_version_patch = version.patch;
    ir->app_build_number  = version.build_number;

    /* Mirror version to holding registers */
    hr->app_version_major = ir->app_version_major;
    hr->app_version_minor = ir->app_version_minor;
    hr->app_version_patch = ir->app_version_patch;
    hr->app_build_number  = ir->app_build_number;

    /* Update PWM enable states from coils */
    coils->pwm_0_enable = peripheral_pwm_is_enabled(0);
    coils->pwm_1_enable = peripheral_pwm_is_enabled(1);
    coils->pwm_2_enable = peripheral_pwm_is_enabled(2);
    coils->pwm_3_enable = peripheral_pwm_is_enabled(3);
}

void peripheral_adapters_apply_outputs(void)
{
    /* Get pointers to register structures */
    jerry_device_coils_t             *coils = jerry_device_get_coils();
    jerry_device_holding_registers_t *hr = jerry_device_get_holding_registers();

    /* Apply digital outputs from coils */
    uint16_t outputs = 0U;
    outputs |= (coils->digital_output_0 ? (1U << 0U) : 0U);
    outputs |= (coils->digital_output_1 ? (1U << 1U) : 0U);
    outputs |= (coils->digital_output_2 ? (1U << 2U) : 0U);
    outputs |= (coils->digital_output_3 ? (1U << 3U) : 0U);
    outputs |= (coils->digital_output_4 ? (1U << 4U) : 0U);
    outputs |= (coils->digital_output_5 ? (1U << 5U) : 0U);
    outputs |= (coils->digital_output_6 ? (1U << 6U) : 0U);
    outputs |= (coils->digital_output_7 ? (1U << 7U) : 0U);
    outputs |= (coils->digital_output_8 ? (1U << 8U) : 0U);
    outputs |= (coils->digital_output_9 ? (1U << 9U) : 0U);
    outputs |= (coils->digital_output_10 ? (1U << 10U) : 0U);
    outputs |= (coils->digital_output_11 ? (1U << 11U) : 0U);
    outputs |= (coils->digital_output_12 ? (1U << 12U) : 0U);
    outputs |= (coils->digital_output_13 ? (1U << 13U) : 0U);
    outputs |= (coils->digital_output_14 ? (1U << 14U) : 0U);
    outputs |= (coils->digital_output_15 ? (1U << 15U) : 0U);
    peripheral_digital_output_write_all(outputs);

    /* Apply PWM enable states */
    peripheral_pwm_enable(0, coils->pwm_0_enable);
    peripheral_pwm_enable(1, coils->pwm_1_enable);
    peripheral_pwm_enable(2, coils->pwm_2_enable);
    peripheral_pwm_enable(3, coils->pwm_3_enable);

    /* Apply PWM duty cycles */
    peripheral_pwm_set_duty_cycle(0, hr->pwm_0_duty_cycle);
    peripheral_pwm_set_duty_cycle(1, hr->pwm_1_duty_cycle);
    peripheral_pwm_set_duty_cycle(2, hr->pwm_2_duty_cycle);
    peripheral_pwm_set_duty_cycle(3, hr->pwm_3_duty_cycle);

    /* Apply PWM frequencies */
    peripheral_pwm_set_frequency(0, hr->pwm_0_frequency);
    peripheral_pwm_set_frequency(1, hr->pwm_1_frequency);
    peripheral_pwm_set_frequency(2, hr->pwm_2_frequency);
    peripheral_pwm_set_frequency(3, hr->pwm_3_frequency);

    /* Apply RTC if changed (would need change detection in real implementation)
     */
    peripheral_rtc_datetime_t rtc = {.year   = hr->rtc_year,
                                     .month  = (uint8_t)hr->rtc_month,
                                     .day    = (uint8_t)hr->rtc_day,
                                     .hour   = (uint8_t)hr->rtc_hour,
                                     .minute = (uint8_t)hr->rtc_minute,
                                     .second = (uint8_t)hr->rtc_second};
    peripheral_rtc_set_datetime(&rtc);
}

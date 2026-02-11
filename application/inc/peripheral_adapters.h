/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * Peripheral Adapters - Hardware abstraction layer for Modbus registers
 *
 * This module provides adapter functions to interface between Modbus registers
 * and actual hardware peripherals. Currently implements stub functions that
 * return simulated values for testing purposes.
 */

#ifndef PERIPHERAL_ADAPTERS_H
#define PERIPHERAL_ADAPTERS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* ==========================================================================
     * Digital Input Adapter
     * ==========================================================================
     */

    /**
     * @brief Initialize digital input adapter
     */
    void peripheral_digital_input_init(void);

    /**
     * @brief Read a digital input pin
     * @param[in] channel Digital input channel (0-7)
     * @return true if input is high, false if low
     */
    bool peripheral_digital_input_read(uint8_t channel);

    /**
     * @brief Read all digital inputs as a bitmask
     * @return 8-bit value with each bit representing an input
     */
    uint8_t peripheral_digital_input_read_all(void);

    /* ==========================================================================
     * Digital Output Adapter
     * ==========================================================================
     */

    /**
     * @brief Initialize digital output adapter
     */
    void peripheral_digital_output_init(void);

    /**
     * @brief Write a digital output pin
     * @param[in] channel Digital output channel (0-15)
     * @param[in] value true for high, false for low
     */
    void peripheral_digital_output_write(uint8_t channel, bool value);

    /**
     * @brief Read back a digital output pin state
     * @param[in] channel Digital output channel (0-15)
     * @return Current output state
     */
    bool peripheral_digital_output_read(uint8_t channel);

    /**
     * @brief Write all digital outputs from a bitmask
     * @param[in] values 16-bit value with each bit representing an output
     */
    void peripheral_digital_output_write_all(uint16_t values);

    /**
     * @brief Read all digital outputs as a bitmask
     * @return 16-bit value with each bit representing an output
     */
    uint16_t peripheral_digital_output_read_all(void);

    /* ==========================================================================
     * ADC Adapter
     * ==========================================================================
     */

    /**
     * @brief Initialize ADC adapter
     */
    void peripheral_adc_init(void);

    /**
     * @brief Read ADC channel value
     * @param[in] channel ADC channel (0-3)
     * @return 12-bit ADC value (0-4095)
     */
    uint16_t peripheral_adc_read(uint8_t channel);

    /**
     * @brief Read all ADC channels
     * @param[out] values Array to store 4 ADC values
     */
    void peripheral_adc_read_all(uint16_t values[4]);

    /* ==========================================================================
     * PWM Adapter
     * ==========================================================================
     */

    /**
     * @brief Initialize PWM adapter
     */
    void peripheral_pwm_init(void);

    /**
     * @brief Enable/disable PWM channel
     * @param[in] channel PWM channel (0-3)
     * @param[in] enable true to enable, false to disable
     */
    void peripheral_pwm_enable(uint8_t channel, bool enable);

    /**
     * @brief Check if PWM channel is enabled
     * @param[in] channel PWM channel (0-3)
     * @return true if enabled, false if disabled
     */
    bool peripheral_pwm_is_enabled(uint8_t channel);

    /**
     * @brief Set PWM duty cycle
     * @param[in] channel PWM channel (0-3)
     * @param[in] duty_cycle Duty cycle in 0.01% units (0-10000 = 0.00-100.00%)
     */
    void peripheral_pwm_set_duty_cycle(uint8_t channel, uint16_t duty_cycle);

    /**
     * @brief Get PWM duty cycle
     * @param[in] channel PWM channel (0-3)
     * @return Duty cycle in 0.01% units (0-10000)
     */
    uint16_t peripheral_pwm_get_duty_cycle(uint8_t channel);

    /**
     * @brief Set PWM frequency
     * @param[in] channel PWM channel (0-3)
     * @param[in] frequency Frequency in Hz
     */
    void peripheral_pwm_set_frequency(uint8_t channel, uint32_t frequency);

    /**
     * @brief Get PWM frequency
     * @param[in] channel PWM channel (0-3)
     * @return Frequency in Hz
     */
    uint32_t peripheral_pwm_get_frequency(uint8_t channel);

    /* ==========================================================================
     * RTC Adapter
     * ==========================================================================
     */

    /**
     * @brief RTC date/time structure
     */
    typedef struct
    {
        uint16_t year;   /**< Year (2000-2099) */
        uint8_t  month;  /**< Month (1-12) */
        uint8_t  day;    /**< Day of month (1-31) */
        uint8_t  hour;   /**< Hour (0-23) */
        uint8_t  minute; /**< Minute (0-59) */
        uint8_t  second; /**< Second (0-59) */
    } peripheral_rtc_datetime_t;

    /**
     * @brief Initialize RTC adapter
     */
    void peripheral_rtc_init(void);

    /**
     * @brief Get current date/time
     * @param[out] datetime Pointer to datetime structure to fill
     */
    void peripheral_rtc_get_datetime(peripheral_rtc_datetime_t *datetime);

    /**
     * @brief Set date/time
     * @param[in] datetime Pointer to datetime structure with new values
     */
    void peripheral_rtc_set_datetime(const peripheral_rtc_datetime_t *datetime);

    /* ==========================================================================
     * System Info Adapter
     * ==========================================================================
     */

    /**
     * @brief Application version structure
     */
    typedef struct
    {
        uint16_t major;        /**< Major version */
        uint16_t minor;        /**< Minor version */
        uint16_t patch;        /**< Patch version */
        uint32_t build_number; /**< Build number */
    } peripheral_app_version_t;

    /**
     * @brief Get application version
     * @param[out] version Pointer to version structure to fill
     */
    void peripheral_get_app_version(peripheral_app_version_t *version);

    /**
     * @brief Get system tick count
     * @return 32-bit system tick count
     */
    uint32_t peripheral_get_system_tick(void);

    /* ==========================================================================
     * Initialization
     * ==========================================================================
     */

    /**
     * @brief Initialize all peripheral adapters
     */
    void peripheral_adapters_init(void);

    /**
     * @brief Update Modbus registers from peripheral values
     *
     * This function should be called periodically to sync hardware
     * state with Modbus register values.
     */
    void peripheral_adapters_update_registers(void);

    /**
     * @brief Apply Modbus register values to peripherals
     *
     * This function should be called after Modbus writes to apply
     * the new values to hardware.
     */
    void peripheral_adapters_apply_outputs(void);

#ifdef __cplusplus
}
#endif

#endif /* PERIPHERAL_ADAPTERS_H */

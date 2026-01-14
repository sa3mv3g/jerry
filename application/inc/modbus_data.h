/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 */

#ifndef MODBUS_DATA_H
#define MODBUS_DATA_H

#include <stdint.h>

typedef struct {
    uint8_t  u8DigitalInputs;   /* 8 digital input pins */
    uint16_t u16DigitalOutputs; /* 16 digital output pins */
    uint16_t u16AdcValues[4];   /* 4 ADC input values */
    struct {
        uint8_t  u8DutyCycle;  /* PWM Duty Cycle */
        uint32_t u32Frequency; /* PWM Frequency */
    } sPwmOutputs[4];          /* 4 PWM output pins */
} ModbusData_t;

/* Global instance or accessor would go here */

#endif /* MODBUS_DATA_H */

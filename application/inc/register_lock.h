/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#ifndef REGISTER_LOCK_H
#define REGISTER_LOCK_H

#include "FreeRTOS.h"

/**
 * @file register_lock.h
 * @brief Mutual exclusion for the shared Modbus register data.
 *
 * The generated register storage (holding registers, input registers, coils,
 * discrete inputs) is accessed from more than one task — the Modbus server
 * task (read/write via callbacks) and the monitor task (reads). The structs
 * mix 16-bit and 32-bit fields, so concurrent access can tear 32-bit values
 * (e.g. PWM frequency, build number, float calibration values) on the
 * Cortex-M33, which does not guarantee atomic access to misaligned words.
 *
 * These functions provide a single FreeRTOS mutex that guards every access to
 * the shared register data. The lock is held only for the short duration of a
 * register access; do not perform blocking I/O while holding it.
 */

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Create the register-data mutex.
     *
     * Uses static allocation, so it is safe to call before the scheduler
     * starts. Must be called once during initialization, before any task
     * accesses the shared register data.
     */
    void RegisterLock_Init(void);

    /**
     * @brief Acquire the register-data mutex (blocks until available).
     */
    void RegisterLock_Acquire(void);

    /**
     * @brief Release the register-data mutex.
     */
    void RegisterLock_Release(void);

#ifdef __cplusplus
}
#endif

#endif /* REGISTER_LOCK_H */

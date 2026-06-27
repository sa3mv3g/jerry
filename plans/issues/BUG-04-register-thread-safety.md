# [BUG-04] Shared Modbus register data accessed from multiple tasks without synchronization

**Severity:** 🔴 Critical (data corruption)
**Component:** Modbus / RTOS / shared state
**Files:**
- `application/src/modbus_task.c`
- `application/src/modbus_device_callbacks.c`
- `application/src/monitor_task.c` (lines ~184-188)
- `application/src/main.c`

## Description

The register data structures (`s_holding_registers`, `s_coils`, etc.) are accessed from multiple tasks with **no mutex or critical section**:

1. **Modbus task** — reads/writes via callbacks.
2. **Monitor task** — reads `jerry_device_get_holding_registers()` for PWM duty-cycle values.
3. (Indirectly) other tasks via the LCD manager.

The `jerry_device_holding_registers_t` struct mixes `uint16_t` with `uint32_t`/`float` fields, so 32-bit fields may be torn on concurrent access (the Cortex-M33 does not guarantee atomic access to misaligned 32-bit fields).

## Impact

Torn reads/writes of 32-bit fields (`pwm_x_frequency`, `app_build_number`, float calibration values), producing corrupt values intermittently.

## Fix

- Add a FreeRTOS mutex around register access, or use `taskENTER_CRITICAL()`/`taskEXIT_CRITICAL()` for short accesses.
- Coordinate with BUG-19 (the float snapshot should occur under the same lock).
- Coordinate with N-01 (moving all LCD writes to the Main task removes one class of concurrent readers).

# [BUG-07] `modbus_cb_read_coils()` does not sync `coils->digital_output_x` struct fields

**Severity:** 🟠 High
**Component:** Modbus / device callbacks
**File:** `application/src/modbus_device_callbacks.c` (lines ~248-355 vs ~360-455)

## Description

For digital outputs (coils 0-15), the read-coils callback reads from `DigitalOutput_GetChannel()` (the shadow register) into a local `value`, but does **not** update the `coils->digital_output_x` struct field — so that field is stale.

For digital inputs (coils 16-23), the struct field **is** updated via `update_digital_input()`.

`modbus_cb_write_single_coil()` updates the struct, but `modbus_cb_read_coils()` does not, so any code reading `coils->digital_output_x` directly gets stale data.

## Impact

Stale `coils->digital_output_x` values if read directly (rather than through `DigitalOutput_GetChannel()`).

## Fix

After each `DigitalOutput_GetChannel()` call in `modbus_cb_read_coils()`, assign the result back:

```c
coils->digital_output_x = value;
```

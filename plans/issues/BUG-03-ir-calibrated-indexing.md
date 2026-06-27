# [BUG-03] Calibrated ADC input-register reads return garbage — wrong array index in `modbus_cb_read_input_registers()`

**Severity:** 🔴 Critical (data corruption)
**Component:** Modbus / device callbacks
**File:** `application/src/modbus_device_callbacks.c` (lines ~1473-1503)

## Description

Same root cause as BUG-02, in the input-register read path. `update_calibrated_adcval()` is given the **absolute** Modbus address as the destination index instead of the relative response position `i`:

```c
case JERRY_DEVICE_IR_ADC_0_CALIBRATED_VALUE:
case JERRY_DEVICE_IR_ADC_0_CALIBRATED_VALUE + 1:
    update_calibrated_adcval(...,
        &register_values[JERRY_DEVICE_IR_ADC_0_CALIBRATED_VALUE]); /* index 4, should be i */
    break;
```

The response encoder ships back `register_values[0 .. quantity-1]`, so data written at absolute index 4/5 is never sent.

## Impact

All calibrated ADC value reads return garbage data (same "right value, wrong mailbox" effect as BUG-02).

## Fix

- Use `&register_values[i]` and handle the two-register float span correctly (as in BUG-02).
- This callback additionally **re-samples the ADC** via `update_calibrated_adcval()` inside each case — fix together with BUG-19 (snapshot once at callback entry).

## Related

- BUG-02 (same pattern in holding registers)
- BUG-19 (non-atomic float reads)

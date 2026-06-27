# [BUG-02] Holding-register float reads return garbage — wrong array index in `modbus_cb_read_holding_registers()`

**Severity:** 🔴 Critical (data corruption)
**Component:** Modbus / device callbacks
**File:** `application/src/modbus_device_callbacks.c` (lines ~1004-1073)

## Description

When reading 32-bit float calibration registers (e.g. `JERRY_DEVICE_HR_ADC_0_SCALE_FACTOR` = 104), the callback writes the result to the **absolute** Modbus address index instead of the **relative** response position `i`:

```c
case JERRY_DEVICE_HR_ADC_0_SCALE_FACTOR:
case JERRY_DEVICE_HR_ADC_0_SCALE_FACTOR + 1U:
    f32_to_u16(regs->adc_0_scale_factor,
               &register_values[JERRY_DEVICE_HR_ADC_0_SCALE_FACTOR]); /* index 104, wrong */
    break;
```

The Modbus response encoder reads `register_values[0 .. quantity-1]`. With `start=104, quantity=2`, the data is written to `register_values[104]/[105]` but the encoder ships back `register_values[0]/[1]`, which were never written.

**"Reading both registers" does not rescue this** — both the `base` and `base+1` cases write the whole float to the absolute base index, so the response is garbage regardless of read pattern. (In plain terms: right value, wrong mailbox.)

## Impact

All float calibration holding-register reads via Modbus return garbage / leftover memory instead of the real values.

## Fix

- Change all `f32_to_u16()` calls to use `&register_values[i]` instead of `&register_values[JERRY_DEVICE_HR_ADC_x_...]`.
- For the two-register float span: when `addr == base_addr`, write both u16 words at `register_values[i]`/`register_values[i+1]` (with bounds check); when `addr == base_addr + 1`, only write the second word.
- Prefer snapshotting the float once at callback entry (see BUG-19) so it is not recomputed per case.

## Related

- BUG-03 (same pattern in input registers)
- BUG-19 (non-atomic float reads — fix together via snapshot-at-entry)

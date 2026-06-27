# [N-02] Boot-time coil restore reinterprets a `uint16_t` as `uint8_t*` — endianness/aliasing bug

**Severity:** 🔴 Critical
**Component:** Modbus task / digital outputs
**File:** `application/src/modbus_task.c` (~197-203)

## Description

```c
initDigitalOutputCoilsValues = 0x0;
if (BSP_OK == BSP_I2CDO_Read(&initDigitalOutputCoilsValues))
{
    modbus_cb_write_multiple_coils(
        0, 16, (const uint8_t *)&initDigitalOutputCoilsValues);
}
```

A `uint16_t` is reinterpreted as the packed-coil byte array for 16 coils, baking in an undocumented little-endian assumption (coils 0-7 = low byte, 8-15 = high byte). If the coil packing / `BSP_I2CDO_Read` bit layout doesn't match, the restored output state is wrong.

## Impact

Digital outputs may be restored to the wrong pattern on boot. Also a fragile aliasing/MISRA concern.

## Fix

Build an explicit 2-byte array with documented bit ordering:

```c
uint8_t coils[2] = { (uint8_t)(val & 0xFFU), (uint8_t)(val >> 8) };
modbus_cb_write_multiple_coils(0, 16, coils);
```

Add a comment documenting the coil-to-channel mapping.

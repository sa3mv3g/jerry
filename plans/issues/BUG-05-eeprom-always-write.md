# [BUG-05] `update_calibration()` compares floats with `==` — replace with unconditional write

**Severity:** 🟠 High
**Component:** Modbus / EEPROM calibration
**File:** `application/src/modbus_device_callbacks.c` (lines ~196-209)

## Description

```c
static inline bsp_error_t update_calibration(uint32_t address, float newValue)
{
    float       oldVal = 0.0f;
    bsp_error_t err    = BSP_EEPROM_Read(address, (uint8_t *)&oldVal, sizeof(oldVal));
    if (err == BSP_OK && oldVal == newValue)  /* unreliable float compare */
    {
        return BSP_OK;
    }
    return BSP_EEPROM_Write(address, (uint8_t *)&newValue, sizeof(newValue));
}
```

Float `==` is unreliable (NaN/denormal/bit-pattern differences), so the redundant-write skip may misfire in both directions.

## Decision

**Remove the read-back-and-compare entirely and always write.** Calibration is an infrequent, master-initiated operation, so the skip gave negligible benefit while adding an unreliable comparison and an extra EEPROM read.

## Fix

```c
static inline bsp_error_t update_calibration(uint32_t address, float newValue)
{
    return BSP_EEPROM_Write(address, (uint8_t *)&newValue, sizeof(newValue));
}
```

## Trade-off (acceptable)

Every calibration update now hits the EEPROM even if unchanged. On flash-emulated EEPROM a write may trigger a page erase/relocation — fine given the low write frequency. Do **not** reintroduce a naive `==`; if write-suppression is ever needed, compare raw bytes via `memcmp()`.

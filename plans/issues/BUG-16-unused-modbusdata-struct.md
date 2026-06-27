# [BUG-16] `modbus_data.h` defines an unused `ModbusData_t` struct

**Severity:** 🔵 Low (dead code)
**Component:** Modbus data header
**File:** `application/inc/modbus_data.h`

## Description

`ModbusData_t` appears to be a legacy/placeholder superseded by the code-generated `jerry_device_registers.h` structures, and is never used.

## Impact

Dead code; potential confusion for readers.

## Fix

Remove the unused struct / header if nothing references it (verify with a search first).

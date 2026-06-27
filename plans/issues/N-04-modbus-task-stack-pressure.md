# [N-04] Large per-case stack buffers in `modbus_process_request()` pressure the Modbus task stack

**Severity:** 🟠 High
**Component:** Modbus task
**File:** `application/src/modbus_task.c` (~399, 421, 443, 465, 550); stack size at `main.c:34`

## Description

Switch-case branches declare large local arrays: `uint8_t coil_values[256]`, `uint8_t input_values[256]`, `uint16_t register_values[125]` (×2), `uint16_t values[123]`. Combined with three `static` ADU/PDU structs, the callback call depth, and lwIP `netconn_*` usage, **1024 words (4 KB)** can be tight. The Modbus task is not in the stack high-water-mark monitor.

## Impact

Potential stack overflow on the Modbus task under deep call chains (which then triggers the buggy stack-overflow hook — see BUG-08).

## Fix

- Make the large per-case buffers `static` (server is single-threaded).
- Add the Modbus task handle to the stack-usage monitor.

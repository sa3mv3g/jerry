# [BUG-13] `end_address` underflows when `quantity == 0`, bypassing address-range validation

**Severity:** 🟡 Medium
**Component:** Modbus / device callbacks + PDU decode
**File:** `application/src/modbus_device_callbacks.c` (lines ~222, 697, 840, 1445); `modbus_pdu.c` (decode functions)

## Description

```c
uint16_t end_address = start_address + quantity - 1U;
```

If `quantity == 0` (the PDU decoder does not reject it), `end_address` wraps to `0xFFFF`, bypassing the range check. The PDU decoder validates `quantity > 0` for *encode* but not *decode*.

## Impact

A malformed request with `quantity=0` can pass address validation incorrectly (benign zero-iteration loop today, but unexpected behavior in general).

## Fix

- Add `quantity == 0` validation at the start of each callback, **or**
- Reject `quantity == 0` in the PDU decode functions in `modbus_pdu.c`.

# [N-16] Inconsistent task-priority casts (`tskIDLE_PRIORITY + N` without `(UBaseType_t)`)

**Severity:** 🔵 Low (MISRA signedness consistency)
**Component:** Main task
**File:** `application/src/main.c` (~438-459)

## Description

The Main task uses `(UBaseType_t)(tskIDLE_PRIORITY + 1U)`, but the child tasks use bare `tskIDLE_PRIORITY + 1` / `+ 2` (signed int arithmetic). Minor MISRA 10.x signedness inconsistency.

## Fix

Use consistent unsigned casts for all `xTaskCreateStatic` priority arguments.

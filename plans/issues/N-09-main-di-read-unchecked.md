# [N-09] `vMainTask` digital-input loop ignores the `BSP_GPIODI_Read()` return value

**Severity:** 🟡 Medium
**Component:** Main task
**File:** `application/src/main.c` (~472-490)

## Description

```c
BSP_GPIODI_Read(digitalInputChannelIndex, &digitalInputValue);
if (digitalInputValue == GPIO_PIN_SET) { ... }
```

The return code is discarded. On a failed read, `digitalInputValue` is stale/uninitialized but still used as a valid level.

## Impact

A failed DI read pushes a wrong state to the LCD.

## Fix

Check the return value; skip the LCD update on error. (Implement alongside N-01, which makes the Main task the sole DI/LCD owner.)

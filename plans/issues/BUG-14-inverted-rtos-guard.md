# [BUG-14] `logging_port.c` has an inverted `#ifndef BSP_USING_RTOS` guard

**Severity:** 🟡 Medium
**Component:** Logging port
**File:** `application/src/logging_port.c` (lines ~7-9, ~39-43)

## Description

```c
#ifndef BSP_USING_RTOS
#include "FreeRTOS.h"
#include "task.h"
#endif
```

The guard includes FreeRTOS headers only when RTOS is **not** defined — backwards. Worse, the fallback path uses `xTaskGetTickCount()` inside `#ifdef BSP_USING_RTOS`, but the headers are only included in the `#ifndef` branch.

## Impact

- If `BSP_USING_RTOS` is defined: headers not included but `xTaskGetTickCount()` is called → compile error.
- If not defined (current state): headers included; fallback uses `ticks = 0` (dead code).

## Fix

Change `#ifndef` to `#ifdef`, or remove the guard entirely since this is always an RTOS project.

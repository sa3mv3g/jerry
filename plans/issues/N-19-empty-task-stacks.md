# [N-19] Empty stub tasks `vLoggingTask` / `vFotaTask` reserve full task stacks

**Severity:** 🔵 Low (RAM waste)
**Component:** Logging task / FOTA task
**File:** `application/src/logging_task.c`, `application/src/fota_task.c`

## Description

Both tasks only `vTaskDelay` in a loop, yet each reserves 512 words of static stack (`LOG_TASK_STACK_SIZE`, `FOTA_TASK_STACK_SIZE`) — ~4 KB of RAM for no functional work. (`fota_task.c` also has the dead `#define CONST 10000` — see BUG-15.)

## Impact

Wasted RAM on a memory-constrained MCU.

## Fix

Reduce their stack sizes to `configMINIMAL_STACK_SIZE` until they do real work, or remove the tasks.

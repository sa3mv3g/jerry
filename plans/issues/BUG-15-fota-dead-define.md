# [BUG-15] `fota_task.c` has unused `#define CONST 10000`

**Severity:** 🔵 Low (dead code)
**Component:** FOTA task
**File:** `application/src/fota_task.c` (line ~10)

## Description

`#define CONST 10000` is unused.

## Fix

Remove the unused macro.

# [BUG-09] LCD manager binary semaphore can lose updates

**Severity:** 🟠 High
**Component:** LCD manager / RTOS
**File:** `application/src/lcd_manager.c` (lines ~28-29, 59-63, 167-176)

## Description

The LCD update uses a **binary** semaphore (`gUdpateLcdSem`). Each `update_row_x()` gives the semaphore. If an update happens **during** the LCD write loop, the give may be consumed by the in-progress iteration, and the new update is lost until the next trigger (binary semaphore saturates at 1).

## Impact

Occasional missed LCD updates → stale display until the next periodic refresh.

## Fix

- Use a counting semaphore, **or**
- Add a per-row "dirty" flag checked atomically, so a row updated during a write cycle is re-rendered on the next pass.

## Related

- N-01 (single LCD writer = Main task) reduces concurrent producers but does not by itself fix the give/consume race during the write loop.

# [BUG-17] `LcdManager_IsLcdReady()` passes a `SemaphoreHandle_t*` to `xSemaphoreGive()` instead of dereferencing

**Severity:** 🔵 Low severity in original report, but **potential crash** — consider prioritizing
**Component:** LCD manager / RTOS
**File:** `application/src/lcd_manager.c` (lines ~185-191)

## Description

```c
void LcdManager_IsLcdReady(SemaphoreHandle_t* isLcdReadySem)
{
    if ((NULL != isLcdReadySem) && (NULL != gUdpateLcdSem))
    {
        xSemaphoreGive(isLcdReadySem);  /* should be *isLcdReadySem */
    }
}
```

`xSemaphoreGive()` expects a `SemaphoreHandle_t`, but `isLcdReadySem` is a pointer to a handle. FreeRTOS will interpret the pointer-to-handle as a queue handle.

## Impact

Memory corruption or crash when this function is called.

## Fix

```c
xSemaphoreGive(*isLcdReadySem);
```

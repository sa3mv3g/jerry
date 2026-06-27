# [N-07] `RTC_Manager_GetTimeWithMs()` underflows when `subseconds > second_fraction`

**Severity:** 🟠 High
**Component:** RTC manager
**File:** `application/src/rtc_manager.c` (~30-35)

## Description

```c
if (pTimeDate->second_fraction > 0)
{
    uint32_t ms = ((pTimeDate->second_fraction - pTimeDate->subseconds) * 1000U)
                  / (pTimeDate->second_fraction + 1U);
    return ms;
}
```

If `subseconds > second_fraction` (transiently across a second rollover, or a stale read between the time and subsecond registers), the unsigned subtraction wraps to a huge value, so `ms` is garbage instead of `[0, 999]`.

## Impact

Log timestamps occasionally show a garbage millisecond field.

## Fix

Clamp: if `subseconds > second_fraction`, treat ms as 0; or latch time + subseconds atomically using the documented shadow-register read sequence.

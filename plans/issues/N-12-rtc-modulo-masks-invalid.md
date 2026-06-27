# [N-12] `gRtc* % 100U` masks invalid RTC values instead of validating

**Severity:** 🟡 Medium
**Component:** LCD manager
**File:** `application/src/lcd_manager.c` (~54-56)

## Description

```c
snprintf(temp, ..., gRtcHours % 100U, gRtcMinutes % 100U, gRtcSeconds % 100U);
```

Applying `% 100` hides out-of-range RTC reads (e.g. hours=200 displays as `00`) rather than surfacing the error. Valid ranges are hours `< 24`, minutes/seconds `< 60`.

## Impact

Invalid RTC values are silently masked into plausible-but-wrong times.

## Fix

Validate ranges; display a sentinel (e.g. `--:--:--`) on invalid reads.

# [N-08] `format_ai()` can exceed the LCD field width for negative / NaN / Inf values

**Severity:** 🟠 High
**Component:** LCD manager
**File:** `application/src/lcd_manager.c` (~79-96, 98-120)

## Description

`format_ai()` tries `%.3f`→`%.2f`→`%.1f`→`%.0f` until ≤ 9 chars, then right-justifies into a 10-byte buffer with `%-9.9s`. For negative large values, NaN, or Inf, `%.0f` can still produce strings the ladder cannot shrink (e.g. `-2147483648`, `-inf`, `nan`), so `%-9.9s` truncates silently and the displayed reading is wrong. Inputs are millivolts (`adc * 3300.0f`), so a negative calibration offset pushes past the field.

## Impact

Misleading analog readings on the LCD (no crash).

## Fix

Clamp/round to the display range and handle NaN/Inf before formatting; choose a fixed format that always fits. (See also N-18 for the magic-number `9`/`10` widths.)

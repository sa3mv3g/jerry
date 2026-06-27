# [N-18] `format_ai()` duplicates magic numbers `9` / `10` for the LCD field width

**Severity:** 🔵 Low (maintainability)
**Component:** LCD manager
**File:** `application/src/lcd_manager.c` (~79-96)

## Description

The width `9` and buffer size `10` are magic numbers duplicated across the function (`len > 9`, `%-9.9s`, `snprintf(buf, 10, ...)`). A mismatch would silently truncate.

## Fix

Define a single `LCD_AI_FIELD_WIDTH` macro and derive the buffer size from it. (Fix alongside N-08.)

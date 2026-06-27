# [BUG-18] Typo in semaphore variable name `gUdpateLcdSem` (should be `gUpdateLcdSem`)

**Severity:** 🔵 Low (readability)
**Component:** LCD manager
**File:** `application/src/lcd_manager.c` (lines ~28-29 and all usages)

## Description

`gUdpateLcdSem` is a misspelling of `gUpdateLcdSem`. No functional impact, but it hurts readability/searchability.

## Fix

Rename `gUdpateLcdSem` → `gUpdateLcdSem` (and `gUdpateLcdSemBuff` → `gUpdateLcdSemBuff`) consistently across `lcd_manager.c`.

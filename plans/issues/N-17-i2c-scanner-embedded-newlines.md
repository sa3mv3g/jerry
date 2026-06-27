# [N-17] `i2c_scanner.c` log lines embed `\n` while the logging port already adds per-call timestamps

**Severity:** 🔵 Low (log cosmetics)
**Component:** I2C scanner / logging
**File:** `application/src/i2c_scanner.c` (throughout)

## Description

Every `LOG_INF("...\n")` includes a trailing `\n`, but `log_hook_with_timestamp()` prepends a timestamp on every call. The result is timestamp + line + newline, and the blank `LOG_INF("\n")` calls produce timestamp-only blank lines.

## Fix

Drop embedded `\n`; rely on the logging framework's line handling.

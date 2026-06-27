# [N-10] DI read error sentinel `0xFF` is logged as `255`, indistinguishable from a valid level

**Severity:** 🟡 Medium (diagnostics clarity)
**Component:** Monitor task
**File:** `application/src/monitor_task.c` (~150-163)

## Description

On read failure, `di_values[i] = 0xFFU` is set as an error marker, but the subsequent `LOG_INF` prints it as `DIx=255`, which a human may misread as a valid high reading. There is no explicit error token.

## Impact

Minor — error reads look like valid `255` in logs.

## Fix

Log an explicit `ERR` token for the sentinel value.

> Note: if N-01 removes DI handling from the Monitor task entirely, fold this into the Main task's DI logging instead.

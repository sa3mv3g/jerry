# [N-15] NETCONN pool error logged at `LOG_INF` instead of `LOG_ERR`

**Severity:** 🔵 Low
**Component:** Monitor task
**File:** `application/src/monitor_task.c` (~87-93)

## Description

The NETCONN pool error branch in `check_lwip_errors()` uses `LOG_INF`, while every other pool error uses `LOG_ERR`. In non-debug builds the log level is `LOG_LEVEL_WARNING`, so a real NETCONN pool error is hidden.

## Fix

Use `LOG_ERR` for the NETCONN pool error, consistent with the other pools.

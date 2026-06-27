# [BUG-11] `modbus_tcp_server_thread()` dereferences `lwip_stats.memp[MEMP_NETCONN]` without a NULL check

**Severity:** 🟡 Medium
**Component:** Modbus / lwIP stats
**File:** `application/src/modbus_task.c` (line ~231)

## Description

```c
LOG_INF("[Modbus]: Available netconns: %d",
        lwip_stats.memp[MEMP_NETCONN]->avail);
```

Unlike all other `lwip_stats.memp[]` accesses in the codebase (which NULL-check first), this line dereferences unconditionally.

## Impact

Hard fault if `MEMP_STATS` is disabled or the pool pointer is NULL.

## Fix

Add a NULL check before dereferencing, consistent with the other `lwip_stats.memp[]` accesses.

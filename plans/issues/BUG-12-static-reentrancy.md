# [BUG-12] `modbus_process_request()` uses `static` locals — latent defect if multi-connection handling is added

**Severity:** 🟡 Medium (latent)
**Component:** Modbus task
**File:** `application/src/modbus_task.c` (lines ~368-370)

## Description

```c
static modbus_adu_t request_adu;
static modbus_adu_t response_adu;
static modbus_pdu_t response_pdu;
```

These are `static` to avoid large stack allocations. Safe today because the server handles one connection at a time — but `MODBUS_MAX_CONNECTIONS 4U` and `s_connections[]` suggest planned multi-connection support, which would corrupt these shared statics.

## Impact

No current bug; data corruption if multi-threaded connection handling is later added.

## Fix

- Document the single-threaded assumption clearly at the declaration, **or**
- Allocate per-connection buffers if/when concurrency is introduced.

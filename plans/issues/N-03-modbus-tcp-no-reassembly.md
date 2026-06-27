# [N-03] Modbus TCP frames are truncated / not reassembled in `modbus_handle_connection()`

**Severity:** 🔴 Critical (functional correctness over the network)
**Component:** Modbus TCP server
**File:** `application/src/modbus_task.c` (~310-320)

## Description

```c
uint16_t copy_len = (len > sizeof(s_rx_buffer)) ? sizeof(s_rx_buffer) : len;
(void)memcpy(s_rx_buffer, data, copy_len);
modbus_err = modbus_process_request(s_rx_buffer, copy_len, ...);
```

Two problems:

1. **Silent truncation** — oversized chunks are truncated and then parsed as if complete.
2. **No fragment/stream reassembly** — `netbuf_data()` returns only the first fragment; the code never walks the chain with `netbuf_next()` (unlike the TCP echo server). A Modbus ADU spanning multiple pbufs, or split across `netconn_recv()` calls, is processed with missing tail bytes.

## Impact

Malformed/fragmented Modbus TCP requests can be mis-parsed → wrong register/coil operations or parse errors. This is the most functionally risky finding for a networked Modbus device.

## Fix

- Walk the netbuf chain via `netbuf_next()` and accumulate.
- Reassemble based on the MBAP-declared length; do not process until the full ADU is present.
- **Reject** (not truncate) frames exceeding `MODBUS_TCP_MAX_ADU_SIZE`.

# 2. Zero-Allocation Syslog UDP Implementation

Date: 2026-07-11

## Status

Accepted

## Context

We need to send internal FreeRTOS task logs out over UDP (RFC 5424 Syslog format, Port 514) to a Host PC for Network Debugging via Wireshark. The project enforces a strict Zero Dynamic Memory Allocation policy (`_MALLOC = 0`). This forbids the use of `malloc`, `free`, dynamic FreeRTOS queues, and lwIP's `netbuf_alloc` if it relies on heap allocation (which `PBUF_RAM` does).

Because multiple tasks concurrently write logs via `LOG_INF()`, passing these safely to a Network Syslog Task requires a thread-safe mechanism. Additionally, sending UDP payloads without allocation means we must use `netbuf_ref()` to point lwIP to a static buffer. However, STM32H5 Ethernet DMA asynchronously reads this buffer; if we reuse a single static buffer for the next log immediately, we risk overwriting data mid-DMA transfer.

## Decision

1. **Serialize Logging via a Mutexed MessageBuffer**: We will use a statically allocated FreeRTOS `MessageBuffer` to transport logs from the `logging_port` hook to the `SyslogTask`. To support concurrent writers, access to the `MessageBuffer` is protected by a statically allocated Mutex. If the Mutex is contested (timeout=0), we will *silently drop* the log rather than block critical tasks. We explicitly forbid Syslog forwarding from ISRs.
2. **Double-Buffering for Zero-Copy DMA**: Inside `SyslogTask`, we will use two static character arrays (`char syslog_buf[2][256]`) to hold the formatted RFC 5424 string. We will alternate between them on each transmission. This ensures the buffer pointed to by `netbuf_ref()` remains valid and unmodified until the Ethernet DMA safely completes the previous asynchronous transfer.

## Consequences

*   **Positive**: Completely avoids dynamic memory allocation on the heap.
*   **Positive**: Prevents race conditions with the Ethernet DMA controller.
*   **Positive**: Prevents logging latency from blocking high-priority control tasks.
*   **Negative**: Logs may be dropped under extremely high contention.
*   **Negative**: ISRs are strictly excluded from network logging.
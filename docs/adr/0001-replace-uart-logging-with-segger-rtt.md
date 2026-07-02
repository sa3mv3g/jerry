# ADR 0001: Replace UART Logging with SEGGER RTT

**Status:** Accepted
**Date:** 2026-07-02

## Context
The system currently uses UART (USART1) as the backend transport for the `embedded-log` library. While UART is universally accessible without specialized debug hardware, it is slow and blocking, which can impact real-time performance, especially when logging heavily from multiple FreeRTOS tasks.

We need a faster logging transport mechanism that minimizes overhead.

## Decision
We will completely replace the UART backend in `logging_port.c` with SEGGER RTT (Real Time Transfer). 
- We will integrate the SEGGER RTT source code via CMake `FetchContent` (Tag: `V8.58.0`) from `https://github.com/SEGGERMicro/RTT`.
- We will configure RTT to use `SEGGER_RTT_MODE_NO_BLOCK_SKIP` so that the application does not block if the host debugger is disconnected or slow to read the logs.
- The UART logging code will be completely removed rather than kept as a fallback, simplifying the codebase.
- We will use an increased up-buffer size (1024 or 2048 bytes) in our custom `SEGGER_RTT_Conf.h` to minimize dropped messages.

## Consequences
**Positive:**
- Logging overhead will be drastically reduced, running at memory speed.
- UART1 is now freed up and can be repurposed if needed.
- No changes required to application code that already calls `LOG_INF`, `LOG_ERR`, etc., since `embedded-log` remains the frontend.

**Negative:**
- Viewing logs now strictly requires a J-Link debugger and the RTT Viewer software. Tapping a UART pin on a bare board is no longer sufficient to see diagnostic output.
- Logs will be dropped silently if the host debugger is not reading the RTT buffer fast enough.
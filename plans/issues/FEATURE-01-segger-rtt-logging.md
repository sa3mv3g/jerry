# [FEATURE-01] Port embedded-log backend to SEGGER RTT

**Severity:** Feature
**Component:** Logging port
**File:** `application/src/logging_port.c`, `application/CMakeLists.txt`, `application/inc/SEGGER_RTT_Conf.h`

## What to build

Currently, the `embedded-log` library uses UART (USART1) via `_write` in `logging_port.c` to output logs. This blocking transport is too slow for real-time task constraints. We need to replace it with SEGGER RTT (Real Time Transfer).

We will remove the UART backend completely and pipe the `embedded-log` hook directly into `SEGGER_RTT_Write`. This ensures our existing logs (with RTC timestamps and log levels) are preserved, but are output at memory speed via the J-Link debugger. 

The SEGGER RTT source should be retrieved via CMake `FetchContent` (V8.58.0), identical to how we fetch `embedded-log`. 

A custom `SEGGER_RTT_Conf.h` should be provided in `application/inc` to configure the Up-buffer to at least 2048 bytes and ensure it operates in `SEGGER_RTT_MODE_NO_BLOCK_SKIP` mode, so the application does not hang if the debugger is detached.

## Acceptance criteria

- [ ] `application/CMakeLists.txt` is updated to fetch `https://github.com/SEGGERMicro/RTT.git` at tag `V8.58.0` via `FetchContent`.
- [ ] `application/inc/SEGGER_RTT_Conf.h` is created with a 2048-byte Up-buffer and `NO_BLOCK_SKIP` mode configured.
- [ ] `logging_port.c` no longer uses `_write(1, ...)` or standard UART outputs.
- [ ] `logging_port.c` includes `SEGGER_RTT.h`, initializes it in `Logging_Init()`, and uses `SEGGER_RTT_Write(0, buf, len)` in the timestamp hook.
- [ ] The project successfully compiles and links with the new RTT dependency.
- [ ] Log outputs correctly appear in the J-Link RTT Viewer.

## Blocked by

None - can start immediately
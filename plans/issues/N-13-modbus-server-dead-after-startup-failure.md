# [N-13] Modbus server stays permanently dead after a startup bind/listen failure

**Severity:** 🟡 Medium
**Component:** Modbus task
**File:** `application/src/modbus_task.c` (~205-213, 234-257)

## Description

If `modbus_tcp_server_thread()` returns early (connection/bind/listen failure), control returns to `vModbusTask`, which enters `for(;;) vTaskDelay(1000)` forever — the server is permanently dead with no retry. The watchdog is refreshed only once from this task at init (other tasks keep the system alive), so the device runs but Modbus never recovers.

## Impact

A transient lwIP resource shortage at startup permanently disables Modbus until reboot.

## Fix

Retry bind/listen with backoff instead of returning, or restart the server thread on failure.

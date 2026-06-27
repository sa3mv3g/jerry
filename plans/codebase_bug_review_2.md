# Codebase Bug Review Report — Round 2 (New Findings)

**Date:** 2026-06-27
**Reviewer:** Fresh independent review (Architect)
**Scope:** Application source files — independent pass to find bugs NOT already listed in `codebase_bug_review.md`
**Note:** Bugs already documented in the first report (BUG-01..BUG-18) are intentionally excluded. This report uses the prefix `N-` for "new".

---

## Summary

| Severity | Count |
|----------|-------|
| 🔴 Critical (data corruption / crash) | 3 |
| 🟠 High (incorrect behavior) | 5 |
| 🟡 Medium (potential issue under certain conditions) | 6 |
| 🔵 Low (code quality / minor) | 5 |

---

## 🔴 Critical Bugs

### N-01: Duplicate / racing digital-input updates from two tasks (vMainTask and vMonitorTask)

**File:** `application/src/main.c` lines 467-491
**File:** `application/src/monitor_task.c` lines 148-170, 213

Both the **Main task** (every 500 ms) and the **Monitor task** (every 5000 ms, via `print_digital_inputs()`) independently read the 8 digital inputs and call `LcdManager_UpdateDigitalInputStatus()`. These two tasks run at the same priority (`tskIDLE_PRIORITY + 1`) and there is no synchronization.

`LcdManager_UpdateDigitalInputStatus()` performs a read-modify-write on the shared `gDigitalInput` bitmask (`lcd_manager.c` lines 232-246) which is **not atomic**. If both tasks update different bits concurrently, one update can be lost (classic lost-update race on the shared byte).

**Impact:** Occasional wrong DI bits on the LCD; redundant I2C/LCD traffic; wasted CPU. Also the two tasks interpret "set" differently — `main.c` compares against `GPIO_PIN_SET`, while `monitor_task.c` uses `di_values[i] > 0`.

**Decision (2026-06-27):** The **Main task** becomes the sole owner of **all** LCD updates. The
**Monitor task must not write the LCD at all** — it keeps only its diagnostic *logging* and
LwIP/stack health checks.

**Fix — move every `LcdManager_*` write out of the Monitor task into the Main task:**
1. **Digital inputs** — remove the DI read + `LcdManager_UpdateDigitalInputStatus()` from
   `print_digital_inputs()` in [`monitor_task.c`](application/src/monitor_task.c:148). The Main task
   already polls DI and updates the LCD in [`vMainTask`](application/src/main.c:472); keep it there
   (and apply N-09: check the `BSP_GPIODI_Read()` return value). The Monitor task may still *log*
   DI values for diagnostics, but must not call any `LcdManager_*` setter.
2. **ADC / analog inputs** — move `print_adc_values()`'s `LcdManager_UpdateAnalogInput()` calls
   ([`monitor_task.c`](application/src/monitor_task.c:136)) into the Main task loop. The Monitor task
   may still log ADC values, but the LCD update belongs to Main.
3. **RTC time + analog outputs** — move `update_time_and_ao()`
   ([`monitor_task.c`](application/src/monitor_task.c:175)) entirely into the Main task:
   `LcdManager_UpdateTime()` and `LcdManager_UpdateAnalogOutput()`.

**Result:** Only the Main task ever calls `LcdManager_*` setters, so the shared LCD state
(`gDigitalInput`, `gAnalogInput[]`, `gAnalogOutput[]`, `gRtc*`) has a single writer and the
lost-update race disappears with **no mutex required** on that state.

**Notes for implementation:**
- The Main task currently runs every 500 ms; the Monitor task ran ADC/time/AO updates every 5 s.
  When relocating, decide the cadence in the Main loop (e.g., update DI every loop, and ADC/time/AO
  on a sub-interval) so the LCD refresh rate stays sensible.
- This also addresses the LCD-state half of **N-11**: with a single LCD writer task, the remaining
  N-11 concern narrows to the **shared I2C bus** (LCD task vs. Modbus/EEPROM/DO tasks at the BSP
  layer), which still needs a BSP-level mutex — independent of this change.

---

### N-02: `modbus_cb_write_multiple_coils()` fed a `uint16_t*` cast to `uint8_t*` — endianness/aliasing bug at startup

**File:** `application/src/modbus_task.c` lines 197-203

```c
initDigitalOutputCoilsValues = 0x0;
if (BSP_OK == BSP_I2CDO_Read(&initDigitalOutputCoilsValues))
{
    modbus_cb_write_multiple_coils(
        0, 16, (const uint8_t *)&initDigitalOutputCoilsValues);
}
```

`initDigitalOutputCoilsValues` is a `uint16_t`. It is reinterpreted as a `const uint8_t*` and passed as the packed-coil byte array for 16 coils. The byte ordering of the two coil bytes now depends on the **host endianness** (Cortex-M33 is little-endian), so coil bits 0-7 come from the low byte and 8-15 from the high byte. If the Modbus coil packing convention or the `BSP_I2CDO_Read()` bit layout does not match this exact little-endian interpretation, the restored output state will be wrong (e.g., channels 8-15 swapped with 0-7's semantics).

**Impact:** On boot, digital outputs may be restored to the wrong pattern. This is also a strict-aliasing / MISRA violation (accessing a `uint16_t` object through a `uint8_t*` is allowed, but the implicit endianness assumption is undocumented and fragile).

**Fix:** Build an explicit 2-byte array with documented bit ordering: `uint8_t coils[2] = { (uint8_t)(val & 0xFF), (uint8_t)(val >> 8) };` and pass that, with a comment explaining the coil-to-channel mapping.

---

### N-03: `modbus_handle_connection()` truncates oversized requests silently, then parses a corrupt frame

**File:** `application/src/modbus_task.c` lines 310-320

```c
uint16_t copy_len = (len > sizeof(s_rx_buffer))
                        ? (uint16_t)sizeof(s_rx_buffer)
                        : len;
(void)memcpy(s_rx_buffer, data, copy_len);
modbus_err = modbus_process_request(s_rx_buffer, copy_len, ...);
```

Two problems:

1. **Silent truncation:** If a single `netbuf` chunk is larger than `MODBUS_TCP_MAX_ADU_SIZE`, the request is truncated and then parsed as if complete. The truncated frame may pass length validation by coincidence and act on partial data.
2. **Fragmented frames not reassembled:** `netbuf_data()` only returns the **first** fragment of the netbuf. If a Modbus ADU spans multiple pbufs in the chain, only the first fragment is copied (the code never calls `netbuf_next()` to walk the chain, unlike the TCP echo server which does). A large/fragmented Modbus request will be processed with missing tail bytes.

**Impact:** Malformed or fragmented Modbus TCP requests can be mis-parsed, leading to wrong register/coil operations or parse errors. Under TCP, a single logical ADU is not guaranteed to arrive in one `netconn_recv()` either — there is no length-prefixed reassembly loop at all.

**Fix:** Walk the netbuf chain via `netbuf_next()`, accumulate until the MBAP-declared length is satisfied, and reject (not truncate) frames exceeding the buffer.

---

## 🟠 High Severity Bugs

### N-04: Stack-heavy local buffers in `modbus_process_request()` switch cases risk Modbus task stack overflow

**File:** `application/src/modbus_task.c` lines 399, 421, 443, 465, 550
**File:** `application/src/main.c` line 34 (`MODBUS_TASK_STACK_SIZE 1024U` words)

Several switch-case branches declare large local arrays on the stack:

- `uint8_t coil_values[256]` (read coils)
- `uint8_t input_values[256]` (read discrete inputs)
- `uint16_t register_values[125]` (read holding regs = 250 bytes)
- `uint16_t register_values[125]` (read input regs = 250 bytes)
- `uint16_t values[123]` (write multiple regs = 246 bytes)

Although these are in separate case scopes (so they don't all coexist), the compiler may not overlay them, and `modbus_process_request()` also holds three `static` ADU/PDU structs. Combined with the call depth into `modbus_cb_*` callbacks (which themselves declare `register_values`-indexed locals) and lwIP's `netconn_*` stack usage, **1024 words (4 KB)** can be tight. There is no high-water-mark check on the Modbus task (only the Main and the calling task are monitored in `print_task_stack_usage()` / `check_task_stacks()`).

**Impact:** Potential stack overflow on the Modbus task under deep call chains, which would trigger `vApplicationStackOverflowHook` (itself buggy — see original BUG-08).

**Fix:** Make the large per-case buffers `static` (the server is single-threaded), and add the Modbus task handle to the stack-usage monitor.

---

### N-05: `vEthernetTask` is created but its handle is never retained; link callback registered but `vEthernetTask` also polls link — double link handling

**File:** `application/src/tcp_echo_task.c` lines 63-89, 91-104, 220-227

`vEthernetTask` calls `ethernetif_check_link(netif)` every 10 ms. Separately, `link_callback()` is registered via `netif_set_link_callback()`. Depending on the port, `ethernetif_check_link()` may itself invoke the link callback, so link-up/down may be logged/handled from two contexts. More importantly, the 10 ms polling loop runs `ethernetif_check_link()` at high frequency at priority `tskIDLE_PRIORITY + 2` (same as the Modbus task), which can starve lower-priority work and adds needless MDIO traffic.

**Impact:** Redundant link processing, possible log spam, CPU/MDIO overhead, and priority contention with the Modbus server thread.

**Fix:** Either rely on the link callback (event-driven) or poll at a slower cadence (e.g., 500 ms-1 s). Do not run both at 10 ms.

---

### N-06: `tcp_echo_thread()` inner loop uses `netbuf_next()` incorrectly — can drop final fragment or loop wrong

**File:** `application/src/tcp_echo_task.c` lines 139-148

```c
while ((err = netconn_recv(newconn, &buf)) == ERR_OK)
{
    do
    {
        netbuf_data(buf, &data, &len);
        err = netconn_write(newconn, data, len, NETCONN_COPY);
    } while (netbuf_next(buf) >= 0);
    netbuf_delete(buf);
}
```

`netbuf_next()` returns `-1` when there are no more fragments, `0` on the last fragment, and `1` if more remain. The loop condition `>= 0` continues while on the last fragment too, but after `netbuf_next()` moves to the last fragment it returns `0` (continue), then the next call returns `-1` (stop). The issue: `netbuf_data()` is called **before** the first `netbuf_next()`, so the first fragment is echoed, but the write return value `err` is overwritten by the loop and never checked — a failed `netconn_write()` is ignored, so a half-open / errored connection keeps looping. Also there is no check on `netconn_write` failure to break out.

**Impact:** On write errors the echo loop spins; partial data may be echoed without backpressure handling.

**Fix:** Check `netconn_write()` result and break on error; verify the `netbuf_next()` return-value handling against the lwIP contract.

---

### N-07: `RTC_Manager_GetTimeWithMs()` can underflow when `subseconds > second_fraction`

**File:** `application/src/rtc_manager.c` lines 30-35

```c
if (pTimeDate->second_fraction > 0)
{
    uint32_t ms =
        ((pTimeDate->second_fraction - pTimeDate->subseconds) * 1000U) /
        (pTimeDate->second_fraction + 1U);
    return ms;
}
```

`second_fraction` and `subseconds` are read from the RTC. If `subseconds > second_fraction` (can happen transiently across a second rollover, or due to a stale read between the time and subsecond registers), the unsigned subtraction wraps to a huge value, and `ms` becomes an enormous bogus number instead of a value in `[0, 999]`.

**Impact:** Log timestamps occasionally show a garbage millisecond field (e.g., `.4294967` style overflow truncated). Not crashing, but misleading diagnostics.

**Fix:** Clamp: if `subseconds > second_fraction`, treat ms as 0 (or read time and subseconds with the documented shadow-register sequence that latches both atomically).

---

### N-08: `format_ai()` / `update_row_2/3` floating-point formatting can exceed field width and corrupt the LCD layout

**File:** `application/src/lcd_manager.c` lines 79-96, 98-120

`format_ai()` tries `%.3f`, `%.2f`, `%.1f`, `%.0f` until the result is ≤ 9 chars, then right-justifies into a 10-byte buffer with `%-9.9s`. But for **negative** large values or NaN/Inf, `snprintf("%.0f", val)` can still produce strings like `"-2147483648"` or `"-inf"`/`"nan"` that the `len > 9` ladder cannot shrink further. The final `%-9.9s` truncates to 9 chars, so a value like `-12345.678` becomes `-12345.67` silently (wrong reading) and the analog input value displayed is incorrect/misleading.

Additionally `gAnalogInput` is fed `adc_values[ch] * 3300.0f` (millivolts) from `monitor_task.c` line 136, so values up to ~3300 with 3 decimals (`3300.000` = 8 chars) are borderline; any negative calibration offset pushes past the width.

**Impact:** Misleading analog readings on the LCD; no crash.

**Fix:** Clamp/round to the display range explicitly and handle NaN/Inf before formatting; choose a fixed format that always fits the field.

---

## 🟡 Medium Severity Bugs

### N-09: `vMainTask` digital-input loop ignores the `BSP_GPIODI_Read()` return value

**File:** `application/src/main.c` lines 472-490

```c
BSP_GPIODI_Read(digitalInputChannelIndex, &digitalInputValue);
if (digitalInputValue == GPIO_PIN_SET) { ... }
```

The return code of `BSP_GPIODI_Read()` is discarded. If the read fails, `digitalInputValue` is used uninitialized-or-stale, so a failed read is interpreted as a valid logic level.

**Impact:** On I2C/GPIO read failure, a wrong DI state is pushed to the LCD.

**Fix:** Check the return value; skip the LCD update on error (as `monitor_task.c` partially does).

---

### N-10: `print_digital_inputs()` reads DI as `uint32_t` then compares `> 0`, but error sentinel is `0xFF`

**File:** `application/src/monitor_task.c` lines 150-163

On read failure, `di_values[i] = 0xFFU` is set as an "error" marker, but the **preceding** `LcdManager_UpdateDigitalInputStatus(i, di_values[i] > 0)` is only called in the success branch — good — yet the subsequent `LOG_INF` prints `0xFF` (255) as the DI value, which is indistinguishable from a real high reading in the log line (it prints `DIx=255`). There is no visual "ERR" indicator.

**Impact:** Minor — error reads are logged as `255`, which a human may misread as a valid level.

**Fix:** Log an explicit `ERR` token for the sentinel.

---

### N-11: `LcdManager_ShowVersionSplash()` ignores `LcdI2c_WriteStringAt()` return values and writes from the LCD task during init while no mutex protects `lcd_handle`

**File:** `application/src/lcd_manager.c` lines 281-309, 122-177

`LcdManager_ShowVersionSplash()` is called from `vLcdManageTask` during init (line 154), before the sync barrier. It writes directly to `lcd_handle`. The same `lcd_handle` is later written from the task's main loop. Since all LCD writes currently come from the single LCD task this is presently safe, but `lcdManager_Send` → `BSP_I2C_LcdWrite` shares the I2C bus with `BSP_I2CDO_*` (digital output expander) and `BSP_EEPROM_*`, which are driven from **other** tasks (Modbus, DigitalOutput). There is no indication the I2C bus access is mutex-protected at the BSP layer.

**Impact:** Concurrent I2C transactions from Modbus task (DO writes / EEPROM) and the LCD task can interleave on the shared I2C peripheral, corrupting transfers.

**Fix:** Confirm the BSP I2C layer serializes access with a mutex; if not, add one.

> **Related to N-01 decision (2026-06-27):** LCD *application state* now has a single owner —
> the Main task is the sole caller of `LcdManager_*` setters (the Monitor task no longer writes the
> LCD). That resolves the shared-LCD-state race. The remaining N-11 concern is strictly the
> **shared I2C peripheral**: `BSP_I2C_LcdWrite` (LCD) vs. `BSP_I2CDO_*` (digital outputs) vs.
> `BSP_EEPROM_*`, which are still driven from different tasks (LCD task, Modbus, DigitalOutput).
> That bus-level serialization mutex is still required and is independent of the LCD-ownership move.

---

### N-12: `gRtcHours/Minutes/Seconds % 100U` masks bad RTC values instead of validating

**File:** `application/src/lcd_manager.c` lines 54-56

```c
snprintf(temp, ..., gRtcHours % 100U, gRtcMinutes % 100U, gRtcSeconds % 100U);
```

Applying `% 100` to hours/minutes/seconds hides out-of-range RTC reads (e.g., hours=200 would display as `00`) rather than surfacing the error. Minutes/seconds should be `< 60` and hours `< 24`.

**Impact:** Invalid RTC values are silently masked into plausible-looking but wrong times.

**Fix:** Validate ranges; display a sentinel (e.g., `--:--:--`) on invalid reads.

---

### N-13: `modbus_tcp_server_thread()` exits on bind/listen failure leaving the task in a tight `for(;;)` doing nothing

**File:** `application/src/modbus_task.c` lines 205-213, 234-257

If `modbus_tcp_server_thread()` returns early (connection/bind/listen failure), control returns to `vModbusTask` which then enters `for(;;) vTaskDelay(1000)` forever — the Modbus server is **permanently dead** with no retry, and the watchdog is no longer refreshed from this task (it was only refreshed once at line 191). Other tasks refresh `hiwdg`, so the system may survive, but the Modbus server never recovers.

**Impact:** A transient lwIP resource shortage at startup permanently disables Modbus until reboot.

**Fix:** Retry bind/listen with backoff instead of returning; or restart the server thread.

---

### N-14: I2C scanner uses a *read* probe which can miss write-only devices and disturb device state

**File:** `application/src/i2c_scanner.c` lines 96-104

The scanner probes each address with a 1-byte **read** (`BSP_I2C_LcdRead`). Many devices (e.g., the PCF8574 outputs, some LCDs) are detected more reliably with a zero-length **write**/address-ACK probe. A read probe can (a) miss devices that NACK reads but ACK their address, and (b) for the PCF8574 output expander, reading drives the quasi-bidirectional pins and can momentarily affect outputs.

**Impact:** Inaccurate scan results; potential glitch on PCF8574 outputs during scan.

**Fix:** Use an address-only / zero-length write probe (ACK detection) for discovery.

---

## 🔵 Low Severity Issues

### N-15: `LOG_INF` used for an error condition in `check_lwip_errors()` (NETCONN pool)

**File:** `application/src/monitor_task.c` lines 87-93

The NETCONN pool error branch logs with `LOG_INF` while every other pool error uses `LOG_ERR`. Inconsistent severity hides a real error at the INFO level (which is filtered out in non-debug builds where the level is `LOG_LEVEL_WARNING`).

**Fix:** Use `LOG_ERR` for consistency.

---

### N-16: Inconsistent task priorities / `tskIDLE_PRIORITY + N` not using `(UBaseType_t)` casts like Main

**File:** `application/src/main.c` lines 438-459

The Main task is created with `(UBaseType_t)(tskIDLE_PRIORITY + 1U)` but all child tasks use bare `tskIDLE_PRIORITY + 1` / `+ 2` (signed int arithmetic). Minor MISRA 10.x signedness inconsistency.

**Fix:** Use consistent unsigned casts.

---

### N-17: `i2c_scanner.c` log lines embed `\n` while the logging port already appends timestamps per call

**File:** `application/src/i2c_scanner.c` (throughout)

Every `LOG_INF("...\n")` includes a trailing `\n`, but `log_hook_with_timestamp()` prepends a timestamp on **every** call. The result is timestamp + line + newline, and the blank `LOG_INF("\n")` calls produce timestamp-only blank lines. Cosmetic but noisy.

**Fix:** Drop embedded `\n`; rely on the logging framework's line handling.

---

### N-18: `format_ai()` declares `int len` and compares against literal `9` (magic number) repeatedly

**File:** `application/src/lcd_manager.c` lines 79-96

The width `9` and buffer size `10` are magic numbers duplicated across the function and the `%-9.9s`/`snprintf(buf, 10, ...)` calls. A mismatch between them would silently truncate.

**Fix:** Define a single `LCD_AI_FIELD_WIDTH` macro and derive the rest.

---

### N-19: `vLoggingTask` and `vFotaTask` are effectively empty stubs that still consume full task stacks

**File:** `application/src/logging_task.c`, `application/src/fota_task.c`

Both tasks do nothing but `vTaskDelay` in a loop, yet each reserves 512 words of static stack (`LOG_TASK_STACK_SIZE`, `FOTA_TASK_STACK_SIZE`). That is ~4 KB of RAM reserved for no functional work. (`fota_task.c` also has the dead `#define CONST 10000` already noted as BUG-15.)

**Impact:** Wasted RAM on a memory-constrained MCU.

**Fix:** Reduce their stack sizes to `configMINIMAL_STACK_SIZE` until they have real work, or remove them.

---

## Cross-cutting Observations (not numbered bugs)

- **Shared-state ownership is unclear.** Digital input/output, RTC, and analog values are mutated from multiple tasks through the `LcdManager_*` setters, none of which are synchronized. The original report's BUG-04 covers Modbus register data; N-01/N-11 extend the same class of concern to the LCD manager and the shared I2C bus. A single documented locking strategy would retire several of these.
- **Watchdog coverage is uneven.** `hiwdg` is refreshed from Monitor (every loop), and once each from Modbus/TcpEcho/LcdManage init. If the Monitor task ever stalls, the watchdog behavior depends entirely on it — worth confirming the IWDG timeout vs. the 5 s monitor interval (N-13 relates).
- **No length-delimited Modbus reassembly** (N-03) is the most functionally risky new finding for a networked Modbus/TCP device.
- **Non-atomic 32-bit float reads** are tracked as **BUG-19** in `codebase_bug_review.md` (added during re-validation). The calibrated-ADC read callback re-samples per register case, so a float can tear both within a single 2-register transaction and across two transactions. Fix it together with BUG-02/03 via a snapshot-at-callback-entry, and document the master-side requirement to read both float words in one transaction.

---

## Recommended Fix Priority (new items only)

1. **N-03** — Modbus TCP frame truncation / no reassembly (functional correctness over the network)
2. **N-02** — Boot-time coil restore endianness/aliasing
3. **N-01** — Racing digital-input updates between Main and Monitor tasks
4. **N-13** — Modbus server permanently dead after startup resource failure
5. **N-04** — Modbus task stack pressure from large per-case buffers
6. **N-07** — RTC millisecond underflow
7. **N-11** — Shared I2C bus access across tasks (verify BSP mutex)
8. **N-05 / N-06** — Ethernet link double-handling and echo write-error handling
9. **N-08 / N-12** — LCD numeric formatting / RTC range masking
10. **N-09 / N-10 / N-14** — Unchecked reads and scanner probe method
11. **N-15..N-19** — Logging/consistency/RAM cleanups

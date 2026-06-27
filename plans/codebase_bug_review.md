# Codebase Bug Review Report

**Date:** 2026-06-27  
**Reviewer:** Automated Code Review  
**Scope:** All application source files, modbus library, generated registers, BSP interfaces

---

## Summary

| Severity | Count |
|----------|-------|
| 🔴 Critical (data corruption / crash) | 2 |
| 🟠 High (incorrect behavior) | 5 |
| 🟡 Medium (potential issue under certain conditions) | 5 |
| 🔵 Low (code quality / minor) | 4 |

> **Re-validation note (2026-06-27):** This report was originally written against the
> repository-root `generated/` copy of the register files, which is **not** the copy the
> build compiles. The build regenerates and compiles `application/src/generated/` from
> `config/jerry_registers.json`. After re-validating against the actually-compiled files,
> **BUG-01 and BUG-06 are FALSE POSITIVES** (artifacts of the stale root copy, now deleted).
> BUG-02 and BUG-03 remain valid (they live in the hand-written callbacks, not the generated
> code). Counts above have been adjusted accordingly. **BUG-19 was added** (non-atomic 32-bit
> float reads) following a re-validation discussion — see the High Severity section.

---

## 🔴 Critical Bugs

### BUG-01: ~~`JERRY_DEVICE_COIL_RTC_COMMIT` undefined~~ — ❌ FALSE POSITIVE (re-validated 2026-06-27)

> **Status: FALSE POSITIVE.** This was diagnosed against the stale repository-root `generated/`
> copy. The actually-compiled header `application/src/generated/jerry_device_registers.h`
> **does** define `JERRY_DEVICE_COIL_RTC_COMMIT` (at address **30U**, not 28), with
> `JERRY_DEVICE_COIL_MAX_ADDR = 30U`, `JERRY_DEVICE_NUM_COILS = 29`, and the `jerry_device_coils_t`
> struct includes the `rtc_commit` field. No fix required. (Minor note: coil addresses 28-29 are
> an intentional gap between the PWM-enable coils and `RTC_COMMIT`.) The stale root copy that
> triggered this finding has been deleted.

#### Original (incorrect) analysis follows:

`JERRY_DEVICE_COIL_RTC_COMMIT` undefined — compilation should fail or uses wrong value

**File:** `application/src/modbus_device_callbacks.c` lines 468-469, 625-644  
**File:** `generated/jerry_device_registers.h`

The callback code references `JERRY_DEVICE_COIL_RTC_COMMIT` in both `modbus_cb_read_coils()` and `modbus_cb_write_single_coil()`, but this macro is **not defined** in `jerry_device_registers.h`. The header defines coils 0-27 (digital_output_0..15, digital_input_0..7, pwm_0..3_enable) with `JERRY_DEVICE_COIL_MAX_ADDR = 27U`, but there is no `RTC_COMMIT` coil at address 28.

Additionally, the `jerry_device_coils_t` struct has a `rtc_commit` field, but the header has no corresponding address macro.

**Impact:** If this macro is defined elsewhere (e.g., a missing header), the coil address may be 28 which is beyond `JERRY_DEVICE_COIL_MAX_ADDR` (27), meaning the address range validation at the top of `modbus_cb_read_coils()` would reject it. If it's not defined at all, the code won't compile.

**Fix:** Add `#define JERRY_DEVICE_COIL_RTC_COMMIT 28U` to the register header and update `JERRY_DEVICE_COIL_MAX_ADDR` to `28U`. Also update `JERRY_DEVICE_NUM_COILS` to `29`.

---

### BUG-02: Out-of-bounds array access in `modbus_cb_read_holding_registers()` for float calibration registers

**File:** `application/src/modbus_device_callbacks.c` lines 1004-1073  
**Register header:** `generated/jerry_device_registers.h`

When reading calibration float registers (e.g., `JERRY_DEVICE_HR_ADC_0_SCALE_FACTOR` = 104), the code does:

```c
case JERRY_DEVICE_HR_ADC_0_SCALE_FACTOR:
case JERRY_DEVICE_HR_ADC_0_SCALE_FACTOR + 1U:
    f32_to_u16(regs->adc_0_scale_factor,
               &register_values[JERRY_DEVICE_HR_ADC_0_SCALE_FACTOR]);
    break;
```

The `register_values` array is indexed by the **absolute Modbus address** (104), not by the relative offset `i` within the response. The `register_values` array is declared as `uint16_t register_values[125]` in the caller (`modbus_process_request`), so writing to index 104 is within bounds of that array. However, the Modbus response encoder reads `register_values[0..quantity-1]`, meaning the data written at index 104 will **never be read** by the response encoder if `start_address` is 104 — the encoder expects the data at `register_values[0]` (for `i=0`).

**Impact:** All float calibration register reads via Modbus return **garbage/zero data** instead of the actual calibration values. The response array positions `register_values[i]` are never written for these cases.

> **Clarification (2026-06-27, confirmed real):** A reasonable Modbus master reads both
> registers of a 32-bit float in a single transaction (e.g., `start=104, quantity=2`). One might
> think that "reading both registers" avoids the bug — **it does not.** With `start=104, quantity=2`:
> - `i=0, addr=104` → writes the whole float to `register_values[104]`/`[105]`
> - `i=1, addr=105` → writes the whole float **again** to `register_values[104]`/`[105]`
>
> The encoder still ships back `register_values[0]`/`[1]`, which were never written — so the
> response is garbage regardless of read pattern. In plain terms: **the code writes the right
> value to the wrong mailbox (box 104 instead of box 0), so the master always gets back the
> empty/leftover boxes (0 and 1).** A second defect is visible here too: the value is written
> **twice** (once per case) instead of splitting the high/low words by relative position.

**Fix:** Change all `f32_to_u16()` calls to use `&register_values[i]` instead of `&register_values[JERRY_DEVICE_HR_ADC_x_...]`. For the two-register float case, when `addr == base_addr`, write both u16 words at `register_values[i]` and `register_values[i+1]` (with bounds check); when `addr == base_addr + 1`, only write the second word. Better still, snapshot the float once at callback entry (see BUG-19) so it is not recomputed per case.

---

### BUG-03: Same out-of-bounds issue in `modbus_cb_read_input_registers()` for calibrated ADC values

**File:** `application/src/modbus_device_callbacks.c` lines 1474-1503

Same pattern as BUG-02. The `update_calibrated_adcval()` function writes to `&register_values[JERRY_DEVICE_IR_ADC_x_CALIBRATED_VALUE]` using the absolute Modbus address as the array index, but the response encoder expects data at `register_values[i]`.

```c
case JERRY_DEVICE_IR_ADC_0_CALIBRATED_VALUE:
case JERRY_DEVICE_IR_ADC_0_CALIBRATED_VALUE + 1:
    update_calibrated_adcval(...,
        &register_values[JERRY_DEVICE_IR_ADC_0_CALIBRATED_VALUE]);  // index 4, but should be index i
```

**Impact:** All calibrated ADC value reads return garbage data (same "right value, wrong mailbox" effect as BUG-02 — data lands at absolute index 4/5, response ships back index 0/1).

**Fix:** Same as BUG-02 — use `&register_values[i]` and handle the two-register float span correctly. Note this callback additionally **re-samples the ADC** via `update_calibrated_adcval()` inside each case, which introduces a separate tearing hazard — see BUG-19.

---

### BUG-04: Thread safety — shared Modbus buffers accessed from single-threaded server but register data accessed from multiple tasks

**File:** `application/src/modbus_task.c` lines 70-71 (static buffers)  
**File:** `application/src/modbus_device_callbacks.c` (register access)  
**File:** `application/src/monitor_task.c` lines 184-188 (register access)  
**File:** `application/src/main.c` lines 471-490 (digital input updates)

The `s_rx_buffer`, `s_tx_buffer`, and `modbus_process_request()` static ADU/PDU structures are all `static` in `modbus_task.c`, which is fine since the Modbus server is single-threaded (sequential connection handling). However, the **register data structures** (`s_holding_registers`, `s_coils`, etc.) are accessed from:

1. **Modbus task** — reads/writes via callbacks
2. **Monitor task** — reads `jerry_device_get_holding_registers()` to get PWM duty cycle values (line 186-187)
3. **Main task** — calls `LcdManager_UpdateDigitalInputStatus()` (not directly accessing registers, but the monitor task does)

There is **no mutex or critical section** protecting the shared register data.

**Impact:** Torn reads/writes on 32-bit fields (e.g., `pwm_x_frequency`, `app_build_number`, float calibration values) on the Cortex-M33 which does not guarantee atomic 32-bit access for unaligned fields. The `jerry_device_holding_registers_t` struct has mixed `uint16_t` and `uint32_t`/`float` fields, so compiler padding may cause misalignment.

**Fix:** Add a FreeRTOS mutex around register access, or use `taskENTER_CRITICAL()`/`taskEXIT_CRITICAL()` for short accesses.

---

## 🟠 High Severity Bugs

### BUG-05: `update_calibration()` compares floats with `==` — unreliable skip logic

**File:** `application/src/modbus_device_callbacks.c` lines 196-209

```c
static inline bsp_error_t update_calibration(uint32_t address, float newValue)
{
    float       oldVal = 0.0f;
    bsp_error_t err    = BSP_OK;

    err = BSP_EEPROM_Read(address, (uint8_t *)&oldVal, sizeof(oldVal));

    if (err == BSP_OK && oldVal == newValue)
    {
        return BSP_OK;
    }

    return BSP_EEPROM_Write(address, (uint8_t *)&newValue, sizeof(newValue));
}
```

Comparing floats with `==` is unreliable due to floating-point representation. If the EEPROM stores a slightly different bit pattern (e.g., due to NaN propagation or denormalized values), this comparison may fail when it should succeed, causing unnecessary EEPROM writes. Conversely, two semantically different values could compare equal.

**Impact:** Unnecessary EEPROM wear, or skipped writes when values have changed.

**Decision (2026-06-27):** Remove the read-back-and-compare entirely and **always write** the new value. Calibration is an infrequent, user/master-initiated operation (not a hot loop), so the redundant-write skip provided negligible benefit while introducing the unreliable float comparison and an extra EEPROM read on every update.

**Fix:** Drop the `BSP_EEPROM_Read` + comparison; the function becomes simply:

```c
static inline bsp_error_t update_calibration(uint32_t address, float newValue)
{
    return BSP_EEPROM_Write(address, (uint8_t *)&newValue, sizeof(newValue));
}
```

> **Wear trade-off (acceptable, documented):** every calibration update now hits the EEPROM
> even when the value is unchanged. If the EEPROM is flash-emulated, a write may trigger a
> page erase/relocation. This is fine because calibration writes are rare; do **not** reintroduce
> a naive `==` comparison to "save" writes — if write-suppression is ever genuinely needed, use a
> raw-bytes `memcmp()` instead of float `==`.

---

### BUG-06: ~~`jerry_device_persistant.h` is included but does not exist~~ — ❌ FALSE POSITIVE (re-validated 2026-06-27)

> **Status: FALSE POSITIVE.** This `#include` only appeared in the stale repository-root
> `generated/jerry_device_registers.c`, which is **not compiled**. The actually-compiled
> `application/src/generated/jerry_device_registers.c` does **not** include
> `jerry_device_persistant.h` — it is self-contained (the `jerry_nvm_t` struct and NVM offsets
> are defined inline in the generated header). No fix required. The stale root copy has been
> deleted.

#### Original (incorrect) analysis follows:

`jerry_device_persistant.h` is included but does not exist in the repository

**File:** `generated/jerry_device_registers.c` line 16

```c
#include "jerry_device_persistant.h"
```

This file was not found anywhere in the repository. The filename also has a typo ("persistant" should be "persistent").

**Impact:** Compilation failure if this header is not generated or provided by the build system. If it's an empty/stub header, it's dead code.

**Fix:** Either create the missing header, remove the include if unused, or fix the typo.

---

### BUG-07: `modbus_cb_read_coils()` reads digital outputs from `DigitalOutput_GetChannel()` but reads digital inputs from BSP — inconsistent coil struct update

**File:** `application/src/modbus_device_callbacks.c` lines 248-355 vs 360-455

For digital outputs (coils 0-15), the code reads from `DigitalOutput_GetChannel()` which reads the shadow register, but does **not** update the `coils->digital_output_x` struct field. The `value` local variable is set but the struct field is stale.

For digital inputs (coils 16-23), the code correctly updates `coils->digital_input_x` via `update_digital_input()`.

**Impact:** If any code reads `coils->digital_output_x` directly (rather than through `DigitalOutput_GetChannel()`), it will get stale data. The `modbus_cb_write_single_coil()` does update the struct, but `modbus_cb_read_coils()` does not sync it.

**Fix:** Update `coils->digital_output_x = value;` after each `DigitalOutput_GetChannel()` call in `modbus_cb_read_coils()`.

---

### BUG-08: `vApplicationStackOverflowHook` calls `LOG_ERR` which may use the stack — undefined behavior in stack overflow

**File:** `application/src/main.c` lines 125-144

When a stack overflow is detected, the hook calls `LOG_ERR()` multiple times. However, at this point the task's stack is already corrupted. `LOG_ERR` likely uses `snprintf` and other stack-heavy functions, which will further corrupt memory or fault.

**Impact:** Instead of a clean halt, the system may hard-fault or corrupt other memory before reaching the infinite loop.

**Decision (2026-06-27):** Replace all `LOG_ERR` calls in the hook with a **characteristic RED-LED blink** pattern and halt. The blink must be self-contained — no `snprintf`/logging/stack-heavy calls — so it is safe to run on a corrupted stack. This gives an unmistakable visual fault indication on hardware without relying on the (now-suspect) stack or the logging subsystem.

**Chosen pattern — "SOS-style" triple-blink burst, distinct from any normal heartbeat:**
- The board's **RED LED** (`LED_RED` on the NUCLEO-H563ZI, Nucleo-144) is used.
- Pattern: **3 fast blinks (≈150 ms on / 150 ms off), then a ≈1 s pause, repeat forever.**
- This 3-blink-then-long-pause cadence is deliberately different from any steady or slow "alive" blink the application might use elsewhere, so a stack overflow is visually unambiguous.

**Implementation notes (for the deferred code change in [`vApplicationStackOverflowHook`](application/src/main.c:125)):**
- Disable interrupts first (`taskDISABLE_INTERRUPTS()`), as already done.
- Drive the LED with the lightweight BSP/HAL primitives only — `BSP_LED_On(LED_RED)` / `BSP_LED_Off(LED_RED)` (or direct `HAL_GPIO_WritePin` on the LED port/pin) — **no** logging, **no** `snprintf`.
- Use a simple bounded busy-wait delay (a `volatile` down-counter loop), **not** `vTaskDelay` (the scheduler context is unreliable here).
- Call `BSP_LED_Init(LED_RED)` defensively in case the LED GPIO clock/config was not already set up.
- Keep the existing `for(;;)` halt structure, with the blink loop inside it.

**Documentation:** This diagnostic behavior is documented for end users in the top-level [`README.md`](README.md:1) under "Diagnostic LED Patterns".

**Fix (sketch — to be applied in Code mode):**

```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    taskDISABLE_INTERRUPTS();
    (void)BSP_LED_Init(LED_RED);  /* defensive */

    for (;;)
    {
        /* 3 fast blinks */
        for (uint32_t i = 0U; i < 3U; i++)
        {
            (void)BSP_LED_On(LED_RED);
            for (volatile uint32_t d = 0U; d < 600000U; d++) { /* ~150 ms */ }
            (void)BSP_LED_Off(LED_RED);
            for (volatile uint32_t d = 0U; d < 600000U; d++) { /* ~150 ms */ }
        }
        /* long pause */
        for (volatile uint32_t d = 0U; d < 4000000U; d++) { /* ~1 s */ }
    }
}
```

> Delay loop iteration counts are placeholders — tune them to the actual core clock so the
> on/off times land near 150 ms / 1 s. Exact timing is not critical; the *pattern* is what
> identifies the fault.

---

### BUG-09: LCD manager `gUdpateLcdSem` binary semaphore can lose updates

**File:** `application/src/lcd_manager.c` lines 28-29, 59-63, 167-176

The LCD update mechanism uses a binary semaphore. Multiple `update_row_x()` calls each do `xSemaphoreGive(gUdpateLcdSem)`. If multiple rows are updated before the LCD task runs, only one semaphore give is effective (binary semaphore saturates at 1). The LCD task then wakes up and writes all 4 rows, which is correct. However, if an update happens **during** the LCD write loop (lines 170-174), the semaphore give may be consumed by the current iteration, and the new update is lost until the next trigger.

**Impact:** Occasional missed LCD updates, causing stale display data until the next periodic update from the monitor task.

**Fix:** Use a counting semaphore, or add a "dirty" flag per row that is checked atomically.

---

### BUG-19: Non-atomic 32-bit float reads — value can "tear" within and across Modbus transactions

**File:** `application/src/modbus_device_callbacks.c` lines 1004-1073 (holding regs), 1473-1503 (input regs)

A 32-bit float occupies **two** Modbus registers (Modbus has no native 32-bit type — it is a convention). The current read callbacks recompute/copy the value **per register case** rather than taking a single stable snapshot. For input registers this is worse: each calibrated-value case calls `update_calibrated_adcval()`, which **re-reads the ADC channel and recomputes the float during the read callback itself**.

Two distinct tearing scenarios result:

1. **Within a single 2-register transaction.** Because the float is produced once for the `addr == base` case and again for the `addr == base + 1` case, the two halves the master receives can come from two different computations (and, for the calibrated values, two different ADC acquisitions). The high word and low word may not belong to the same float.

2. **Across two separate transactions.** If a master reads the high word in one request and the low word in another, the input is re-sampled in between, so the reassembled float on the master side is a corrupt bit pattern that matches neither the old nor the new value.

**Plain-language version:** imagine a float is a two-digit number. The firmware reads the "tens digit" from one camera snapshot and the "ones digit" from a *later* snapshot. If the value changed in between, you get a number that never actually existed.

**Impact:** Calibrated ADC float reads (and, to a lesser extent, the calibration HR floats if they are ever changed concurrently) can return corrupt values whenever the underlying input is changing. This is masked today by BUG-02/03 (the data never reaches the master at all), but becomes live as soon as BUG-02/03 is fixed.

**Fix:**
- **Snapshot once at callback entry:** compute every multi-register value a single time into the backing struct (`regs->adc_x_calibrated_value`) before the per-register loop, then the loop only copies the frozen words by relative index `i`. This eliminates both the duplicate computation and the within-transaction tear.
- **Document the master-side contract:** a master must read both words of a float in **one** transaction. Cross-transaction tearing cannot be solved on the slave alone.
- **Optional:** provide a "latch/freeze" coil the master can set to snapshot all volatile values before a multi-transaction read.

**Where to document the master-side float-atomicity contract (2026-06-27):**

The contract is "every 32-bit value (all `float32` registers, plus the `uint32` build number / PWM
frequencies) spans two consecutive 16-bit registers; a master must read or write **both** words in a
**single** Modbus transaction. The device does not guarantee a consistent snapshot if the two halves
are accessed in separate transactions." Record it in these places, in priority order:

1. **`config/jerry_registers.json` (source of truth).** Add a device-level note (e.g., a
   `description`/`notes` field) stating the 32-bit-pair atomicity rule. The register entries already
   carry the type information — the generated map shows a `Type` column (`float32`, `uint32`) with
   `Size 2` for these registers — so no per-register retagging is needed; the rule keys off
   `Size == 2` / `Type` being a 32-bit type. Everything else is generated from this file.
2. **The codegen — `tools/modbus_codegen/` (script + templates).** Make the generated register-map
   doc print an atomicity **legend/footnote** for every `Size 2` / `float32`/`uint32` register, so
   the build artifact `build/<profile>/jerry_device_register_map.txt` always carries the contract.
   ⚠️ Do **not** hand-edit the generated `.txt` — it is regenerated on every build; edit the template
   instead.
3. **`README.md` — "Modbus Library" section.** Add a short integrator-facing note describing the
   two-register 32-bit convention, the word order, and the single-transaction requirement.
4. **(Optional) `docs/FIRMWARE_README.md`** — cross-reference the same note if it documents the
   Modbus interface for firmware consumers.
5. **(Optional, enforcement) `tests/integration/test_holding_registers.py`** — add/confirm a test
   that reads each float as a single 2-register transaction (correct usage), making the contract
   executable and regression-proof. `tests/integration/register_map.py` already parses
   `jerry_registers.json`, so the test can iterate all `Size 2` registers automatically.

---

## 🟡 Medium Severity Bugs

### BUG-10: `LcdManager_UpdateIpv4Address()` uses `atoi()` — banned by MISRA, no error handling

**File:** `application/src/lcd_manager.c` lines 193-208

```c
gIpLastOctet = (uint8_t)atoi(last_dot + 1);
```

`atoi()` has undefined behavior for out-of-range values and no error reporting. Also, `stdlib.h` is included (line 4) which may pull in dynamic memory allocation functions, violating the project requirement of no dynamic allocation.

**Impact:** If the IP string is malformed, `atoi()` returns 0 silently. The `stdlib.h` include may cause linker issues if `malloc`/`free` symbols are referenced.

**Fix:** Use `strtoul()` with error checking, or a simple manual parser. Remove `stdlib.h` include.

---

### BUG-11: `modbus_tcp_server_thread()` dereferences `lwip_stats.memp[MEMP_NETCONN]` without NULL check

**File:** `application/src/modbus_task.c` line 231

```c
LOG_INF("[Modbus]: Available netconns: %d",
        lwip_stats.memp[MEMP_NETCONN]->avail);
```

Unlike all other accesses to `lwip_stats.memp[]` in the codebase (which check for NULL first), this line dereferences without a NULL check.

**Impact:** If `MEMP_STATS` is disabled or the pool pointer is NULL, this causes a hard fault.

**Fix:** Add a NULL check before dereferencing.

---

### BUG-12: `modbus_process_request()` uses `static` local variables — not reentrant

**File:** `application/src/modbus_task.c` lines 368-370

```c
static modbus_adu_t request_adu;
static modbus_adu_t response_adu;
static modbus_pdu_t response_pdu;
```

These are `static` to avoid stack allocation (the structs are large). This is safe only because the Modbus server handles one connection at a time. However, the code defines `MODBUS_MAX_CONNECTIONS 4U` and has a `s_connections[]` array, suggesting future multi-connection support was planned. If multi-threaded connection handling is ever added, these statics will cause data corruption.

**Impact:** No current bug, but a latent defect if the architecture changes.

**Fix:** Add a comment documenting the single-threaded assumption, or allocate per-connection buffers.

---

### BUG-13: `end_address` underflow when `quantity` is 0

**File:** `application/src/modbus_device_callbacks.c` lines 222, 697, 840, 1445

```c
uint16_t end_address = start_address + quantity - 1U;
```

If `quantity` is 0 (which the Modbus PDU decoder does not validate against), `end_address` wraps to `0xFFFF`, bypassing the address range check. The PDU decoder in `modbus_pdu.c` does check `quantity > 0` for encode functions but **not** for decode functions.

**Impact:** A malformed Modbus request with `quantity=0` could cause the address validation to pass incorrectly, leading to a zero-iteration loop (benign) or unexpected behavior.

**Fix:** Add `quantity == 0` validation at the start of each callback, or add it to the PDU decode functions.

---

### BUG-14: `logging_port.c` has inverted `#ifndef BSP_USING_RTOS` guard

**File:** `application/src/logging_port.c` lines 7-9

```c
#ifndef BSP_USING_RTOS
#include "FreeRTOS.h"
#include "task.h"
#endif
```

The guard says "if NOT using RTOS, include FreeRTOS headers" — this is backwards. The FreeRTOS headers should be included when RTOS **is** being used.

Additionally, the fallback code on line 42 uses `xTaskGetTickCount()` inside `#ifdef BSP_USING_RTOS`, but the FreeRTOS headers are only included when `BSP_USING_RTOS` is **not** defined.

**Impact:** If `BSP_USING_RTOS` is defined, FreeRTOS headers are not included but `xTaskGetTickCount()` is called — compilation error. If `BSP_USING_RTOS` is not defined (current state), FreeRTOS headers are included and the fallback path uses `ticks = 0` which is dead code.

**Fix:** Change `#ifndef` to `#ifdef`, or remove the guard entirely since this is always an RTOS project.

---

## 🔵 Low Severity Issues

### BUG-15: `fota_task.c` has unused `#define CONST 10000`

**File:** `application/src/fota_task.c` line 10

**Impact:** Dead code, no functional impact.

---

### BUG-16: `modbus_data.h` defines `ModbusData_t` struct that is never used

**File:** `application/inc/modbus_data.h`

This struct appears to be a legacy/placeholder that was superseded by the code-generated `jerry_device_registers.h` structures.

**Impact:** Dead code, potential confusion.

---

### BUG-17: `LcdManager_IsLcdReady()` takes `SemaphoreHandle_t*` but calls `xSemaphoreGive()` on the pointer directly

**File:** `application/src/lcd_manager.c` lines 185-191

```c
void LcdManager_IsLcdReady(SemaphoreHandle_t* isLcdReadySem)
{
    if ((NULL != isLcdReadySem) && (NULL != gUdpateLcdSem))
    {
        xSemaphoreGive(isLcdReadySem);  // Should be *isLcdReadySem
    }
}
```

`xSemaphoreGive()` expects a `SemaphoreHandle_t`, but `isLcdReadySem` is a `SemaphoreHandle_t*` (pointer to handle). This should be `xSemaphoreGive(*isLcdReadySem)`.

**Impact:** Passes a pointer-to-pointer to the FreeRTOS API, which will interpret it as a queue handle and likely corrupt memory or crash.

**Fix:** Dereference the pointer: `xSemaphoreGive(*isLcdReadySem)`.

---

### BUG-18: Typo in semaphore variable name `gUdpateLcdSem` (should be `gUpdateLcdSem`)

**File:** `application/src/lcd_manager.c` lines 28-29

**Impact:** No functional impact, but reduces code readability and searchability.

---

## Recommended Fix Priority

1. **BUG-02 & BUG-03** — Critical data corruption: Modbus float register reads return garbage
2. **BUG-17** — `LcdManager_IsLcdReady()` pointer bug: potential crash
3. **BUG-11** — NULL pointer dereference in Modbus stats logging
4. **BUG-08** — Stack overflow hook using stack-heavy logging
5. **BUG-04** — Thread safety on shared register data
6. **BUG-14** — Inverted RTOS preprocessor guard
7. **BUG-05** — Float comparison in EEPROM skip logic (decided: remove comparison, always write)
8. **BUG-07** — Stale coil struct fields on read
9. **BUG-19** — Non-atomic 32-bit float reads (tearing); fix alongside BUG-02/03 via snapshot-at-entry
10. **BUG-13** — Quantity=0 underflow
11. **BUG-09** — Binary semaphore LCD update loss
12. **BUG-10** — `atoi()` usage
13. **BUG-12, 15, 16, 18** — Code quality items

> ~~**BUG-01**~~ and ~~**BUG-06**~~ removed from the priority list — re-validated as FALSE
> POSITIVES against the actually-compiled `application/src/generated/` files (see notes on each).

# [N-01] Racing digital-input / LCD updates between Main and Monitor tasks

**Severity:** 🔴 Critical (lost-update race)
**Component:** Main task / Monitor task / LCD manager
**Files:** `application/src/main.c` (~467-491), `application/src/monitor_task.c` (~136, 148-170, 175-188, 213)

## Description

Both the Main task (every 500 ms) and the Monitor task (every 5 s) independently poll digital inputs and call `LcdManager_UpdateDigitalInputStatus()`. They run at the same priority with no synchronization, and `LcdManager_*` setters do an unguarded read-modify-write on shared LCD state — a classic lost-update race. The two tasks also disagree on "set" (`GPIO_PIN_SET` vs `> 0`).

## Decision

The **Main task** becomes the **sole owner of all LCD updates**. The **Monitor task must not write the LCD at all** (it keeps only diagnostic logging and LwIP/stack health checks).

## Fix — move every `LcdManager_*` write from Monitor to Main

1. **Digital inputs** — remove DI read + `LcdManager_UpdateDigitalInputStatus()` from `print_digital_inputs()` (`monitor_task.c`). Keep DI polling/LCD update in `vMainTask` (apply N-09: check `BSP_GPIODI_Read()` return). Monitor may still *log* DI values.
2. **ADC / analog inputs** — move `LcdManager_UpdateAnalogInput()` (`monitor_task.c:136`) into the Main loop.
3. **RTC time + analog outputs** — move `update_time_and_ao()` (`monitor_task.c:175`) — `LcdManager_UpdateTime()` and `LcdManager_UpdateAnalogOutput()` — into the Main task.

**Result:** single writer for all shared LCD state → no mutex needed on that state. Choose a sensible refresh cadence in the Main loop (DI every loop, ADC/time/AO on a sub-interval).

## Related

- N-09 (check `BSP_GPIODI_Read()` return)
- N-11 (shared I2C bus still needs a BSP-level mutex)
- BUG-09 (LCD semaphore give/consume race — separate)

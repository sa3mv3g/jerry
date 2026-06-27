# [BUG-08] `vApplicationStackOverflowHook` uses stack-heavy `LOG_ERR` — replace with characteristic RED-LED blink

**Severity:** 🟠 High
**Component:** RTOS hooks / diagnostics
**File:** `application/src/main.c` (lines ~125-144)

## Description

On stack overflow, the hook calls `LOG_ERR()` several times. The task stack is already corrupted, and `LOG_ERR` uses `snprintf`/stack-heavy functions, which can further corrupt memory or fault before the halt.

## Decision

Replace all `LOG_ERR` calls in the hook with a **characteristic RED-LED blink** and halt. The blink must be self-contained (no logging, no `snprintf`) so it is safe on a corrupted stack.

**Pattern:** RED LED (`LED_RED` on NUCLEO-H563ZI) — **3 fast blinks (~150 ms on/off), then a ~1 s pause, repeat forever.** Deliberately distinct from any normal heartbeat so a stack overflow is visually unambiguous.

## Fix (sketch)

```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    taskDISABLE_INTERRUPTS();
    (void)BSP_LED_Init(LED_RED);  /* defensive */

    for (;;)
    {
        for (uint32_t i = 0U; i < 3U; i++)
        {
            (void)BSP_LED_On(LED_RED);
            for (volatile uint32_t d = 0U; d < 600000U; d++) { /* ~150 ms */ }
            (void)BSP_LED_Off(LED_RED);
            for (volatile uint32_t d = 0U; d < 600000U; d++) { /* ~150 ms */ }
        }
        for (volatile uint32_t d = 0U; d < 4000000U; d++) { /* ~1 s */ }
    }
}
```

- Use `BSP_LED_On/Off(LED_RED)` (or direct `HAL_GPIO_WritePin`) — no logging.
- Busy-wait delay (`volatile` counter), **not** `vTaskDelay`.
- Tune iteration counts to the core clock; exact timing is not critical, the *cadence* identifies the fault.

## Documentation

Behavior is documented for operators in the top-level `README.md` → "Diagnostic LED Patterns".

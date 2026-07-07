# Debug Report: `EE_Init()` Fails with `EE_ERASE_ERROR` on STM32H563 TrustZone

**Date:** 2026-07-07  
**Project:** Jerry / MPCS (STM32H563 TrustZone firmware)  
**Resolved by:** Zoo (debug agent)  
**Status:** ✅ Fixed and verified on hardware

---

## 1. Failure Signature

### Symptom
Secure firmware halts in [`Error_Handler()`](../../application/bsp/stm/stm32h563/Secure/Core/Src/main.c:553) at boot. Call stack:

```
Error_Handler @ 0x0c003eec   (Secure firmware)
main          @ 0x0c003680   line 127  (Secure main.c)
Reset_Handler @ 0x0c004942
```

### Debugger Variables at Halt
| Variable | Value | Meaning |
|---|---|---|
| `ee_unlock_status` | `HAL_OK (0)` | Flash unlock succeeded |
| `ee_init_status` | `EE_ERASE_ERROR (2)` | Erase inside EE_Init failed |
| `flash_seccr_before` | `1` | SECCR was locked at reset (expected) |
| `flash_seccr_after` | `0` | SECCR unlocked successfully |
| `flash_edata1r` | `0x8007` | EDATA1 enabled, 8 sectors (correct) |
| `flash_secsr` | `0x00000000` | SECSR already cleared by HAL |
| `flash_error_code` | `0x00020000` | **`WRPERR` (bit 17) — Write Protection Error** |

### XPERIPHERALS at Halt
```
FLASH_SECSR @ 0x24 = 0x00000000   (cleared by HAL after error)
FLASH_NSCR  @ 0x28 = 0x00000001   (LOCK bit set)
```

### Option Bytes at Time of Failure
```
TZEN         = 0xB4  (TrustZone enabled)
SECWM1_STRT  = 0x0   (sector 0)
SECWM1_END   = 0x8   (sector 8 — NSC veneer)
SECWM2_STRT  = 0x7F  (Bank 2 fully Non-Secure)
SECWM2_END   = 0x0
NSBOOTADD    = 0x80120  (0x08012000)
EDATA1_EN    = 1
EDATA1_STRT  = 7     (8 sectors: 120-127)
EDATA2_EN    = 0
WRPSGn1      = 0xFFFFFFFF  (no WRP active)
```

---

## 2. How to Reproduce

1. Flash the STM32H563 with TrustZone enabled (`TZEN=0xB4`).
2. Set `SECWM1_END=8` (sectors 0-8 Secure, sectors 9-127 Non-Secure).
3. Enable EDATA: `EDATA1_EN=1, EDATA1_STRT=7` (sectors 120-127).
4. In Secure `main.c`, call `EE_Init(EE_FORCED_ERASE)` with `FI_PageErase` using `FLASH_TYPEERASE_SECTORS` (the default ST middleware value).
5. Observe: board halts in `Error_Handler()`. `pFlash.ErrorCode = 0x20000` (WRPERR).

---

## 3. Root Cause Analysis

### The TrustZone Flash Control Register Rule

The STM32H563 has two flash control registers:
- **`FLASH->SECCR`** — Secure Control Register. Used by Secure code for operations on **Secure sectors** (those within `SECWM1_STRT` to `SECWM1_END`).
- **`FLASH->NSCR`** — Non-Secure Control Register. Used for operations on **Non-Secure sectors** (those outside the SECWM).

The HAL selects which register to use based on [`IS_FLASH_SECURE_OPERATION()`](../../application/bsp/stm/stm32h563/Drivers/STM32H5xx_HAL_Driver/Inc/stm32h5xx_hal_flash.h:795):

```c
#define IS_FLASH_SECURE_OPERATION() \
    ((pFlash.ProcedureOnGoing & FLASH_NON_SECURE_MASK) == 0U)
```

`FLASH_NON_SECURE_MASK = 0x80000000U`. When `FLASH_TYPEERASE_SECTORS` is used (no `FLASH_NON_SECURE_MASK` bit), `IS_FLASH_SECURE_OPERATION()` returns `true`, so [`FLASH_Erase_Sector()`](../../application/bsp/stm/stm32h563/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash_ex.c:1002) writes to `FLASH->SECCR`.

### Why EDATA Sectors Trigger WRPERR

EDATA sectors 120-127 are **outside** `SECWM1_END=8`. The hardware classifies them as **Non-Secure sectors**. When Secure code attempts to erase a Non-Secure sector via `FLASH->SECCR`, the hardware rejects the operation with `WRPERR` (Write Protection Error, `FLASH_SR_WRPERR`, bit 17 of `FLASH->SECSR`).

This is documented in RM0481 §7.6.1: the Secure Watermark defines which sectors are Secure. Sectors outside the SECWM are Non-Secure and must be erased via `FLASH->NSCR`, even when the caller is Secure code.

### Call Chain

```
EE_Init(EE_FORCED_ERASE)
  └─ EE_Format(EE_FORCED_ERASE)          [eeprom_emul.c:649]
       └─ FI_PageErase(page=0, NbPages=1) [flash_interface.c:123]
            └─ HAL_FLASHEx_Erase(TypeErase=FLASH_TYPEERASE_SECTORS, Sector=120)
                 └─ FLASH_Erase_Sector(120, FLASH_BANK_1)
                      └─ FLASH->SECCR |= (FLASH_CR_SER | (120<<6) | FLASH_CR_START)
                           ↑ WRPERR: sector 120 is Non-Secure, SECCR rejects it
```

### Why `flash_secsr = 0` in the Debugger

`FLASH_WaitForLastOperation()` reads `FLASH->SECSR`, saves the error to `pFlash.ErrorCode`, then **clears** the error flags via `FLASH->SECCCR`. By the time the debugger reads `FLASH->SECSR` after `EE_Init` returns, the flags are already cleared. The error is preserved in `pFlash.ErrorCode = 0x20000`.

### Why the Previous "Double-Unlock" Theory Was Wrong

An earlier hypothesis claimed that calling `HAL_FLASH_Unlock()` before `EE_Init()` caused a "double-unlock" that locked `FLASH_SECCR`. This is false. [`HAL_FLASH_Unlock()`](../../application/bsp/stm/stm32h563/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash.c:559) guards against double-unlock:

```c
if (READ_BIT(FLASH->NSCR, FLASH_CR_LOCK) != 0U)   // only writes keys if locked
if (READ_BIT(FLASH->SECCR, FLASH_CR_LOCK) != 0U)  // same for SECCR
```

Calling `HAL_FLASH_Unlock()` when already unlocked is a safe no-op.

---

## 4. Fix

### Files Modified

#### [`application/bsp/stm/stm32h563/Secure/Core/EEPROM_Emul/Porting/STM32H5/flash_interface.c`](../../application/bsp/stm/stm32h563/Secure/Core/EEPROM_Emul/Porting/STM32H5/flash_interface.c)

**`FI_PageErase()`** and **`FI_PageErase_IT()`**: Changed erase type from `FLASH_TYPEERASE_SECTORS` to `FLASH_TYPEERASE_SECTORS_NS` when `EDATA_ENABLED`:

```c
// BEFORE (broken):
s_eraseinit.TypeErase = FLASH_TYPEERASE_SECTORS;   // routes to SECCR → WRPERR

// AFTER (fixed):
#ifdef EDATA_ENABLED
s_eraseinit.TypeErase = FLASH_TYPEERASE_SECTORS_NS; // routes to NSCR → OK
s_eraseinit.Sector    = Page + 120;
#else
s_eraseinit.TypeErase = FLASH_TYPEERASE_SECTORS;
s_eraseinit.Sector    = Page;
#endif
```

**`FI_WriteDoubleWord()`**: Changed program type from `FLASH_TYPEPROGRAM_HALFWORD_EDATA` to `FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS`:

```c
// BEFORE (broken):
HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD_EDATA, Address, ...)

// AFTER (fixed):
HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS, Address, ...)
```

#### [`application/bsp/stm/stm32h563/Secure/Core/Src/main.c`](../../application/bsp/stm/stm32h563/Secure/Core/Src/main.c)

Restored `HAL_FLASH_Unlock()` before `EE_Init()`. The EEPROM library does not call `HAL_FLASH_Unlock()` internally — it assumes the caller has already unlocked flash. Without this call, `FLASH->NSCR` remains locked at reset and all erase/write operations are silently discarded.

```c
if (HAL_FLASH_Unlock() != HAL_OK)
{
  Error_Handler();
}
if (EE_Init(EE_FORCED_ERASE) != EE_OK)
{
  Error_Handler();
}
```

---

## 5. Verification

After applying the fix and flashing:
- `EE_Init(EE_FORCED_ERASE)` returns `EE_OK`
- Board boots past Secure `main()` and reaches `NonSecure_Init()`
- NS firmware runs normally

---

## 6. Key STM32H563 TrustZone Flash Rule (for future agents)

> **EDATA sectors (120-127) are Non-Secure sectors** (outside `SECWM1_END=8`).  
> All flash operations on Non-Secure sectors — even from Secure code — must use the `_NS` variants:
> - Erase: `FLASH_TYPEERASE_SECTORS_NS` (not `FLASH_TYPEERASE_SECTORS`)
> - Program: `FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS` (not `FLASH_TYPEPROGRAM_HALFWORD_EDATA`)
>
> Using the Secure variants on Non-Secure sectors triggers `WRPERR` (`pFlash.ErrorCode = 0x20000`).
>
> The `_NS` variants set `FLASH_NON_SECURE_MASK` in `pFlash.ProcedureOnGoing`, which causes
> `IS_FLASH_SECURE_OPERATION()` to return `false`, routing the operation through `FLASH->NSCR`.

---

## 7. Diagnostic Instrumentation Used

The following temporary variables were added to `main()` to capture the failure state (removed in final firmware):

```c
volatile HAL_StatusTypeDef ee_unlock_status;
volatile EE_Status          ee_init_status;
volatile uint32_t           flash_seccr_before  = FLASH->SECCR;
volatile uint32_t           flash_edata1r       = FLASH->EDATA1R_CUR;
volatile uint32_t           flash_secsr         = 0U;
volatile uint32_t           flash_error_code    = 0U;

ee_unlock_status = HAL_FLASH_Unlock();
volatile uint32_t flash_seccr_after = FLASH->SECCR;

ee_init_status   = EE_Init(EE_FORCED_ERASE);
flash_secsr      = FLASH->SECSR;
flash_error_code = pFlash.ErrorCode;
```

To re-diagnose a future `EE_Init` failure: add these variables, set a breakpoint in `Error_Handler()`, and inspect `flash_error_code`. The error codes map to:
- `0x00020000` = `WRPERR` — wrong erase type (Secure vs NS mismatch)
- `0x00040000` = `PGSERR` — programming sequence error
- `0x00080000` = `STRBERR` — strobe error
- `0x00100000` = `INCERR` — inconsistency error

---

## 8. Related Files

| File | Role |
|---|---|
| [`Secure/Core/Src/main.c`](../../application/bsp/stm/stm32h563/Secure/Core/Src/main.c) | Calls `HAL_FLASH_Unlock()` + `EE_Init()` |
| [`Secure/Core/EEPROM_Emul/Porting/STM32H5/flash_interface.c`](../../application/bsp/stm/stm32h563/Secure/Core/EEPROM_Emul/Porting/STM32H5/flash_interface.c) | `FI_PageErase`, `FI_WriteDoubleWord` — fixed to use `_NS` variants |
| [`Secure/Core/Inc/partition_stm32h563xx.h`](../../application/bsp/stm/stm32h563/Secure/Core/Inc/partition_stm32h563xx.h) | SAU regions, `SECWM1_END=8` |
| [`tools/flash_nucleo.py`](../../tools/flash_nucleo.py) | Programs option bytes: `EDATA1_EN=1, EDATA1_STRT=7, SECWM1_END=8` |
| [`Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash_ex.c`](../../application/bsp/stm/stm32h563/Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash_ex.c) | `HAL_FLASHEx_Erase`, `FLASH_Erase_Sector` |
| [`Drivers/STM32H5xx_HAL_Driver/Inc/stm32h5xx_hal_flash.h`](../../application/bsp/stm/stm32h563/Drivers/STM32H5xx_HAL_Driver/Inc/stm32h5xx_hal_flash.h) | `IS_FLASH_SECURE_OPERATION()`, `FLASH_NON_SECURE_MASK` |

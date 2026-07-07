# STM32H563 Memory Partition

**MCU:** STM32H563ZIT6 (NUCLEO-H563ZI)  
**Flash:** 2 MB (2 × 1 MB banks, 128 sectors × 8 KB each)  
**RAM:** 640 KB (SRAM1 256 KB + SRAM2 64 KB + SRAM3 320 KB)  
**TrustZone:** Enabled (`TZEN = 0xB4`)  
**Reference:** RM0481 Rev 4, Chapter 7

---

## Flash Layout

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  BANK 1  (1 MB physical — 0x0800_0000 to 0x080F_FFFF)                       │
│  Secure alias:  0x0C00_0000 to 0x0C0F_FFFF  (SECBOOTADD entry point)        │
├──────────┬──────────────────┬────────┬──────────────────────────────────────┤
│ Sectors  │ Physical Address │  Size  │ Contents                             │
├──────────┼──────────────────┼────────┼──────────────────────────────────────┤
│  0 – 30  │ 0x0800_0000      │ 248 KB │ Secure firmware code + wolfSSL       │
│          │ (alias 0x0C00_0000)       │ Linker: FLASH (rx) @ 0x0C000000      │
│          │                  │        │ SECWM1_STRT=0, SECWM1_END=0x1F      │
├──────────┼──────────────────┼────────┼──────────────────────────────────────┤
│   31     │ 0x0803_E000      │   8 KB │ NSC veneer (.gnu.sgstubs)            │
│          │ (alias 0x0C03_E000)       │ Linker: FLASH_NSC (rx) @ 0x0C03E000  │
│          │                  │        │ SAU Region 0: 0x0C03E000–0x0C03FFFF  │
├──────────┼──────────────────┼────────┼──────────────────────────────────────┤
│ 32 – 119 │ 0x0804_0000      │ 704 KB │ NonSecure firmware (active)          │
│          │                  │        │ Linker: FLASH (rx) @ 0x08040000      │
│          │                  │        │ NSBOOTADD = 0x8040_0 → 0x0804_0000  │
│          │                  │        │ SAU Region 1: 0x08040000–0x081FFFFF  │
├──────────┼──────────────────┼────────┼──────────────────────────────────────┤
│ 120–127  │ remapped → EDATA │  48 KB │ EDATA1: EEPROM emulation             │
│          │ 0x0D00_0000 (S)  │        │ Linker: EDATA_S (rw) @ 0x0D000000    │
│          │ 0x0900_0000 (NS) │        │ 100K erase cycles, 6-bit ECC/16-bit  │
│          │                  │        │ EDATA1_EN=1, EDATA1_STRT=7           │
│          │                  │        │ Stable across SWAP_BANK (Secure alias)│
└──────────┴──────────────────┴────────┴──────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│  BANK 2  (1 MB physical — 0x0810_0000 to 0x081F_FFFF)                       │
│  Fully Non-Secure (SECWM2_STRT=0x7F, SECWM2_END=0x00)                       │
├──────────┬──────────────────┬────────┬──────────────────────────────────────┤
│ Sectors  │ Physical Address │  Size  │ Contents                             │
├──────────┼──────────────────┼────────┼──────────────────────────────────────┤
│  0 – 31  │ 0x0810_0000      │ 256 KB │ Reserved (mirrors Secure partition)  │
│          │                  │        │ Not used — FOTA writes start at s32  │
├──────────┼──────────────────┼────────┼──────────────────────────────────────┤
│ 32 – 119 │ 0x0814_0000      │ 704 KB │ FOTA target: new NS firmware         │
│          │                  │        │ Written by SECURE_FOTA_WriteChunk()  │
│          │                  │        │ After SWAP_BANK=1: NSBOOTADD maps    │
│          │                  │        │ 0x0804_0000 → Bank 2 sector 32       │
└──────────┴──────────────────┴────────┴──────────────────────────────────────┘
```

---

## SRAM Layout

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  SRAM1  (256 KB — 0x2000_0000 to 0x2003_FFFF)                               │
│  Secure alias: 0x3000_0000 to 0x3003_FFFF                                   │
├──────────────────────┬────────┬─────────────────────────────────────────────┤
│ Address              │  Size  │ Contents                                    │
├──────────────────────┼────────┼─────────────────────────────────────────────┤
│ 0x3000_0000          │ 320 KB │ Secure firmware RAM                         │
│ (alias of SRAM1+2)   │        │ Linker: RAM (xrw) @ 0x30000000              │
│                      │        │ Stack, heap, Secure globals                 │
├──────────────────────┼────────┼─────────────────────────────────────────────┤
│ 0x2005_0000          │ 320 KB │ NonSecure firmware RAM                      │
│                      │        │ Linker: RAM (xrw) @ 0x20050000              │
│                      │        │ SAU Region 2: 0x20050000–0x2009FFFF         │
└──────────────────────┴────────┴─────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│  SRAM3  (320 KB — 0x2004_0000 to 0x2008_FFFF)                               │
│  Non-Secure, Non-Privileged (ETH DMA descriptors + RX/TX buffers)           │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Option Bytes (programmed by `tools/flash_nucleo.py`)

| Option Byte    | Value      | Meaning                                              |
|----------------|------------|------------------------------------------------------|
| `TZEN`         | `0xB4`     | TrustZone enabled                                    |
| `SECWM1_STRT`  | `0x00`     | Secure watermark Bank 1 start: sector 0              |
| `SECWM1_END`   | `0x1F`     | Secure watermark Bank 1 end: sector 31 (incl. NSC)  |
| `SECWM2_STRT`  | `0x7F`     | Bank 2 fully Non-Secure (STRT > END)                 |
| `SECWM2_END`   | `0x00`     |                                                      |
| `SECBOOTADD`   | `0x0C0000` | Secure boot: `0x0C000000` (Bank 1 Secure alias)      |
| `NSBOOTADD`    | `0x80400`  | NS boot: `0x08040000` (Bank 1 sector 32)             |
| `EDATA1_EN`    | `1`        | Bank 1 EDATA enabled                                 |
| `EDATA1_STRT`  | `7`        | 8 sectors (120–127), 48 KB                           |
| `EDATA2_EN`    | `0`        | Bank 2 EDATA disabled                                |
| `SWAP_BANK`    | managed    | Managed by FOTA boot check — not set by flash script |

---

## SAU Regions

| Region | Start        | End          | Type | Purpose                              |
|--------|--------------|--------------|------|--------------------------------------|
| 0      | `0x0C03E000` | `0x0C03FFFF` | NSC  | NSC veneer (secure gateway stubs)    |
| 1      | `0x08040000` | `0x081FFFFF` | NS   | NS flash (firmware + Bank 2 FOTA)    |
| 2      | `0x20050000` | `0x2009FFFF` | NS   | NS SRAM                              |
| 3      | `0x40000000` | `0x4FFFFFFF` | NS   | Peripherals (ETH, I2C, UART, etc.)   |
| 4      | `0x08FFF000` | `0x08FFFFFF` | NS   | OTP + Read-Only (UID, flash size)    |
| 5      | `0x0BF90000` | `0x0BFA8FFF` | NS   | NS system flash (RSS/bootloader)     |

---

## Address Map Summary (SWAP_BANK=0 vs SWAP_BANK=1)

| Address          | SWAP_BANK=0                   | SWAP_BANK=1                   |
|------------------|-------------------------------|-------------------------------|
| `0x0C00_0000`    | Bank 1 physical (Secure boot) | Bank 1 physical (Secure boot) |
| `0x0C03_E000`    | Bank 1 sector 31 (NSC veneer) | Bank 1 sector 31 (NSC veneer) |
| `0x0804_0000`    | Bank 1 sector 32 (active NS)  | Bank 2 sector 32 (new NS)     |
| `0x0814_0000`    | Bank 2 sector 32 (FOTA target)| Bank 1 sector 32 (old NS)     |
| `0x0D00_0000`    | Bank 1 EDATA (calibration)    | Bank 1 EDATA (calibration)    |
| `0x0900_0000`    | Bank 1 EDATA (NS alias)       | Bank 2 EDATA (NS alias)       |

> **Key property:** `0x0C00_0000` (Secure alias) **always maps to Bank 1 physical** regardless of `SWAP_BANK`.
> The Secure firmware is safe to use `SWAP_BANK` for NS firmware updates.
>
> **Key property:** `0x0D00_0000` (Secure EDATA alias) **never changes** with `SWAP_BANK`.
> Calibration data stored here survives all FOTA firmware updates.

---

## Boot Sequence (FOTA v2)

```
External POR
  → SECBOOTADD (0x0C00_0000) → Secure firmware (Bank 1, always)
  → fota_boot_check():
      Read FOTA_PENDING_VADDR from EEPROM
      If FOTA_PENDING_MAGIC (0xF0F0F0F0):
        Toggle SWAP_BANK (HAL_FLASHEx_OBProgram + HAL_FLASH_OB_Launch)
        Verify firmware at 0x08040000 (X.509 + SHA-256)
        VALID:   clear FOTA_PENDING → boot NS from new firmware
        INVALID: toggle SWAP_BANK back + clear FOTA_PENDING → boot old firmware
      Else: boot NS normally
  → NSBOOTADD (0x0804_0000) → NS firmware
      SWAP_BANK=0: Bank 1 sector 32 (current/old firmware)
      SWAP_BANK=1: Bank 2 sector 32 (new firmware after FOTA)

FOTA download (NS firmware, no reset):
  SECURE_FOTA_EraseTarget()   — erase inactive bank sectors 32-119
  SECURE_FOTA_WriteChunk()    — write new firmware to inactive bank
  SECURE_FOTA_Stage()         — set FOTA_PENDING=0xF0F0F0F0 in EEPROM
  HTTP 200 "FOTA downloaded, activates on next power cycle"
  Current firmware keeps running until external POR
```

See [`Secure/Core/Src/main.c`](Secure/Core/Src/main.c) for the `fota_boot_check()` implementation.

---

## Key Constraints

| Constraint | Detail |
|---|---|
| **EDATA sectors are Non-Secure** | Sectors 120–127 are outside `SECWM1_END=8`. Erase/write must use `FLASH_TYPEERASE_SECTORS_NS` / `FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS` even from Secure code. Using Secure variants triggers `WRPERR`. See [`docs/debug_reports/EE_INIT_WRPERR_FIX.md`](../../../../docs/debug_reports/EE_INIT_WRPERR_FIX.md). |
| **MPU: Normal non-cacheable for EDATA** | `ARM_MPU_ATTR_NON_CACHEABLE` required. `Device-nGnRnE` causes bus faults on EDATA reads. |
| **`HAL_FLASH_Unlock()` before `EE_Init()`** | EEPROM library does not unlock flash internally. Without this, `FLASH->NSCR` is locked and all erase/write operations are silently discarded. |
| **`OB_Init()` is a no-op** | EDATA option bytes are programmed once by `tools/flash_nucleo.py --option-bytes-only`. Runtime `OB_Init()` would trigger a reset loop. |
| **Write granularity** | User flash: 128-bit (16-byte) aligned. EDATA: 16-bit or 32-bit. |
| **RWW** | Read-While-Write only across banks (not within same bank). |
| **`BKSEL` = physical bank** | `BKSEL=0` always targets Bank 1 physical regardless of `SWAP_BANK`. [RM0481 §7.6.6] |

---

## Files

| File | Purpose |
|---|---|
| [`Secure/STM32H563xx_FLASH_s.ld`](Secure/STM32H563xx_FLASH_s.ld) | Secure linker script (`FLASH=248K @ 0x0C000000`, `FLASH_NSC=8K @ 0x0C03E000`) |
| [`NonSecure/STM32H563xx_FLASH_ns.ld`](NonSecure/STM32H563xx_FLASH_ns.ld) | NS linker script (`FLASH=704K @ 0x08040000`) |
| [`Secure/Core/Inc/partition_stm32h563xx.h`](Secure/Core/Inc/partition_stm32h563xx.h) | SAU regions, NVIC ITNS routing |
| [`Secure/Core/Src/main.c`](Secure/Core/Src/main.c) | Secure init: MPU, flash unlock, EE_Init, fota_boot_check, NS jump |
| [`Secure/Core/EEPROM_Emul/Porting/STM32H5/flash_interface.c`](Secure/Core/EEPROM_Emul/Porting/STM32H5/flash_interface.c) | EEPROM porting layer (NS erase/write variants) |
| [`../../tools/flash_nucleo.py`](../../tools/flash_nucleo.py) | Programs option bytes + flashes both ELFs |
| [`../../../../plans/flash_storage_and_fota_plan.md`](../../../../plans/flash_storage_and_fota_plan.md) | Full architecture plan (EEPROM + FOTA) |
| [`../../../../docs/debug_reports/EE_INIT_WRPERR_FIX.md`](../../../../docs/debug_reports/EE_INIT_WRPERR_FIX.md) | Debug report: WRPERR root cause and fix |

 # Flash Memory: Persistent Storage & FOTA Plan

**Target MCU:** STM32H563 (STM32H5xx family)  
**Reference:** RM0481 Rev 4, Chapter 7 — Embedded Flash Memory  
**Project:** Jerry (MPCS)  
**Last revised:** Final architecture with correct boot sequence and FOTA layout

---

## 1. Flash Memory Architecture Overview

### 1.1 Memory Map (STM32H563 — 2 MB device)

| Region | Start Address | End Address | Size | Notes |
|---|---|---|---|---|
| Bank 1 (User Flash) | `0x0800_0000` | `0x080F_FFFF` | 1 MB | 128 sectors × 8 KB, 9-bit ECC/128-bit word |
| Bank 2 (User Flash) | `0x0810_0000` | `0x081F_FFFF` | 1 MB | 128 sectors × 8 KB, 9-bit ECC/128-bit word |
| EDATA NS base | `0x0900_0000` | `0x0901_7FFF` | 96 KB | `FLASH_EDATA_BASE_NS` — swaps with banks |
| EDATA S base | `0x0D00_0000` | `0x0D01_7FFF` | 96 KB | `FLASH_EDATA_BASE_S` — **does NOT swap, always Bank 1** |
| OTP Area | `0x08FF_F000` | `0x08FF_F7FF` | 2 KB | Write-once, 6-bit ECC |
| Read-Only Area | `0x08FF_F800` | `0x08FF_FFFF` | 2 KB | Unique Device ID, Package info |
| System Memory | `0x0BF8_0000` | `0x0BF9_FFFF` | 128 KB | ST Bootloader / RSS |

### 1.2 Key Hardware Features

| Feature | Detail |
|---|---|
| Write granularity (user flash) | 128-bit word (16 bytes), aligned |
| Write granularity (high-cycle data) | 16-bit or 32-bit |
| Erase granularity | 8 KB sector, bank, or mass erase |
| ECC (user flash) | 9-bit SECDED per 128-bit word |
| ECC (high-cycle data) | 6-bit per 16-bit word |
| Endurance (user flash) | ~10K erase cycles |
| Endurance (high-cycle data) | **100K erase cycles** |
| Read-While-Write (RWW) | Supported **across banks** (not within same bank) |
| Bank swap | `SWAP_BANK` option bit — effective after system reset |
| `SECBOOTADD` | Fixed Secure boot address — always Bank 1 physical via Secure alias `0x0C00_0000` |
| `NSBOOTADD` | Fixed NS boot address — `0x0800_8000` (Bank 1 sector 4 when SWAP_BANK=0, Bank 2 sector 4 when SWAP_BANK=1) |
| `BKSEL` always = physical bank | `BKSEL=0` always targets Bank 1 physical regardless of `SWAP_BANK` (RM0481 §7.6.6) |

### 1.3 Boot Sequence (TrustZone Active)

```
Power-on / Reset
      │
      ▼
Hardware loads option bytes (SECBOOTADD, NSBOOTADD, SWAP_BANK, SECWM, etc.)
      │
      ▼
CPU jumps to SECBOOTADD = 0x0C00_0000
(Secure alias of Bank 1 physical — ALWAYS Bank 1, regardless of SWAP_BANK)
      │
      ▼
Secure firmware runs (Bank 1, sectors 0–8, 72 KB)
- Sets up TrustZone (SAU, GTZC, MPU)
- Unlocks flash (HAL_FLASH_Unlock)
- Initializes EEPROM emulation on Bank 1 EDATA (via 0x0D000000)
- Exposes NSC services
      │
      ▼
Secure firmware jumps to NSBOOTADD = 0x0801_2000
(Bank 1 sector 9 when SWAP_BANK=0 → active NS firmware)
(Bank 2 sector 9 when SWAP_BANK=1 → new NS firmware after FOTA)
      │
      ▼
NonSecure firmware runs
```

**Why `SECBOOTADD` is always Bank 1:**
- `SECBOOTADD = 0x0C00_0000` is the Secure AHB alias
- The Secure alias always maps to Bank 1 physical, regardless of `SWAP_BANK`
- The Secure firmware is permanently in Bank 1 and is never touched by FOTA

**Why `NSBOOTADD = 0x0801_2000` works for FOTA:**
- `SWAP_BANK=0`: `0x0801_2000` → Bank 1 physical sector 9 → current NS firmware
- `SWAP_BANK=1`: `0x0801_2000` → Bank 2 physical sector 9 → new NS firmware (written during FOTA)
- The address `0x0801_2000` is fixed in option bytes — it automatically points to the right firmware after swap

### 1.4 EDATA Address Behaviour After Bank Swap (RM0481 §7.3.10 and §7.6.6)

From RM0481 §7.3.10:
> *"When SWAP_BANK feature is enabled, the banks are swapped: the flash high-cycle data in Bank 2 are accessible from 0x0900_0000 to 0x0900_BFFF, and the data in Bank 1 are accessible from 0x0900_C000 to 0x0901_7FFF."*

The NS EDATA alias (`0x0900_0000`) swaps with the banks. The Secure alias (`0x0D000000`) does NOT swap — it always points to Bank 1 EDATA. This is why the Secure firmware uses `0x0D000000` for EEPROM emulation.

---

## 2. Final Flash Memory Layout

### 2.1 Physical Layout (Both Banks)

```
Bank 1 (1 MB) — Physical 0x0800_0000 (Secure alias: 0x0C00_0000)
├── Sectors 0–7   (64 KB)  — Secure firmware code
│                            [actual firmware size ~44.5 KB; 8 sectors (64 KB) provide future headroom]
├── Sector 8      (8 KB)   — NSC veneer (SECURE_CAL_*, SECURE_FLASH_*, SECURE_FOTA_*, SECURE_RTC_*)
│                            (last sector of Secure watermark, SECWM1_END=8)
│                            Secure alias: 0x0C01_0000
├── Sectors 9–119 (888 KB) — NS firmware (active, SWAP_BANK=0)
│                            Starts at 0x0801_2000 = NSBOOTADD
│                            [RM0481 §7.3.3 Table 48: sector 9 = base+0x12000]
└── Sectors 120–127 (48 KB) → remapped as EDATA1
                               [RM0481 §7.3.10 lines 940–942: "120-127 for STM32H562/563/573xx"]
                               Accessible at 0x0D00_0000 (Secure alias — STABLE)
                               EEPROM emulation: calibration, config, FOTA flag
                               100K erase cycles, SURVIVES ALL FOTA UPDATES

Bank 2 (1 MB) — Physical 0x0810_0000 (NonSecure)
├── Sectors 0–8   (72 KB)  — Reserved / padding (mirrors Secure partition offset)
└── Sectors 9–127 (888 KB) — FOTA target: new NS firmware written here
                              Starts at 0x0811_2000
                              After SWAP_BANK=1: NSBOOTADD (0x0801_2000) maps here
```

### 2.2 Address Map Summary

| Address | SWAP_BANK=0 | SWAP_BANK=1 | Purpose |
|---|---|---|---|
| `0x0C00_0000` | Bank 1 physical | Bank 1 physical | Secure boot (always Bank 1) |
| `0x0C01_0000` | Bank 1 sector 8 (Secure alias) | Bank 1 sector 8 (Secure alias) | NSC veneer — stable |
| `0x0800_0000` | Bank 1 physical | Bank 2 physical | User flash base |
| `0x0801_2000` = `NSBOOTADD` | Bank 1 sector 9 (active NS) | Bank 2 sector 9 (new NS) | NS firmware entry [RM0481 Table 48: sector 9 = base+0x12000] |
| `0x0810_0000` | Bank 2 physical | Bank 1 physical | Second bank base [RM0481 Table 48] |
| `0x0811_2000` | Bank 2 sector 9 (FOTA target) | Bank 1 sector 9 (old NS) | FOTA write target |
| `0x0D00_0000` | Bank 1 EDATA (Secure alias) | Bank 1 EDATA (Secure alias) | Calibration — **never changes** |

### 2.3 Post-FOTA Boot Sequence (SWAP_BANK=1)

```
After FOTA reset with SWAP_BANK=1:

0x0C00_0000 → Bank 1 physical sector 0 → Secure firmware (unchanged) ✓
0x0801_2000 → Bank 2 physical sector 9 → New NS firmware ✓  (NSBOOTADD fixed, bank changes)
0x0D00_0000 → Bank 1 EDATA → Calibration data intact ✓
```

---

## 3. TrustZone Configuration

### 3.1 Secure Watermark

Only sectors 0–8 (72 KB) of Bank 1 are Secure:

```
SECWM1_STRT = 0    (sector 0 = start of Secure area)
SECWM1_END  = 8    (sector 8 = end of Secure area, includes NSC veneer)
```

Sectors 9–127 of Bank 1 are **NonSecure** — accessible by both Secure and NS code. The NS firmware runs from sector 9 onwards.

### 3.2 Boot Addresses

```
SECBOOTADD = 0x0C00_0000   (Bank 1 physical sector 0, Secure alias — fixed, never changes)
NSBOOTADD  = 0x0801_2000   (Bank 1 sector 9 when SWAP_BANK=0,
                             Bank 2 sector 9 when SWAP_BANK=1 — address fixed, bank changes)
```

Programmed by `tools/flash_nucleo.py`:
```python
secbootadd: int = 0x0C0000   # 0x0C000000 >> 8
nsbootadd:  int = 0x80120    # 0x08012000 >> 8
```

---

## 4. Core Design Principle: All Flash Operations in Secure World

With TrustZone enabled, the NonSecure application **must not** perform any flash operations directly. All flash operations are delegated to the Secure firmware via NSC functions.

```
NonSecure World (Jerry app)              Secure World (Secure firmware)
        │                                          │
        │  SECURE_CAL_Read(vaddr, &val) ──────────►│  EEPROM emulation on Bank 1 EDATA
        │  SECURE_CAL_Write(vaddr, val) ──────────►│  via 0x0D000000 (Secure alias — stable)
        │                                          │
        │  SECURE_FLASH_Write(addr, data, len) ───►│  Generic flash write (validated)
        │  SECURE_FLASH_Erase(addr, len) ─────────►│  Generic flash erase (validated)
        │  SECURE_FLASH_Read(addr, buf, len) ──────►│  Generic flash read (validated)
        │                                          │
        │  SECURE_FOTA_EraseTarget() ─────────────►│  Erase Bank 2 sectors 9–127
        │  SECURE_FOTA_WriteChunk(off, data, len) ►│  Write firmware to Bank 2 from sector 9
        │  SECURE_FOTA_Commit(size, crc) ─────────►│  Verify + toggle SWAP_BANK + reset
        │  SECURE_FOTA_SetValidFlag() ────────────►│  Mark new firmware as valid
        │  SECURE_FOTA_Rollback() ────────────────►│  Toggle SWAP_BANK back + reset
```

---

## 5. Requirement 1: Persistent Storage (Calibration Data)

### 5.1 Existing Infrastructure

The project already contains ST's EEPROM Emulation middleware:

- [`application/bsp/eeprom_emul_conf.h`](../application/bsp/eeprom_emul_conf.h) — Configuration
- [`application/bsp/stm/stm32h563/Middlewares/ST/EEPROM_Emul/Porting/STM32H5/flash_interface.h`](../application/bsp/stm/stm32h563/Middlewares/ST/EEPROM_Emul/Porting/STM32H5/flash_interface.h) — HAL porting layer
- [`application/bsp/stm/stm32h563/Middlewares/ST/EEPROM_Emul/Porting/STM32H5/flash_interface.c`](../application/bsp/stm/stm32h563/Middlewares/ST/EEPROM_Emul/Porting/STM32H5/flash_interface.c) — Implementation
- [`application/bsp/stm/stm32h563/Middlewares/ST/EEPROM_Emul/Core/eeprom_emul.h`](../application/bsp/stm/stm32h563/Middlewares/ST/EEPROM_Emul/Core/eeprom_emul.h) — Core API

**Current (incorrect) configuration:**
```c
#define START_PAGE_ADDRESS  0x081F0000U  // Last sector of Bank 2 — erased by FOTA!
```

### 5.2 EEPROM Emulation in Secure Firmware

The EEPROM emulation middleware moves to the **Secure firmware** build. It runs on Bank 1 EDATA accessed via the **Secure alias `0x0D000000`**, which is stable across all FOTA bank swaps.

#### 5.2.1 How the Library Selects the EDATA Address

From [`stm32h563xx.h`](../application/bsp/stm/stm32h563/Drivers/CMSIS/Device/ST/STM32H5xx/Include/stm32h563xx.h:2624):

```c
// When compiled as Secure (CMSE=3):
#define FLASH_EDATA_BASE   FLASH_EDATA_BASE_S   // = 0x0D000000 (Secure alias — STABLE)

// When compiled as NonSecure:
#define FLASH_EDATA_BASE   FLASH_EDATA_BASE_NS  // = 0x09000000 (NS alias — swaps after FOTA!)
```

**The standard Secure build (CMSE=3) automatically uses `FLASH_EDATA_BASE_S = 0x0D000000`. No `SECURE_FEATURES` macro is needed or wanted** — that macro would force `0x09000000` which swaps after FOTA.

#### 5.2.2 Required Compile Definitions (Secure Build)

```cmake
# Secure CMakeLists.txt
target_compile_definitions(secure_firmware PRIVATE
    EDATA_ENABLED      # Use high-cycle data area instead of regular flash
    # Do NOT define SECURE_FEATURES — standard Secure build uses 0x0D000000 automatically
)
```

#### 5.2.3 Option Byte Configuration — `OB_Init()`

The library's [`OB_Init()`](../application/bsp/stm/stm32h563/Middlewares/ST/EEPROM_Emul/Porting/STM32H5/flash_interface.c:350) programs EDATA option bytes. It must be called from **Secure firmware `main()` only** — it triggers a system reset on first boot.

**Preferred:** Pre-program via STM32CubeProgrammer (production).

#### 5.2.4 Updated `eeprom_emul_conf.h` (Secure Build)

```c
// With EDATA_ENABLED in Secure build (CMSE=3):
// - FLASH_EDATA_BASE = 0x0D000000 (Secure alias, always Bank 1 EDATA)
// - START_PAGE_ADDRESS not used when EDATA_ENABLED is defined

#define CYCLES_NUMBER       1U    // 100K cycles via EDATA
#define GUARD_PAGES_NUMBER  2U
#define NB_OF_VARIABLES     200U  // Adjust to actual count
```

#### 5.2.5 Secure Firmware Initialization Sequence

```c
int main(void)  // Secure main.c
{
    HAL_Init();
    SystemClock_Config();
    MX_GTZC_S_Init();             // Configure GTZC: ETH, I2C3 as NS; SRAM3 non-privileged
    MX_GPIO_Init();               // Configure GPIO security attributes
    MX_ICACHE_Init();
    MX_RTC_Init();
    MPU_Config_EDATA();           // Mark 0x0D000000-0x0D017FFF as non-cacheable (Normal memory)
    HAL_FLASH_Unlock();           // Required: EEPROM library assumes flash is pre-unlocked
    EE_Init(EE_FORCED_ERASE);     // Initialize EEPROM emulation via 0x0D000000
    SysTick->CTRL = 0;            // Disable Secure SysTick before NS jump
    NVIC_NS->ICPR[0..4] = 0xFFFFFFFF; // Clear stale NS pending interrupts
    NonSecure_Init();             // Jump to NSBOOTADD = 0x0801_2000
}
```

**Note:** `OB_Init()` is a **no-op** in the Secure firmware. EDATA option bytes
(`EDATA1_EN=1, EDATA1_STRT=7`) are programmed once during board setup by
`tools/flash_nucleo.py --option-bytes-only`. This avoids a reset loop on first boot.

### 5.3 NSC Interface for Calibration Access

```c
// secure_nsc.h
uint32_t SECURE_CAL_Read(uint16_t vaddr, uint32_t *pValue);
uint32_t SECURE_CAL_Write(uint16_t vaddr, uint32_t value);
```

### 5.4 Virtual Address Map

4 ADC channels × 3 `float` parameters = 12 variables. Floats are stored as `uint32_t` via bit-cast.
Address formula: `CAL_ADC_VADDR(ch, param)` = `0x0001 + ch*3 + param_offset`

| Virtual Address | Macro | Type | Description |
|---|---|---|---|
| `0x0001` | `CAL_ADC_VADDR(0, 0)` | `float` | CH0 `scaling_factor` |
| `0x0002` | `CAL_ADC_VADDR(0, 1)` | `float` | CH0 `offset_term` |
| `0x0003` | `CAL_ADC_VADDR(0, 2)` | `float` | CH0 `deadzone` |
| `0x0004` | `CAL_ADC_VADDR(1, 0)` | `float` | CH1 `scaling_factor` |
| `0x0005` | `CAL_ADC_VADDR(1, 1)` | `float` | CH1 `offset_term` |
| `0x0006` | `CAL_ADC_VADDR(1, 2)` | `float` | CH1 `deadzone` |
| `0x0007` | `CAL_ADC_VADDR(2, 0)` | `float` | CH2 `scaling_factor` |
| `0x0008` | `CAL_ADC_VADDR(2, 1)` | `float` | CH2 `offset_term` |
| `0x0009` | `CAL_ADC_VADDR(2, 2)` | `float` | CH2 `deadzone` |
| `0x000A` | `CAL_ADC_VADDR(3, 0)` | `float` | CH3 `scaling_factor` |
| `0x000B` | `CAL_ADC_VADDR(3, 1)` | `float` | CH3 `offset_term` |
| `0x000C` | `CAL_ADC_VADDR(3, 2)` | `float` | CH3 `deadzone` |
| `0x0010` | `CAL_MODBUS_ADDR_VADDR` | `uint32_t` | Modbus device address (1–247) |
| `0x0011` | `CAL_BAUD_RATE_VADDR` | `uint32_t` | Baud rate configuration |
| `0x0030` | `FOTA_VALID_FLAG_VADDR` | `uint32_t` | FOTA firmware valid flag (`0xA5A5A5A5`) |

Total: 15 variables. `NB_OF_VARIABLES = 20` (headroom for future additions).

**Float bit-cast pattern (NS caller):**
```c
/* Write float calibration value */
uint32_t bits;
memcpy(&bits, &scaling_factor, sizeof(float));
SECURE_CAL_Write(CAL_ADC_VADDR(ch, CAL_ADC_SCALING_FACTOR_OFF), bits);

/* Read float calibration value */
uint32_t bits;
float val;
SECURE_CAL_Read(CAL_ADC_VADDR(ch, CAL_ADC_SCALING_FACTOR_OFF), &bits);
memcpy(&val, &bits, sizeof(float));
```

---

## 6. Generic Secure Flash Service (Large Data > 48 KB)

```c
// secure_nsc.h
uint32_t SECURE_FLASH_Write(uint32_t addr, const uint8_t *pData, uint32_t len);
uint32_t SECURE_FLASH_Erase(uint32_t addr, uint32_t len);
uint32_t SECURE_FLASH_Read(uint32_t addr, uint8_t *pBuf, uint32_t len);
```

Allowed regions table in Secure firmware restricts writes to pre-approved NS data areas only.

---

## 7. Requirement 2: FOTA (Firmware Over-The-Air)

### 7.1 FOTA Architecture

```
NonSecure World (Jerry app)          Secure World (Secure firmware)
        │                                        │
        │  Receive firmware via TCP              │
        │  Buffer chunks in SRAM                 │
        │                                        │
        │──── SECURE_FOTA_EraseTarget() ────────►│  Erase Bank 2 sectors 4–127
        │◄─── return OK ─────────────────────────│  (0x0810_8000 – 0x081F_FFFF)
        │                                        │
        │──── SECURE_FOTA_WriteChunk(            │
        │       offset, data, len) ─────────────►│  Write to Bank 2 from 0x0810_8000
        │◄─── return OK ─────────────────────────│  (RWW: Bank 1 still runs)
        │  (repeat for all chunks)               │
        │                                        │
        │──── SECURE_FOTA_Commit(size, crc) ────►│  Verify CRC → toggle SWAP_BANK → reset
        │                                        │
        ▼ (System Reset)                         │
        │                                        │
        │  SECBOOTADD → Bank 1 → Secure boots ✓  │
        │  NSBOOTADD (0x0800_8000) → Bank 2      │
        │  sector 4 → New NS firmware runs ✓     │
        │  0x0D000000 → Bank 1 EDATA intact ✓    │
        │                                        │
        │──── SECURE_CAL_Read(FOTA_VALID_FLAG) ─►│  Read from 0x0D000000 (stable)
        │  If not set → SECURE_FOTA_Rollback()   │
        │──── SECURE_CAL_Write(FOTA_VALID_FLAG) ►│  Set after self-test passes
```

### 7.2 FOTA Target Address Calculation

```c
// secure_fota.c

#define FOTA_SECTOR_OFFSET   4U          // Skip sectors 0–3 (Secure partition)
#define FOTA_BYTE_OFFSET     (FOTA_SECTOR_OFFSET * 8192U)  // 32 KB offset

static uint32_t fota_get_target_base(void)
{
    bool swapped = (FLASH->OPTSR_CUR & FLASH_OPTSR_CUR_SWAP_BANK_Msk) != 0U;
    // When SWAP_BANK=0: NS firmware is in Bank 1 → write new firmware to Bank 2
    // When SWAP_BANK=1: NS firmware is in Bank 2 → write new firmware to Bank 1
    // Always skip first 32 KB (sectors 0–3) to avoid overwriting Secure partition
    uint32_t bank_base = swapped ? 0x08000000U : 0x08100000U;
    return bank_base + FOTA_BYTE_OFFSET;  // 0x0810_8000 (SWAP_BANK=0) or 0x0800_8000 (SWAP_BANK=1)
}

__attribute__((cmse_nonsecure_entry))
uint32_t SECURE_FOTA_EraseTarget(void)
{
    uint32_t target_base = fota_get_target_base();
    bool swapped = (FLASH->OPTSR_CUR & FLASH_OPTSR_CUR_SWAP_BANK_Msk) != 0U;
    uint32_t bank = swapped ? FLASH_BANK_1 : FLASH_BANK_2;

    // Erase sectors 4–127 of the target bank (skip sectors 0–3 = Secure partition mirror)
    // [RM0481 §7.3.6: sector erase via FLASH_NSCR/SECCR SER+SNB+STRT]
    // Bank 2 has no EDATA reservation — all 124 sectors (4–127) available for NS firmware
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .Banks     = bank,
        .Sector    = FOTA_SECTOR_OFFSET,          // Start at sector 4
        .NbSectors = 128U - FOTA_SECTOR_OFFSET    // 124 sectors (4–127)
    };
    uint32_t error = 0;
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef ret = HAL_FLASHEx_Erase(&erase, &error);
    HAL_FLASH_Lock();
    return (ret == HAL_OK && error == 0xFFFFFFFFU) ? 0U : 1U;
}

__attribute__((cmse_nonsecure_entry))
uint32_t SECURE_FOTA_WriteChunk(uint32_t offset, const uint8_t *pData, uint32_t len)
{
    if (cmse_check_address_range((void*)pData, len, CMSE_NONSECURE) == NULL) return 2U;
    if ((offset % 16U) != 0U || (len % 16U) != 0U) return 3U;

    uint32_t base = fota_get_target_base();
    HAL_FLASH_Unlock();
    for (uint32_t i = 0; i < len; i += 16U) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD,
                              base + offset + i,
                              (uint32_t)(pData + i)) != HAL_OK) {
            HAL_FLASH_Lock();
            return 4U;
        }
    }
    HAL_FLASH_Lock();
    return 0U;
}

__attribute__((cmse_nonsecure_entry))
uint32_t SECURE_FOTA_Commit(uint32_t fw_size, uint32_t expected_crc)
{
    uint32_t base = fota_get_target_base();
    uint32_t computed = HAL_CRC_Calculate(&hcrc_s, (uint32_t*)base, fw_size / 4U);
    if (computed != expected_crc) return 5U;

    // Toggle SWAP_BANK
    bool swapped = (FLASH->OPTSR_CUR & FLASH_OPTSR_CUR_SWAP_BANK_Msk) != 0U;
    HAL_FLASH_OB_Unlock();
    FLASH_OBProgramInitTypeDef ob = {0};
    ob.OptionType  = OPTIONBYTE_USER;
    ob.USERType    = OB_USER_SWAP_BANK;
    ob.USERConfig  = swapped ? OB_SWAP_BANK_DISABLE : OB_SWAP_BANK_ENABLE;
    HAL_FLASHEx_OBProgram(&ob);
    HAL_FLASH_OB_Launch();  // Triggers reset — does not return
    return 0U;
}
```

### 7.3 NSC Interface for FOTA

```c
// secure_nsc.h
uint32_t SECURE_FOTA_EraseTarget(void);
uint32_t SECURE_FOTA_WriteChunk(uint32_t offset, const uint8_t *pData, uint32_t len);
uint32_t SECURE_FOTA_Commit(uint32_t fw_size, uint32_t expected_crc);
void     SECURE_FOTA_Rollback(void);
```

### 7.4 NonSecure FOTA Task (`application/src/fota_task.c`)

```c
#include "secure_nsc.h"

static uint8_t fota_buf[4096U] __attribute__((aligned(16)));

void fota_startup_check(void)
{
    uint32_t valid_flag = 0U;
    SECURE_CAL_Read(FOTA_VALID_FLAG_VADDR, &valid_flag);  // reads from 0x0D000000 — stable
    if (valid_flag != 0xA5A5A5A5U) {
        SECURE_FOTA_Rollback();  // Does not return
    }
}

void fota_on_chunk_received(uint32_t offset, const uint8_t *data,
                             uint32_t len, uint32_t total_size,
                             uint32_t expected_crc)
{
    memcpy(fota_buf, data, len);
    uint32_t ret = SECURE_FOTA_WriteChunk(offset, fota_buf, len);
    if (ret != 0U) { fota_handle_error(ret); return; }

    if (offset + len >= total_size) {
        uint32_t padded = (total_size + 15U) & ~15U;
        ret = SECURE_FOTA_Commit(padded, expected_crc);
        fota_handle_error(ret);  // Only reached on CRC failure
    }
}

void fota_mark_valid(void)
{
    SECURE_CAL_Write(FOTA_VALID_FLAG_VADDR, 0xA5A5A5A5U);
}
```

### 7.5 FOTA State Machine

```
IDLE
  │
  ▼ (new firmware notification via TCP/Modbus)
ERASING
  │  SECURE_FOTA_EraseTarget()
  │  Erases Bank 2 sectors 4–127 (0x0810_8000 – 0x081F_FFFF)
  │
  ▼
DOWNLOADING
  │  Receive chunks via TCP
  │  SECURE_FOTA_WriteChunk() per chunk
  │  RWW: Bank 1 (Secure + active NS) keeps running
  │
  ▼ (all chunks received)
COMMITTING
  │  SECURE_FOTA_Commit(size, crc)
  │  Secure verifies CRC → toggles SWAP_BANK → OB_Launch → reset
  │
  ▼ (after reset)
  │  SECBOOTADD → Bank 1 → Secure firmware boots (unchanged) ✓
  │  NSBOOTADD (0x0800_8000) → Bank 2 sector 4 → New NS firmware ✓
  │
VALIDATING
  │  fota_startup_check() — IWDG running
  │  SECURE_CAL_Read(FOTA_VALID_FLAG) via 0x0D000000 — Bank 1 EDATA, intact
  │  Run self-test
  │  SECURE_CAL_Write(FOTA_VALID_FLAG, 0xA5A5A5A5)
  │
  ├─ FAIL / watchdog reset ──► SECURE_FOTA_Rollback() → toggle SWAP_BANK back → reset
  │
  ▼
RUNNING (new firmware active)
```

---

## 8. Implementation Checklist

### 8.1 Persistent Storage

**Option Bytes (programmed by `tools/flash_nucleo.py`):**
- [x] `EDATA1_EN = 1`, `EDATA1_STRT = 7` — Bank 1 high-cycle data area (48 KB, sectors 120-127)
- [x] `EDATA2_EN = 0` — Bank 2 EDATA disabled
- [x] `SECWM1_STRT = 0`, `SECWM1_END = 8` — Sectors 0–8 (72 KB) are Secure (code + NSC veneer)
- [x] `SECWM2_STRT = 0x7F`, `SECWM2_END = 0x00` — Bank 2 fully Non-Secure
- [x] `SECBOOTADD = 0x0C00_0000` — Secure boot from Bank 1 physical (default)
- [x] `NSBOOTADD = 0x0801_2000` — NS boot from sector 9 (Bank 1 when SWAP_BANK=0, Bank 2 when SWAP_BANK=1)
- [x] `SWAP_BANK = 0` — Initial state
- [x] `NSBOOT_LOCK = 0xC3` — Allow `SWAP_BANK` modification

**Secure Firmware Build:**
- [ ] Move `eeprom_emul_conf.h` and EEPROM emulation middleware to Secure build
- [ ] Add `EDATA_ENABLED` compile definition to Secure `CMakeLists.txt`
- [ ] **Do NOT add `SECURE_FEATURES`** — standard Secure build (CMSE=3) uses `FLASH_EDATA_BASE_S = 0x0D000000` automatically
- [ ] Configure Secure MPU Region 0: `0x0D00_0000`, 48 KB, non-cacheable
- [ ] Call `OB_Init()` in Secure `main()` before `EE_Init()`
- [ ] Call `EE_Init(EE_FORCED_ERASE)` in Secure `main()`
- [ ] Add CRC peripheral initialization (`hcrc_s`)
- [ ] Implement `SECURE_CAL_Read()` and `SECURE_CAL_Write()` NSC functions
- [ ] Add declarations to `secure_nsc.h`

**NonSecure Firmware:**
- [ ] Remove `EDATA_ENABLED` from NS build
- [ ] Replace direct `EE_*` calls with `SECURE_CAL_Read()` / `SECURE_CAL_Write()`
- [ ] Create `calibration_storage.c/.h` wrapping NSC calls
- [ ] Define virtual address constants (see Section 5.4)

### 8.2 Generic Flash Service

- [ ] Implement `SECURE_FLASH_Write()`, `SECURE_FLASH_Erase()`, `SECURE_FLASH_Read()` NSC functions
- [ ] Define `allowed_regions[]` table in Secure firmware
- [ ] Add declarations to `secure_nsc.h`

### 8.3 FOTA

**Secure Firmware:**
- [ ] Implement `SECURE_FOTA_EraseTarget()` — erases sectors 4–127 of target bank
- [ ] Implement `SECURE_FOTA_WriteChunk()` — writes to target bank from sector 4 offset
- [ ] Implement `SECURE_FOTA_Commit()` — CRC verify + toggle `SWAP_BANK`
- [ ] Implement `SECURE_FOTA_Rollback()` — toggle `SWAP_BANK` back + reset
- [ ] Add all declarations to `secure_nsc.h`

**NonSecure Firmware (`application/src/fota_task.c`):**
- [ ] Implement `fota_startup_check()` — reads FOTA valid flag via `SECURE_CAL_Read()`
- [ ] Implement `fota_on_chunk_received()` — calls `SECURE_FOTA_WriteChunk()`
- [ ] Implement `fota_mark_valid()` — calls `SECURE_CAL_Write(FOTA_VALID_FLAG_VADDR, ...)`
- [ ] Allocate 16-byte aligned SRAM staging buffer
- [ ] Configure IWDG with appropriate timeout for self-test window
- [ ] Define FOTA protocol over TCP (chunk size, sequence numbers, CRC field)
- [ ] Add FOTA status reporting via Modbus holding registers

---

## 9. Key Constraints and Gotchas

| Constraint | Detail |
|---|---|
| **`SECBOOTADD` always = Bank 1** | `0x0C00_0000` is the Secure alias — always maps to Bank 1 physical regardless of `SWAP_BANK`. Secure firmware is permanently in Bank 1. |
| **`NSBOOTADD = 0x0801_2000` is fixed** | After `SWAP_BANK=1`, this address maps to Bank 2 sector 9 — the new NS firmware. The address is fixed; the bank it points to changes. |
| **FOTA skips sectors 0–8** | Sectors 0–8 of the target bank are reserved (mirror of Secure partition offset). FOTA writes start at sector 9 (`+72 KB` offset). |
| **NS firmware size limit** | With 72 KB reserved for Secure (sectors 0–8) + 48 KB for EDATA (sectors 120–127), NS firmware is limited to **888 KB** per bank (sectors 9–119). [RM0481 §7.3.10 lines 940–942: EDATA sectors are 120–127 for STM32H563] |
| **`0x0D000000` is stable** | `FLASH_EDATA_BASE_S = 0x0D000000` always points to Bank 1 EDATA regardless of `SWAP_BANK`. Calibration data survives all FOTA updates. |
| **Do NOT use `SECURE_FEATURES`** | That macro forces `0x09000000` (NS alias) which swaps after FOTA. Standard Secure build (CMSE=3) uses `0x0D000000` automatically. |
| **`SWAP_BANK` must be toggled** | `SECURE_FOTA_Commit()` must **toggle** (not set) `SWAP_BANK` to handle multiple FOTA cycles. |
| **`BKSEL` always = physical bank** | `BKSEL=0` always targets Bank 1 physical regardless of `SWAP_BANK` (RM0481 §7.6.6). |
| Write granularity | User flash requires **128-bit (16-byte) aligned** writes. |
| No write-while-write | RWW only works **across banks**. |
| EDATA MPU | Secure MPU **must** mark `0x0D00_0000` as non-cacheable before any EDATA access. |
| ECC virgin read | Reading unwritten EDATA generates double ECC error (NMI). `EE_Init()` handles initialization. |

---

## 10. Linker Script Changes

### 10.1 Secure Linker Script (`STM32H563xx_FLASH_s.ld`)

```ld
MEMORY
{
  RAM        (xrw) : ORIGIN = 0x30000000, LENGTH = 320K

  /* Secure firmware: Bank 1, sectors 0–7 (64 KB code)
   * Actual firmware size ~44.5 KB; 8 sectors (64 KB) provides future headroom.
   * Sector 8 (8 KB) = NSC veneer
   * Total Secure watermark = 72 KB (sectors 0–8, SECWM1_STRT=0, SECWM1_END=8) */
  FLASH      (rx)  : ORIGIN = 0x0C000000, LENGTH = 64K   /* Sectors 0–7: 8 × 8 KB = 64 KB */
  FLASH_NSC  (rx)  : ORIGIN = 0x0C010000, LENGTH = 8K    /* Sector 8: NSC veneer */

  /* EDATA1: Bank 1 high-cycle data (sectors 120–127)
   * [RM0481 §7.3.10 lines 940–942: "120-127 for STM32H562/563/573xx devices"]
   * Accessible via Secure alias 0x0D000000 — STABLE, does not swap
   * [RM0481 §7.3.10 lines 913–915: "data are accessible from 0x0900_0000 to 0x0900_BFFF for Bank 1"]
   * [Secure alias 0x0D000000 is separate AHB path — not in Table 48 swap range] */
  EDATA_S    (rw)  : ORIGIN = 0x0D000000, LENGTH = 48K

  /* FOTA target banks — symbols only, not used for code placement
   * NS firmware occupies sectors 9–119 = 111 × 8 KB = 888 KB */
  FOTA_BANK_A (rw) : ORIGIN = 0x08112000, LENGTH = 888K  /* Bank 2 sectors 9–119 (SWAP_BANK=0) */
  FOTA_BANK_B (rw) : ORIGIN = 0x08012000, LENGTH = 888K  /* Bank 1 sectors 9–119 (SWAP_BANK=1) */
}
```

### 10.2 NonSecure Linker Script (`STM32H563xx_FLASH_ns.ld`)

```ld
MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20050000, LENGTH = 320K

  /* NS firmware: Bank 1 sectors 9–119 (888 KB)
   * [RM0481 §7.3.10: EDATA sectors are 120–127, so NS code ends at sector 119]
   * Starts at 0x0801_2000 = NSBOOTADD
   * No EDATA region — calibration is Secure-only */
  FLASH (rx)  : ORIGIN = 0x08012000, LENGTH = 888K
}
```

> **Note:** After `SWAP_BANK=1`, the NS firmware executes from Bank 2 sector 9 (`0x0811_2000` physical), but the linker script `ORIGIN = 0x08012000` is still correct because `NSBOOTADD = 0x0801_2000` maps to Bank 2 sector 9 after the swap. The firmware is position-independent relative to `NSBOOTADD`.

### 10.3 Option Byte vs Linker Script Alignment

| Setting | Option Byte | Linker Script |
|---|---|---|
| Secure area | `SECWM1_STRT=0, SECWM1_END=8` | `FLASH: ORIGIN=0x0C000000, LENGTH=64K` + `FLASH_NSC: ORIGIN=0x0C010000, LENGTH=8K` |
| NS firmware start | `NSBOOTADD=0x0801_2000` | `FLASH: ORIGIN=0x08012000` (NS linker) |
| Bank 1 EDATA | `EDATA1_EN=1, EDATA1_STRT=7` | `EDATA_S: ORIGIN=0x0D000000, LENGTH=48K` |
| Bank swap | `SWAP_BANK=0` (initial) | FOTA toggles this at runtime |

---

## 11. MPU Configuration

### 11.1 Secure MPU

```c
/* MPU_Config_EDATA() in Secure main.c */
static void MPU_Config_EDATA(void)
{
    MPU_Region_InitTypeDef     MPU_InitStruct     = {0};
    MPU_Attributes_InitTypeDef MPU_AttributesInit = {0};

    HAL_MPU_Disable();

    /* Attribute 0: Normal memory, Non-cacheable.
     * EDATA is flash (not a device) — must use Normal, not Device memory.
     * Device-nGnRnE would cause bus faults on EDATA reads.
     * Non-cacheable required: EDATA uses 6-bit ECC on 16-bit words;
     * caching 128-bit AHB words would corrupt the 16-bit ECC granularity. */
    MPU_AttributesInit.Number     = MPU_ATTRIBUTES_NUMBER0;
    MPU_AttributesInit.Attributes = ARM_MPU_ATTR(ARM_MPU_ATTR_NON_CACHEABLE,
                                                  ARM_MPU_ATTR_NON_CACHEABLE);
    HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);

    /* Region 0: EDATA1 — Bank 1 high-cycle data (Secure alias 0x0D000000)
     * Base:  0x0D000000, Limit: 0x0D017FFF (96KB covers all EDATA)
     * Privileged read/write only, Execute Never */
    MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
    MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress      = 0x0D000000U;
    MPU_InitStruct.LimitAddress     = 0x0D017FFFU;
    MPU_InitStruct.AttributesIndex  = MPU_ATTRIBUTES_NUMBER0;
    MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RW;
    MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
```

### 11.2 NonSecure MPU

```c
void MPU_Config_NonSecure(void)
{
    HAL_MPU_Disable();

    /* Region 0: Ethernet DMA buffers — non-cacheable */
    MPU_Region_InitTypeDef mpu = {
        .Enable = MPU_REGION_ENABLE, .Number = MPU_REGION_NUMBER0,
        .BaseAddress = 0x20050000U, .Size = MPU_REGION_SIZE_16KB,
        .TypeExtField = MPU_TEX_LEVEL1, .AccessPermission = MPU_REGION_FULL_ACCESS,
        .DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE,
        .IsShareable = MPU_ACCESS_NOT_SHAREABLE,
        .IsCacheable = MPU_ACCESS_NOT_CACHEABLE,
        .IsBufferable = MPU_ACCESS_NOT_BUFFERABLE,
    };
    HAL_MPU_ConfigRegion(&mpu);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
```

### 11.3 Initialization Call Order

```c
// Secure firmware main() — actual implementation
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GTZC_S_Init();             // ← GTZC: ETH/I2C3 NS, SRAM3 non-privileged
    MX_GPIO_Init();               // ← GPIO security attributes
    MX_ICACHE_Init();
    MX_RTC_Init();
    MPU_Config_EDATA();           // ← Before HAL_FLASH_Unlock() and EE_Init()
    HAL_FLASH_Unlock();           // ← Required: EEPROM library assumes flash pre-unlocked
    EE_Init(EE_FORCED_ERASE);     // ← EDATA1 initialized via 0x0D000000
    SysTick->CTRL = 0;            // ← Disable Secure SysTick
    NVIC_NS->ICPR[0..4] = 0xFFFFFFFF; // ← Clear stale NS pending interrupts
    NonSecure_Init();             // ← Jump to NSBOOTADD = 0x0801_2000
}

// NonSecure firmware main()
int main(void)
{
    HAL_Init();
    SystemClock_Config_NS();
    fota_startup_check();         // ← Reads FOTA flag via SECURE_CAL_Read() → 0x0D000000
    // ... rest of application init
}
```

---

## 12. RM0481 Cross-Reference Verification

The following table maps every key architectural claim in this plan to its source in RM0481 Rev 4 (Chapter 7). All line numbers refer to the extracted text in [`refs/FLASH_MEMORY.pdf`](../refs/FLASH_MEMORY.pdf).

| Claim | RM0481 Section | Exact Quote / Reference |
|---|---|---|
| Bank 1: `0x0800_0000`–`0x080F_FFFF`, 128 sectors × 8 KB | §7.3.3, Table 48 | Table 48 (lines 1191–1194): `0x0800_0000`–`0x080F_FFFF`, Sectors 0–127, 8 K each |
| Bank 2: `0x0810_0000`–`0x081F_FFFF`, 128 sectors × 8 KB | §7.3.3, Table 48 | Table 48 (lines 1197–1200): `0x0810_0000`–`0x081F_FFFF`, Sectors 0–127, 8 K each |
| EDATA total = 96 KB (`FLASH_EDATA_SIZE = 0x18000`) | `stm32h563xx.h` line 2118 | `#define FLASH_EDATA_SIZE (0x18000U) /*!< 96 KB of Flash high-cycle data */` |
| EDATA NS base = `0x0900_0000` | `stm32h563xx.h` line 2116 | `#define FLASH_EDATA_BASE_NS (0x09000000UL)` |
| EDATA S base = `0x0D000000` | `stm32h563xx.h` line 2117 | `#define FLASH_EDATA_BASE_S (0x0D000000UL)` |
| EDATA NS alias swaps with banks | §7.3.10 (lines 918–920) | *"When SWAP_BANK feature is enabled, the banks are swapped: the flash high-cycle data in Bank 2 are accessible from 0x0900_000 to 0x0900_BFFF, and the data in Bank 1 are accessible from 0x0900_C000 to 0x0901_7FFF."* |
| EDATA S alias (`0x0D000000`) is NOT in the swapped address range | §7.3.3, Table 48 (lines 1179–1212) | Table 48 lists only `0x0800_0000`–`0x081F_FFFF` as the swappable user main memory range. The Secure alias range `0x0C00_0000`–`0x0D01_7FFF` is not listed as swappable. The Secure alias is a separate AHB path to the same physical Bank 1. |
| EDATA follows physical bank (not logical address) | §7.6.6 (lines 1063–1064) | *"Bank specific settings for data area and security attributes follow the original bank and its contents."* |
| OTP: `0x08FF_F000`–`0x08FF_F7FF`, 2 KB | §7.3.9 (lines 809, 815) | *"The OTP is accessible at addresses 0x08FF_F000 to 0x08FF_F7FF"* |
| Read-Only: `0x08FF_F800`–`0x08FF_FFFF`, 2 KB | §7.3.9 (lines 809–810) | *"the read-only section is accessible from 0x08FF_F800 to 0x08FF_FFFF"* |
| System Memory: `0x0BF8_0000`–`0x0BF9_FFFF`, 128 KB | §7.3.3, Table 48 (lines 1204–1212) | Table 48: System 1 Sector 0–7 at `0x0BF8_0000`–`0x0BF8_FFFF`; System 2 at `0x0BF9_0000`–`0x0BF9_FFFF` |
| Write granularity: 128-bit (16 bytes) for user flash | §7.3.5 (lines 485–487) | *"the embedded flash memory must always perform write operations to nonvolatile memory with a 128-bit word granularity. Once the write buffer is full (128 bits), the Busy flag is set"* |
| Write granularity: 16-bit or 32-bit for EDATA | §7.3.5 (lines 499–501) | *"6-bits ECC code is associated to each 16-bit data flash word. The embedded flash memory supports 16- or 32-bit write operations (8-bit write operations are not supported)"* |
| Erase granularity: 8 KB sector | §7.2 (line 21) | *"8-Kbyte sector erase, bank erase and dual-bank mass erase"* |
| ECC: 9-bit SECDED per 128-bit word (user flash) | §7.3.8 (lines 793–794) | *"This mechanism uses nine ECC bits per 128-bit flash word, and applies to user and system memory"* |
| ECC: 6-bit per 16-bit word (EDATA/OTP) | §7.3.8 (lines 795–796) | *"For read-only, OTP, flash high-cycle data, a stronger six ECC bits per 16-bit word is used"* |
| Endurance: 100K cycles for EDATA | §7.2 (line 49) | *"Up to 48 Kbytes per bank supporting high-cycling capability (100 kcycles), to be used for data (EEPROM emulation)"* |
| RWW supported across banks only | §7.3.7 (lines 780–783) | *"supports a read in one bank while a write (RWW. read while write) or an erase is executed in the other bank. It does not support write-while-write, nor read-while-read"* |
| SWAP_BANK effective only after system reset | §7.3.11 (lines 1253–1255) | *"Force a system reset or a POR. When the reset rises up, the bank swapping is effective (SWAP_BANK value updated in FLASH_OPTCR) and the new firmware shall be executed."* |
| Default `SECBOOTADD = 0x0C00_0000` | §7.4.6 (lines 1747–1749) | *"Addresses are SECBOOTADD = 0x0C00 0000, NSBOOTADD = 0x0800 0000"* |
| `SECBOOTADD` immune to `SWAP_BANK` (inferred) | §7.3.3, Table 48 (lines 1179–1212) | Table 48 shows only `0x0800_0000`–`0x081F_FFFF` (user main memory) as swappable. The Secure alias `0x0C00_0000` is a separate AHB path not listed in the swap table. **Note:** RM0481 does not explicitly state this in a single sentence — it is inferred from the address map structure. |
| `NSBOOTADD = 0x0800_8000` maps to Bank 2 sector 4 after SWAP_BANK=1 | §7.3.11, Table 48 (lines 1195–1200) | Table 48: after SWAP_BANK=1, `0x0800_0000`–`0x080F_FFFF` maps to Bank 2 physical. Therefore `0x0800_8000` = Bank 2 physical + 0x8000 offset = Bank 2 sector 4. |
| `BKSEL` always refers to physical bank | §7.6.6 (lines 2547–2549) | *"BKSEL bit in FLASH_NSCR and FLASH_SECCR always refers to Bank1 (respectively Bank2) when it is low (respectively high), regardless of the SWAP_BANK value."* |
| EDATA sectors 119–126 for 8-sector (48 KB) configuration | §7.3.10 (lines 940–942) | *"Erasing the data area sector is possible by normal erase request for the corresponding user flash sector (120-127 for the STM32H562/563/573xx devices)"* — Note: RM0481 uses sectors 120–127; plan uses 119–126. **Correction needed: use sectors 120–127.** |
| EDATA accessible at `0x0900_0000`–`0x0900_BFFF` for Bank 1 (48 KB) | §7.3.10 (lines 913–915) | *"if 48 Kbytes of data are needed in Bank 1, set EDATA1_EN to 1, and EDATA1_STRT to 7. In this case the data are accessible from address 0x0900_0000 to 0x0900_BFFF for Bank 1."* |
| EDATA write uses `FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS` | `flash_interface.c` line 78 | `HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD_EDATA_NS, Address, ...)` — NS variant required because EDATA sectors 120-127 are outside SECWM1_END=8 (Non-Secure sectors). Using Secure variant triggers WRPERR. See `docs/debug_reports/EE_INIT_WRPERR_FIX.md`. |

### 12.1 Correction: EDATA Sector Numbers

The plan uses sectors 119–126 for EDATA. The RM0481 §7.3.10 (lines 940–942) states:
> *"Erasing the data area sector is possible by normal erase request for the corresponding user flash sector (120-127 for the STM32H562/563/573xx devices)"*

**Correct sector range: 120–127** (not 119–126). The plan's layout diagram should be updated accordingly:
- Sectors 0–2 (24 KB): Secure firmware code
- Sector 3 (8 KB): NSC veneer
- Sectors 4–119 (928 KB): NS firmware (active)
- Sectors 120–127 (48 KB → 8 × 6 KB = 48 KB): EDATA1

This also means NS firmware size = sectors 4–119 = 116 sectors × 8 KB = **928 KB** (not 920 KB).

---

## 13. References

- [`refs/FLASH_MEMORY.pdf`](../refs/FLASH_MEMORY.pdf) — RM0481 Rev 4, Chapter 7: Embedded Flash Memory
- [`application/bsp/eeprom_emul_conf.h`](../application/bsp/eeprom_emul_conf.h) — EEPROM emulation configuration (to be moved to Secure build)
- [`application/bsp/stm/stm32h563/Middlewares/ST/EEPROM_Emul/Porting/STM32H5/flash_interface.c`](../application/bsp/stm/stm32h563/Middlewares/ST/EEPROM_Emul/Porting/STM32H5/flash_interface.c) — `OB_Init()`, `GetBankNumber()`, `FI_WriteDoubleWord()`
- [`application/bsp/stm/stm32h563/Middlewares/ST/EEPROM_Emul/Core/eeprom_emul.h`](../application/bsp/stm/stm32h563/Middlewares/ST/EEPROM_Emul/Core/eeprom_emul.h) — `PAGE_ADDRESS` macro, CMSE-based address selection
- [`application/bsp/stm/stm32h563/Drivers/CMSIS/Device/ST/STM32H5xx/Include/stm32h563xx.h`](../application/bsp/stm/stm32h563/Drivers/CMSIS/Device/ST/STM32H5xx/Include/stm32h563xx.h) — `FLASH_EDATA_BASE_NS = 0x09000000`, `FLASH_EDATA_BASE_S = 0x0D000000`
- [`application/src/fota_task.c`](../application/src/fota_task.c) — FOTA task (to be implemented)
- [`application/bsp/stm/stm32h563/NonSecure/STM32H563xx_FLASH_ns.ld`](../application/bsp/stm/stm32h563/NonSecure/STM32H563xx_FLASH_ns.ld) — NS linker script (update `ORIGIN=0x08008000, LENGTH=928K`)
- [`application/bsp/stm/stm32h563/Secure/STM32H563xx_FLASH_s.ld`](../application/bsp/stm/stm32h563/Secure/STM32H563xx_FLASH_s.ld) — Secure linker script (update `FLASH=24K`, `FLASH_NSC=8K`, add `EDATA_S` at `0x0D000000`)
- [`application/bsp/stm/stm32h563/Secure_nsclib/secure_nsc.h`](../application/bsp/stm/stm32h563/Secure_nsclib/secure_nsc.h) — NSC interface (add `SECURE_CAL_*`, `SECURE_FLASH_*`, `SECURE_FOTA_*`)

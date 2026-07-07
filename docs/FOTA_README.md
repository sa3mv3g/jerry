# Jerry FOTA — Firmware Over-The-Air Update

> **Platform:** STM32H563 (TrustZone, Cortex-M33)  
> **Crypto:** wolfSSL v5.7.4 — ECDSA-P256 + SHA-256 + X.509 DER  
> **Delivery:** HTTP POST on port 8080 via `curl`  
> **Allocation:** Fully static — no `malloc`/`free` anywhere in the FOTA path

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Memory Layout](#2-memory-layout)
3. [Firmware Package Format](#3-firmware-package-format)
4. [Cryptographic Design](#4-cryptographic-design)
5. [File Map](#5-file-map)
6. [Build System Integration](#6-build-system-integration)
7. [End-to-End Workflow](#7-end-to-end-workflow)
8. [HTTP Server Protocol](#8-http-server-protocol)
9. [NSC Gateway Interface](#9-nsc-gateway-interface)
10. [Boot Sequence and Rollback](#10-boot-sequence-and-rollback)
11. [wolfSSL Configuration](#11-wolfssl-configuration)
12. [Hardware Acceleration Upgrade Path](#12-hardware-acceleration-upgrade-path)
13. [Key Management](#13-key-management)
14. [Error Codes](#14-error-codes)
15. [Troubleshooting](#15-troubleshooting)

---

## 1. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│  Host (Linux/macOS/Windows)                                     │
│                                                                 │
│  tools/sign_firmware.py  →  jerry_app_signed.bin               │
│  tools/fota_upload.sh    →  curl POST http://<ip>:8080/fota    │
└──────────────────────────────────┬──────────────────────────────┘
                                   │ HTTP/1.1 POST (binary body)
                                   ▼
┌─────────────────────────────────────────────────────────────────┐
│  NonSecure Firmware (jerry_app.elf)                             │
│                                                                 │
│  fota_task.c          — FreeRTOS task, startup check           │
│  fota_http_server.c   — lwIP netconn HTTP server, port 8080    │
│  calibration_storage.c — fota_startup_check / fota_mark_valid  │
└──────────────────────────────────┬──────────────────────────────┘
                                   │ NSC calls (CMSE gateway)
                                   ▼
┌─────────────────────────────────────────────────────────────────┐
│  Secure Firmware (jerry_secure_app.elf)                         │
│                                                                 │
│  secure_nsc.c         — SECURE_FOTA_EraseTarget/WriteChunk/    │
│                          Commit/Rollback                        │
│  fota_crypto.c        — wolfSSL wrappers (SHA-256, ECDSA-P256, │
│                          X.509 DER parse + verify)              │
│  user_settings.h      — wolfSSL config (static memory only)    │
└─────────────────────────────────────────────────────────────────┘
```

**Key design decisions:**

| Decision | Rationale |
|----------|-----------|
| All flash operations in Secure world | NS code cannot directly write to flash; only the Secure firmware can call `HAL_FLASH_Program` |
| wolfSSL with `WOLFSSL_STATIC_MEMORY` | Project requirement: no `malloc`/`free` anywhere |
| X.509 DER cert with custom OID | Standard format, easy to generate with `openssl`; custom OID carries the firmware SHA-256 hash |
| SWAP_BANK for atomic update | STM32H5 hardware feature — bank swap is atomic, no partial-update risk |
| `NO_ASN_TIME` | No wall-clock time source at FOTA verify time; cert validity period is not checked |

---

## 2. Memory Layout

### STM32H563 — 2 MB Flash (2 × 1 MB banks)

```
Bank 1 (active after power-on, SWAP_BANK=0):
  0x0C000000  Secure firmware      248 KB  (sectors 0–30)
  0x0C03E000  NSC veneer           8 KB    (sector 31)
  0x08040000  NS firmware          704 KB  (sectors 32–119)

Bank 2 (inactive, FOTA target):
  0x08100000  Secure firmware      248 KB  (sectors 0–30, mirrored)
  0x0813E000  NSC veneer           8 KB    (sector 31, mirrored)
  0x08140000  NS firmware          704 KB  (sectors 32–119, FOTA writes here)

EDATA (Bank 1 high-cycle area, stable across SWAP_BANK):
  0x0D000000  Calibration EEPROM   48 KB   (sectors 120–127)
```

**Option bytes:**
- `SECWM1_END = 0x1F` — Secure watermark covers sectors 0–31 (256 KB)
- `NSBOOTADD = 0x80400` — NS boot address = 0x08040000 (sector 32)

**After SWAP_BANK toggle:**
- Bank 2 becomes Bank 1 (new firmware runs)
- EDATA address `0x0D000000` remains stable (always points to Bank 1 high-cycle area)
- Calibration data survives FOTA

---

## 3. Firmware Package Format

The signed firmware package is a single binary file with this layout:

```
Offset                  Content
──────────────────────────────────────────────────────────────────
0                       Raw NS firmware binary
                        (padded to 16-byte boundary with 0xFF)
fw_size                 X.509 certificate DER
                        (contains SHA-256 hash in custom extension)
total - 8               cert_size  (uint32_t, little-endian)
total - 4               magic      (uint32_t = 0x464F5441 = "FOTA")
```

**Custom X.509 extension:**
- OID: `1.3.6.1.4.1.99999.1` (private enterprise, Jerry FOTA)
- DER encoding: `06 0A 2B 06 01 04 01 86 8D 1F 01 01`
- Value: `OCTET STRING { OCTET STRING { SHA-256(firmware_binary) } }`

**Verification steps in `SECURE_FOTA_Commit()`:**
1. Read trailer — validate magic `"FOTA"` and `cert_size`
2. Parse X.509 cert with wolfSSL `DecodedCert`
3. Verify cert signature against embedded CA cert (`fota_ca_cert_der[]`)
4. Extract SHA-256 hash from custom OID extension
5. Compute SHA-256 of firmware bytes in flash
6. Compare computed hash vs hash in cert
7. On success: toggle `SWAP_BANK` option byte → system reset

---

## 4. Cryptographic Design

### Algorithm choices

| Component | Algorithm | Reason |
|-----------|-----------|--------|
| Signature | ECDSA-P256 | Small key size (32 bytes), fast verify, widely supported |
| Hash | SHA-256 | Standard, hardware-acceleratable on STM32H5 |
| Certificate | X.509 DER | Standard format; `openssl` tooling available |
| Key format | ECDSA-P256 PEM | Standard `openssl ecparam` output |

### Static memory pool

wolfSSL is configured with `WOLFSSL_STATIC_MEMORY` — all allocations come from a fixed 8 KB pool in `.bss`:

```c
#define FOTA_WOLFSSL_HEAP_SIZE  (8U * 1024U)
static uint8_t fota_wolfssl_heap[FOTA_WOLFSSL_HEAP_SIZE];
```

The pool is reset after each FOTA commit via `fota_crypto_reset()`.

### Certificate chain

```
fota_ca.key  (ECDSA-P256, gitignored)
    │
    └── signs ──► firmware cert (generated per-release by sign_firmware.py)
                      │
                      └── contains SHA-256(firmware) in custom OID extension

fota_ca.crt  (committed to repo, public)
    │
    └── DER bytes embedded in secure_nsc.c as fota_ca_cert_der[]
```

The CA cert is trusted by embedding its DER bytes directly in the Secure firmware. There is no certificate chain beyond one level.

---

## 5. File Map

### New files created for FOTA

| File | Purpose |
|------|---------|
| [`application/bsp/stm/stm32h563/Secure/Core/Inc/user_settings.h`](../application/bsp/stm/stm32h563/Secure/Core/Inc/user_settings.h) | wolfSSL configuration (static memory, wolfcrypt-only, ECC, SHA-256) |
| [`application/bsp/stm/stm32h563/Secure/Core/Inc/fota_crypto.h`](../application/bsp/stm/stm32h563/Secure/Core/Inc/fota_crypto.h) | FOTA crypto API header |
| [`application/bsp/stm/stm32h563/Secure/Core/Src/fota_crypto.c`](../application/bsp/stm/stm32h563/Secure/Core/Src/fota_crypto.c) | wolfSSL wrappers: SHA-256, ECDSA-P256 verify, X.509 parse + verify |
| [`application/inc/fota_http_server.h`](../application/inc/fota_http_server.h) | HTTP server header (port, chunk size) |
| [`application/src/fota_http_server.c`](../application/src/fota_http_server.c) | lwIP netconn HTTP server — POST /fota |
| [`tools/sign_firmware.py`](../tools/sign_firmware.py) | Python signing tool — produces signed firmware package |
| [`tools/fota_upload.sh`](../tools/fota_upload.sh) | Shell script — sign + upload in one command |
| [`keys/fota_ca.crt`](../keys/fota_ca.crt) | CA certificate (public, committed) |
| `keys/fota_ca.key` | CA private key (**gitignored**, keep secure) |
| `keys/fota_ca.der` | CA cert DER (**gitignored**, used to generate embedded bytes) |

### Modified files

| File | Change |
|------|--------|
| [`application/CMakeLists.txt`](../application/CMakeLists.txt) | Added wolfSSL FetchContent (v5.7.4-stable), wolfssl target configuration |
| [`application/bsp/stm/stm32h563/CMakeLists.txt`](../application/bsp/stm/stm32h563/CMakeLists.txt) | Added `fota_crypto.c` to Secure sources; linked wolfssl |
| [`application/bsp/stm/stm32h563/Secure/Core/Src/secure_nsc.c`](../application/bsp/stm/stm32h563/Secure/Core/Src/secure_nsc.c) | Added FOTA NSC functions + embedded CA cert DER |
| [`application/bsp/stm/stm32h563/Secure_nsclib/secure_nsc.h`](../application/bsp/stm/stm32h563/Secure_nsclib/secure_nsc.h) | Added `FOTA_ERR_*` codes + 4 NSC function declarations |
| [`application/bsp/stm/stm32h563/Secure/STM32H563xx_FLASH_s.ld`](../application/bsp/stm/stm32h563/Secure/STM32H563xx_FLASH_s.ld) | `FLASH=248K`, `FLASH_NSC=8K @ 0x0C03E000` |
| [`application/bsp/stm/stm32h563/NonSecure/STM32H563xx_FLASH_ns.ld`](../application/bsp/stm/stm32h563/NonSecure/STM32H563xx_FLASH_ns.ld) | `FLASH=704K @ 0x08040000` |
| [`application/bsp/stm/stm32h563/Secure/Core/Src/main.c`](../application/bsp/stm/stm32h563/Secure/Core/Src/main.c) | `VTOR_TABLE_NS_START_ADDR = 0x08040000` |
| [`application/bsp/stm/stm32h563/Secure/Core/Inc/partition_stm32h563xx.h`](../application/bsp/stm/stm32h563/Secure/Core/Inc/partition_stm32h563xx.h) | SAU regions: NSC veneer @ `0x0C03E000`, NS firmware @ `0x08040000` |
| [`tools/flash_nucleo.py`](../tools/flash_nucleo.py) | `secwm1_end=0x1F`, `nsbootadd=0x80400`, `nonsecure_app_address=0x08040000` |
| [`application/src/fota_task.c`](../application/src/fota_task.c) | Startup validation + HTTP server launch |
| [`application/src/calibration_storage.c`](../application/src/calibration_storage.c) | `fota_startup_check()`, `fota_mark_valid()` |
| [`application/inc/calibration_storage.h`](../application/inc/calibration_storage.h) | Declarations for above |
| [`.gitignore`](../.gitignore) | Added `keys/fota_ca.key`, `keys/fota_ca.der` |

---

## 6. Build System Integration

wolfSSL is fetched via CMake `FetchContent` in [`application/CMakeLists.txt`](../application/CMakeLists.txt):

```cmake
set(WOLFSSL_USER_SETTINGS ON CACHE BOOL "" FORCE)
set(WOLFSSL_SINGLE_THREADED ON CACHE BOOL "" FORCE)   # no pthreads on bare-metal
FetchContent_Declare(
    FCD_wolfssl
    GIT_REPOSITORY https://github.com/wolfSSL/wolfssl.git
    GIT_TAG        v5.7.4-stable
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(FCD_wolfssl)

# Add Secure/Core/Inc so wolfSSL can find user_settings.h
target_include_directories(wolfssl PRIVATE "${BSP_STM32H563_DIR}/Secure/Core/Inc")

# Cortex-M33 ISA flags + suppress -Wpedantic (wolfSSL has empty TUs when features disabled)
target_compile_options(wolfssl PRIVATE
    -mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16
    -Wno-pedantic
)
```

The Secure firmware links wolfSSL:
```cmake
target_link_libraries(jerry_secure_app PUBLIC wolfssl)
```

**Build output (Debug):**
```
jerry_secure_app.elf:  FLASH 130 KB / 248 KB  (51.4%)
jerry_app.elf:         FLASH 246 KB / 704 KB  (34.2%)
```

---

## 7. End-to-End Workflow

### One-time setup

```bash
# Generate CA key pair (keep fota_ca.key secure — never commit it)
openssl ecparam -name prime256v1 -genkey -noout -out keys/fota_ca.key
openssl req -new -x509 -key keys/fota_ca.key -out keys/fota_ca.crt \
    -subj "/CN=Jerry FOTA CA" -days 3650

# Export DER bytes for embedding in secure_nsc.c
openssl x509 -in keys/fota_ca.crt -outform DER -out keys/fota_ca.der
openssl x509 -in keys/fota_ca.crt -outform DER | xxd -i
# → paste the output into fota_ca_cert_der[] in secure_nsc.c
```

### Per-release update

```bash
# 1. Build firmware
uv run python tools/build.py build

# 2. Sign + upload (one command)
./tools/fota_upload.sh <device-ip>

# Or step by step:
uv run python tools/sign_firmware.py \
    --input  build/stm-Debug/application/jerry_app.bin \
    --ca-key keys/fota_ca.key \
    --ca-cert keys/fota_ca.crt \
    --output jerry_app_signed.bin

curl -X POST http://<device-ip>:8080/fota \
     -H "Content-Type: application/octet-stream" \
     --data-binary @jerry_app_signed.bin
```

### Expected output

```
=== Step 1: Signing firmware ===
  Input:  build/stm-Debug/application/jerry_app.bin (246300 bytes)
  Output: build/stm-Debug/application/jerry_app_signed.bin

=== Step 2: Uploading to http://192.168.1.100:8080/fota ===
  File size: 247200 bytes
  This will take ~61 seconds (flash erase + write)...

Response: FOTA accepted, rebooting
HTTP status: 200

✅ FOTA upload accepted. Device is rebooting into new firmware.
   Wait ~5 seconds, then verify the device is running the new version.
```

---

## 8. HTTP Server Protocol

**Endpoint:** `POST /fota` on port `8080`

**curl compatibility:**
- Handles `Expect: 100-continue` (responds `HTTP/1.1 100 Continue\r\n\r\n` before reading body)
- Body bytes arriving in the same TCP segment as headers are correctly saved and written to flash
- 30-second receive timeout prevents hanging on stalled uploads

**Request:**
```
POST /fota HTTP/1.1
Host: 192.168.1.100:8080
Content-Type: application/octet-stream
Content-Length: <size>
Expect: 100-continue

<binary body: signed firmware package>
```

**Responses:**

| Status | Meaning |
|--------|---------|
| `100 Continue` | Sent when `Expect: 100-continue` is present; body can now be sent |
| `200 OK` | Package accepted, device is rebooting (sent before `SECURE_FOTA_Commit`) |
| `400 Bad Request` | Verification failed — body contains `FOTA error: <code>` |
| `404 Not Found` | POST to wrong path |
| `405 Method Not Allowed` | Non-POST request |
| `411 Length Required` | Missing `Content-Length` header |
| `500 Internal Server Error` | Flash erase or write failed |

**Server state machine:**
```
RECV headers → validate POST /fota + Content-Length
    → send 100 Continue (if Expect header present)
    → SECURE_FOTA_EraseTarget()
    → stream body to flash (4KB chunks via SECURE_FOTA_WriteChunk)
    → flush final partial chunk (pad to 16 bytes with 0xFF)
    → send HTTP 200
    → SECURE_FOTA_Commit(total_size)  ← device resets on success
```

---

## 9. NSC Gateway Interface

Declared in [`secure_nsc.h`](../application/bsp/stm/stm32h563/Secure_nsclib/secure_nsc.h), implemented in [`secure_nsc.c`](../application/bsp/stm/stm32h563/Secure/Core/Src/secure_nsc.c).

```c
uint32_t SECURE_FOTA_EraseTarget(void);
uint32_t SECURE_FOTA_WriteChunk(uint32_t offset, const uint8_t *pData, uint32_t len);
uint32_t SECURE_FOTA_Commit(uint32_t total_size);
void     SECURE_FOTA_Rollback(void);
```

**NSC veneer addresses (verified in `libsecure_nsclib.a`):**
```
0x0C03E010  SECURE_FOTA_Rollback
0x0C03E020  SECURE_FOTA_Commit
0x0C03E038  SECURE_FOTA_WriteChunk
0x0C03E040  SECURE_FOTA_EraseTarget
```

**`SECURE_FOTA_EraseTarget()`**
- Erases sectors 32–119 of the inactive bank (88 sectors × 8 KB = 704 KB)
- Uses `HAL_FLASHEx_Erase()` with `FLASH_TYPEERASE_SECTORS`
- Returns `FOTA_OK` or `FOTA_ERR_ERASE`

**`SECURE_FOTA_WriteChunk(offset, pData, len)`**
- Validates `pData` is in NonSecure memory via `cmse_check_address_range()`
- Requires `offset` and `len` to be multiples of 16 (STM32H5 quad-word program)
- Uses `HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, ...)`
- Returns `FOTA_OK` or `FOTA_ERR_WRITE`

**`SECURE_FOTA_Commit(total_size)`**
- Reads 8-byte trailer from flash: `cert_size` (uint32 LE) + magic `"FOTA"`
- Calls `fota_crypto_init()` → `fota_x509_verify_cert()` → `fota_x509_parse()` → `fota_sha256()` → compare
- On success: modifies `FLASH_OPTSR_PRG` to toggle `SWAP_BANK` → `HAL_FLASH_OB_Launch()` → system reset
- On failure: returns `FOTA_ERR_*` code (does NOT reset)

**`SECURE_FOTA_Rollback()`**
- Toggles `SWAP_BANK` back to the previous bank
- Triggers system reset
- Does NOT return

---

## 10. Boot Sequence and Rollback

### Normal boot (first time after FOTA)

```
Power on
  → Secure firmware runs
  → Configures SAU, GTZC, boots NS firmware
  → NS firmware: fota_task starts
  → fota_startup_check():
      SECURE_CAL_Read(FOTA_VALID_FLAG_VADDR, &flag)
      if flag != 0xA5A5A5A5:
          SECURE_FOTA_Rollback()  ← resets to previous bank
  → Application self-test passes
  → fota_mark_valid():
      SECURE_CAL_Write(FOTA_VALID_FLAG_VADDR, 0xA5A5A5A5)
  → fota_http_server_run()  ← waits for next FOTA
```

### FOTA update sequence

```
1. curl POST → HTTP server receives signed package
2. SECURE_FOTA_EraseTarget()  — erase inactive bank
3. SECURE_FOTA_WriteChunk()   — stream firmware to inactive bank
4. HTTP 200 sent to curl
5. SECURE_FOTA_Commit()       — verify + SWAP_BANK + reset
6. Device boots from new bank
7. fota_startup_check() — FOTA_VALID_FLAG not set → rollback guard active
8. Self-test passes → fota_mark_valid() → flag set
9. Next FOTA: inactive bank is now the old bank
```

### Rollback trigger

If the new firmware crashes before calling `fota_mark_valid()`, the watchdog resets the device. On the next boot, `fota_startup_check()` finds the flag unset and calls `SECURE_FOTA_Rollback()` to return to the previous firmware.

**FOTA valid flag virtual address:** `FOTA_VALID_FLAG_VADDR = 0x0030` (in EEPROM emulation)

---

## 11. wolfSSL Configuration

File: [`user_settings.h`](../application/bsp/stm/stm32h563/Secure/Core/Inc/user_settings.h)

```c
/* Static memory — no malloc/free */
#define WOLFSSL_STATIC_MEMORY
#define WOLFSSL_NO_MALLOC
#define WOLFSSL_SMALL_STACK
#define WOLFSSL_SMALL_STACK_CACHE

/* Crypto library only — no TLS stack */
#define WOLFCRYPT_ONLY

/* Target platform */
#define WOLFSSL_CORTEXM
#define WOLFSSL_ARM_ARCH 8          /* ARMv8-M (Cortex-M33) */

/* Hash */
#define WOLFSSL_SHA256
#define NO_SHA
#define NO_SHA512
#define NO_MD5
#define NO_SHA224

/* ECC — ECDSA-P256 verify only */
#define HAVE_ECC
#define HAVE_ECC_VERIFY
#define ECC_TIMING_RESISTANT
#define HAVE_ECC_SECPR1
#define WOLFSSL_HAVE_SP_ECC
#define WOLFSSL_SP_SMALL
#define WOLFSSL_SP_NO_MALLOC
#define NO_ECC192
#define NO_ECC224
#define NO_ECC384
#define NO_ECC521

/* X.509 parsing */
#define WOLFSSL_CERT_GEN
#define WOLFSSL_CERT_EXT
#define WOLFSSL_X509_EXTRA
#define WOLFSSL_ASN_EXTRA

/* Disabled — not needed */
#define NO_RSA
#define NO_DH
#define NO_DSA
#define NO_AES
#define NO_DES3
#define NO_RC4
#define NO_HMAC
#define NO_PWDBASED
#define NO_FILESYSTEM
#define NO_WOLFSSL_CLIENT
#define NO_WOLFSSL_SERVER
#define NO_SESSION_CACHE
#define NO_ERROR_STRINGS

/* Bare-metal specific */
#define WC_NO_RNG               /* No RNG needed for verify-only */
#define NO_ASN_TIME             /* No wall clock — skip cert validity period check */
#define SINGLE_THREADED         /* Secure firmware is single-threaded */
```

---

## 12. Hardware Acceleration Upgrade Path

The current implementation uses **software crypto only**. The STM32H5 has dedicated HASH and PKA peripherals that wolfSSL can use automatically.

To enable hardware acceleration:

1. Add to [`user_settings.h`](../application/bsp/stm/stm32h563/Secure/Core/Inc/user_settings.h):
   ```c
   #define WOLFSSL_STM32H5
   #define WOLFSSL_STM32_HASH   /* SHA-256 via STM32H5 HASH peripheral */
   #define WOLFSSL_STM32_PKA    /* ECDSA via STM32H5 PKA peripheral */
   ```

2. Enable in [`stm32h5xx_hal_conf.h`](../application/bsp/stm/stm32h563/Secure/Core/Inc/stm32h5xx_hal_conf.h):
   ```c
   #define HAL_HASH_MODULE_ENABLED
   #define HAL_PKA_MODULE_ENABLED
   ```

3. Add HAL HASH/PKA driver sources to the Secure CMakeLists:
   ```cmake
   ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_hash.c
   ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pka.c
   ```

No changes to [`fota_crypto.c`](../application/bsp/stm/stm32h563/Secure/Core/Src/fota_crypto.c) or [`secure_nsc.c`](../application/bsp/stm/stm32h563/Secure/Core/Src/secure_nsc.c) are required — wolfSSL uses the hardware automatically.

---

## 13. Key Management

| File | Status | Notes |
|------|--------|-------|
| `keys/fota_ca.key` | **Gitignored** | ECDSA-P256 private key — keep secure, never commit |
| `keys/fota_ca.crt` | Committed | Public CA certificate — safe to commit |
| `keys/fota_ca.der` | **Gitignored** | DER form of CA cert — regenerate from `.crt` as needed |

### Regenerating the CA key pair

If `keys/fota_ca.key` is lost or compromised:

```bash
# 1. Generate new key pair
openssl ecparam -name prime256v1 -genkey -noout -out keys/fota_ca.key
openssl req -new -x509 -key keys/fota_ca.key -out keys/fota_ca.crt \
    -subj "/CN=Jerry FOTA CA" -days 3650

# 2. Export DER bytes
openssl x509 -in keys/fota_ca.crt -outform DER | xxd -i

# 3. Update fota_ca_cert_der[] in secure_nsc.c with the new bytes
# 4. Update fota_ca_cert_der_len with the new size
# 5. Rebuild and reflash the Secure firmware
```

> ⚠️ After regenerating the CA key, all previously signed firmware packages are invalid. The new Secure firmware (with the new embedded CA cert) must be flashed before any FOTA updates can be performed.

---

## 14. Error Codes

Defined in [`secure_nsc.h`](../application/bsp/stm/stm32h563/Secure_nsclib/secure_nsc.h):

| Code | Value | Meaning |
|------|-------|---------|
| `FOTA_OK` | 0 | Success |
| `FOTA_ERR_FLASH_UNLOCK` | 1 | `HAL_FLASH_Unlock()` failed |
| `FOTA_ERR_ERASE` | 2 | Sector erase failed |
| `FOTA_ERR_WRITE` | 3 | Flash program failed |
| `FOTA_ERR_BAD_POINTER` | 4 | `pData` not in NonSecure memory |
| `FOTA_ERR_ALIGNMENT` | 5 | `offset` or `len` not 16-byte aligned |
| `FOTA_ERR_BAD_SIZE` | 6 | `total_size` too small or `cert_size` invalid |
| `FOTA_ERR_BAD_MAGIC` | 7 | Trailer magic ≠ `"FOTA"` |
| `FOTA_ERR_CERT_PARSE` | 8 | wolfSSL failed to parse firmware cert |
| `FOTA_ERR_CA_PARSE` | 9 | wolfSSL failed to parse CA cert |
| `FOTA_ERR_CERT_VERIFY` | 10 | Firmware cert signature invalid |
| `FOTA_ERR_NO_HASH` | 11 | SHA-256 hash extension not found in cert |
| `FOTA_ERR_HASH_COMPUTE` | 12 | SHA-256 computation error |
| `FOTA_ERR_HASH_MISMATCH` | 13 | Computed hash ≠ hash in cert |

---

## 15. Troubleshooting

### `HTTP 400 Bad Request — FOTA error: 7` (BAD_MAGIC)
The firmware package trailer is missing or corrupt. Ensure you used `sign_firmware.py` to produce the package and that the file was not truncated during transfer.

### `HTTP 400 Bad Request — FOTA error: 10` (CERT_VERIFY)
The firmware certificate was not signed by the embedded CA. Check that `keys/fota_ca.key` matches the `fota_ca_cert_der[]` bytes in `secure_nsc.c`.

### `HTTP 400 Bad Request — FOTA error: 13` (HASH_MISMATCH)
The firmware binary was modified after signing, or the package was corrupted in transit. Re-sign and re-upload.

### `HTTP 500 Internal Server Error`
Flash erase or write failed. Check that the device is not write-protected and that the Secure firmware has flash unlock permissions.

### curl hangs for ~1 second before sending body
The device is not responding to `Expect: 100-continue`. This is normal if the device is busy erasing flash (erase takes ~2 seconds for 704 KB). curl will send the body after a 1-second timeout regardless.

### Device does not reboot after HTTP 200
`SECURE_FOTA_Commit()` returned an error code. The error is logged but the connection is already closed. Check the FOTA error code by examining the device's RTT log.

### New firmware rolls back immediately
`fota_mark_valid()` was not called before the watchdog expired. Check that the application self-test completes within the watchdog timeout and that `fota_mark_valid()` is called on the happy path.

### wolfSSL pool exhausted (8 KB)
If `fota_crypto_init()` fails silently, increase `FOTA_WOLFSSL_HEAP_SIZE` in [`fota_crypto.c`](../application/bsp/stm/stm32h563/Secure/Core/Src/fota_crypto.c). The current 8 KB is sufficient for one X.509 parse + ECDSA-P256 verify with the current wolfSSL configuration.

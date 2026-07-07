# FOTA Implementation Specification

**Target MCU:** STM32H563 (TrustZone enabled)
**Delivery mechanism:** HTTP POST via `curl`
**Firmware signing:** X.509 certificate (ECDSA-P256) + SHA-256 integrity
**Crypto library:** wolfSSL v5.7.4 with `WOLFSSL_STATIC_MEMORY` (no heap, Secure world only)
**Allocation:** Fully static — 8KB static pool in `.bss` for wolfSSL, no `malloc`/`free`
**Status:** Architecture documented — ready for implementation

---

## 1. Memory Layout (256KB Secure partition)

```
Bank 1 sectors:
  0-30  (31 × 8KB = 248KB) — Secure firmware code + mbedTLS
  31    (8KB)               — NSC veneer  (SECWM1_END = 0x1F)
  32-119 (88 × 8KB = 704KB) — NS firmware  (NSBOOTADD = 0x08040000)
  120-127 (EDATA, 48KB)     — Calibration (unchanged)

Bank 2 sectors:
  0-31  (256KB)             — Reserved (mirrors Secure offset)
  32-119 (704KB)            — FOTA target: new NS firmware
```

### Option bytes required

| Option byte    | Value      |
|----------------|------------|
| `SECWM1_END`   | `0x1F`     |
| `NSBOOTADD`    | `0x80400`  |
| `SECBOOTADD`   | `0x0C0000` |

### Linker scripts

**`Secure/STM32H563xx_FLASH_s.ld`:**
```
FLASH    (rx)  : ORIGIN = 0x0C000000, LENGTH = 248K
FLASH_NSC (rx) : ORIGIN = 0x0C03E000, LENGTH = 8K
EDATA_S   (rw) : ORIGIN = 0x0D000000, LENGTH = 48K
```

**`NonSecure/STM32H563xx_FLASH_ns.ld`:**
```
FLASH  (rx)  : ORIGIN = 0x08040000, LENGTH = 704K
```

**`Secure/Core/Src/main.c`:**
```c
#define VTOR_TABLE_NS_START_ADDR  0x08040000UL
```

**`Secure/Core/Inc/partition_stm32h563xx.h`:**
```c
#define SAU_INIT_START0     0x0C03E000   // NSC veneer sector 31
#define SAU_INIT_END0       0x0C03FFFF
#define SAU_INIT_START1     0x08040000   // NS firmware sector 32
```

**`tools/flash_nucleo.py`:**
```python
secwm1_end: int = 0x1F
nsbootadd: int = 0x80400
nonsecure_app_address: int = 0x08040000
```

---

## 2. mbedTLS Integration

### Location
mbedTLS lives in the **Secure firmware only**. Hardware accelerators (PKA, HASH) are Secure peripherals. NS world cannot access them.

### FetchContent (in `application/CMakeLists.txt`)
```cmake
set(MBEDTLS_CONFIG_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/stm/stm32h563/Secure/Core/Inc/mbedtls_config.h"
    CACHE STRING "mbedTLS configuration file" FORCE)
set(ENABLE_TESTING    OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS   OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    FCD_mbedtls
    GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
    GIT_TAG        v3.6.2
    GIT_SHALLOW    TRUE
)
# ... in the per-dependency populate block:
FetchContent_GetProperties(FCD_mbedtls)
if(NOT fcd_mbedtls_POPULATED)
    message(STATUS "  Downloading mbedTLS v3.6.2...")
    FetchContent_MakeAvailable(FCD_mbedtls)
else()
    message(STATUS "  mbedTLS: already cached")
endif()
set(MBEDTLS_INCLUDE_DIR "${fcd_mbedtls_SOURCE_DIR}/include"
    CACHE PATH "mbedTLS include directory" FORCE)
```

### BSP CMakeLists (`application/bsp/stm/stm32h563/CMakeLists.txt`)
After `add_executable(jerry_secure_app)`:
```cmake
# Add mbedTLS ARM flags (must match Secure firmware flags)
foreach(_tgt mbedcrypto mbedx509 mbedtls p256m everest)
    if(TARGET ${_tgt})
        target_compile_options(${_tgt} PRIVATE
            -mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16
        )
    endif()
endforeach()

# Add sources
target_sources(jerry_secure_app PRIVATE
    ...
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/Core/Src/secure_nsc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/Core/Src/mbedtls_platform_alt.c
)

# Add includes
target_include_directories(jerry_secure_app PUBLIC
    ...
    ${MBEDTLS_INCLUDE_DIR}
)

# Link mbedTLS
target_link_libraries(jerry_secure_app PUBLIC mbedx509 mbedcrypto)
```

### `Secure/Core/Inc/mbedtls_config.h`
```c
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_SHA256_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_MD_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_PEM_PARSE_C
// Do NOT include mbedtls/check_config.h manually
```

### `Secure/Core/Src/mbedtls_platform_alt.c`
```c
#include "main.h"
#include <string.h>
#include <stdlib.h>
#include "mbedtls/platform.h"

static void *fota_calloc(size_t n, size_t size) {
    void *p = malloc(n * size);
    if (p) memset(p, 0, n * size);
    return p;
}
static void fota_free(void *ptr) { free(ptr); }

void mbedtls_platform_init_fota(void) {
    mbedtls_platform_set_calloc_free(fota_calloc, fota_free);
}
```

---

## 3. Firmware Package Format

```
[0 .. fw_size-1]          Raw firmware binary (16-byte aligned)
[fw_size .. total-9]      X.509 certificate DER
                            Custom extension OID 1.3.6.1.4.1.99999.1
                            contains SHA-256(firmware) as OCTET STRING
[total-8 .. total-5]      cert_size (uint32_t LE)
[total-4 .. total-1]      magic = 0x464F5441 ("FOTA")
```

### Build-side signing (`tools/sign_firmware.py`)
```bash
# One-time CA setup
openssl ecparam -name prime256v1 -genkey -noout -out keys/fota_ca.key
openssl req -new -x509 -key keys/fota_ca.key -out keys/fota_ca.crt \
    -subj "/CN=Jerry FOTA CA" -days 3650

# Per-release signing
python3 tools/sign_firmware.py \
    --input build/stm-Debug/application/jerry_app.bin \
    --ca-key keys/fota_ca.key \
    --ca-cert keys/fota_ca.crt \
    --output jerry_app_signed.bin

# Flash
curl -X POST http://<device-ip>:8080/fota \
     -H "Content-Type: application/octet-stream" \
     --data-binary @jerry_app_signed.bin
```

`sign_firmware.py` uses the `cryptography` Python package to:
1. Pad firmware to 16-byte boundary
2. Compute SHA-256 of firmware
3. Build X.509 cert with SHA-256 in custom OID extension, signed by CA key
4. Append cert + 8-byte trailer

---

## 4. FOTA NSC Functions (`Secure/Core/Src/secure_nsc.c`)

### Constants
```c
#define FOTA_NS_SECTOR_OFFSET   32U          // Skip sectors 0-31 (256KB Secure)
#define FOTA_NS_BYTE_OFFSET     (32U * 8192U) // 0x40000 = 256KB
#define FOTA_TRAILER_SIZE       8U
#define FOTA_MAGIC              0x464F5441UL  // "FOTA"
```

### Target bank selection
```c
static uint32_t fota_get_target_base(void) {
    bool swapped = (READ_BIT(FLASH->OPTSR_CUR, FLASH_OPTSR_SWAP_BANK) != 0U);
    uint32_t bank_base = swapped ? 0x08000000UL : 0x08100000UL;
    return bank_base + FOTA_NS_BYTE_OFFSET;
}
static uint32_t fota_get_target_bank(void) {
    bool swapped = (READ_BIT(FLASH->OPTSR_CUR, FLASH_OPTSR_SWAP_BANK) != 0U);
    return swapped ? FLASH_BANK_1 : FLASH_BANK_2;
}
```

### `SECURE_FOTA_EraseTarget()`
- Erase inactive bank sectors 32-119 (88 sectors)
- `FLASH_TYPEERASE_SECTORS`, `Banks = fota_get_target_bank()`, `Sector = 32`, `NbSectors = 88`
- Returns `FOTA_OK` or `FOTA_ERR_ERASE`

### `SECURE_FOTA_WriteChunk(offset, pData, len)`
- Validate `pData` is in NS memory: `cmse_check_address_range(pData, len, CMSE_NONSECURE)`
- Validate 16-byte alignment
- `HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, base + offset + i, pData + i)` for each 16 bytes
- Returns `FOTA_OK` or `FOTA_ERR_WRITE / FOTA_ERR_BAD_POINTER / FOTA_ERR_ALIGNMENT`

### `SECURE_FOTA_Commit(total_size)`
1. Read trailer (last 8 bytes): validate magic, extract `cert_size`
2. Parse X.509 cert from `flash[fw_size .. total-8]` using `mbedtls_x509_crt_parse_der()`
3. Verify cert against CA public key (baked into Secure firmware as DER bytes)
4. Extract SHA-256 hash from custom OID extension (`1.3.6.1.4.1.99999.1`)
5. Compute SHA-256 over `flash[0..fw_size-1]` using `mbedtls_sha256()`
6. Compare hashes
7. Toggle `SWAP_BANK` → `HAL_FLASH_OB_Launch()` → reset (does not return)
- Returns `FOTA_ERR_*` on failure only

### `SECURE_FOTA_Rollback()`
- Toggle `SWAP_BANK` back → `HAL_FLASH_OB_Launch()` → reset (does not return)

### CA public key embedding
```c
// In secure_nsc.c — replace with actual DER bytes from keys/fota_ca.crt
static const uint8_t fota_ca_cert_der[] = {
    /* TODO: openssl x509 -in keys/fota_ca.crt -outform DER | xxd -i */
    0x00  // placeholder — mbedTLS will return parse error until replaced
};
```

---

## 5. FOTA Error Codes (`Secure_nsclib/secure_nsc.h`)

```c
#define FOTA_OK                 0U
#define FOTA_ERR_FLASH_UNLOCK   1U
#define FOTA_ERR_ERASE          2U
#define FOTA_ERR_WRITE          3U
#define FOTA_ERR_BAD_POINTER    4U
#define FOTA_ERR_ALIGNMENT      5U
#define FOTA_ERR_BAD_SIZE       6U
#define FOTA_ERR_BAD_MAGIC      7U
#define FOTA_ERR_CERT_PARSE     8U
#define FOTA_ERR_CA_PARSE       9U
#define FOTA_ERR_CERT_VERIFY    10U
#define FOTA_ERR_NO_HASH        11U
#define FOTA_ERR_HASH_COMPUTE   12U
#define FOTA_ERR_HASH_MISMATCH  13U

uint32_t SECURE_FOTA_EraseTarget(void);
uint32_t SECURE_FOTA_WriteChunk(uint32_t offset, const uint8_t *pData, uint32_t len);
uint32_t SECURE_FOTA_Commit(uint32_t total_size);
void     SECURE_FOTA_Rollback(void);
```

---

## 6. NS-side HTTP Server (`application/src/fota_http_server.c`)

### Header (`application/inc/fota_http_server.h`)
```c
#define FOTA_HTTP_PORT   8080U
#define FOTA_CHUNK_SIZE  4096U  // must be multiple of 16
void fota_http_server_run(void);
```

### Implementation
Uses lwIP `netconn` API:
1. `netconn_new(NETCONN_TCP)` → `netconn_bind(port 8080)` → `netconn_listen()`
2. Accept loop: `netconn_accept()` → `handle_fota_connection()` → `netconn_delete()`

`handle_fota_connection()`:
1. Accumulate headers until `\r\n\r\n`
2. Parse `Content-Length` header
3. Validate `POST /fota`
4. Call `SECURE_FOTA_EraseTarget()`
5. Stream body in 4KB chunks → `SECURE_FOTA_WriteChunk(offset, buf, len)`
6. Flush final partial chunk (pad to 16 bytes with 0xFF)
7. Call `SECURE_FOTA_Commit(total_size)` — device resets on success
8. On failure: respond `400 Bad Request` with error code

### Static chunk buffer
```c
static uint8_t chunk_buf[FOTA_CHUNK_SIZE] __attribute__((aligned(16)));
```

---

## 7. FOTA Task (`application/src/fota_task.c`)

```c
void vFotaTask(void *pvParameters) {
    (void)pvParameters;
    xEventGroupSync(xSyncEventGroup, APPTASK_FOTA_TASK_EVENT_MASK,
                    APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);
    fota_startup_check();   // rolls back if FOTA_VALID_FLAG not set
    fota_http_server_run(); // blocks forever
    vTaskDelete(NULL);
}
```

---

## 8. Startup Validation (`application/src/calibration_storage.c`)

```c
void fota_startup_check(void) {
    uint32_t flag = 0U;
    uint32_t ret = SECURE_CAL_Read(FOTA_VALID_FLAG_VADDR, &flag);
    if (ret != 0U || flag != 0xA5A5A5A5U) {
        SECURE_FOTA_Rollback(); // does not return
        while (1) {}
    }
}

void fota_mark_valid(void) {
    (void)SECURE_CAL_Write(FOTA_VALID_FLAG_VADDR, 0xA5A5A5A5U);
}
```

Declarations in `application/inc/calibration_storage.h`:
```c
void fota_startup_check(void);
void fota_mark_valid(void);
```

---

## 9. Files to Create/Modify

| File | Action | Key content |
|---|---|---|
| `Secure/STM32H563xx_FLASH_s.ld` | Modify | `FLASH=248K`, `FLASH_NSC @ 0x0C03E000` |
| `NonSecure/STM32H563xx_FLASH_ns.ld` | Modify | `FLASH=704K @ 0x08040000` |
| `Secure/Core/Src/main.c` | Modify | `VTOR=0x08040000` |
| `Secure/Core/Inc/partition_stm32h563xx.h` | Modify | SAU Region 0/1 addresses |
| `tools/flash_nucleo.py` | Modify | `secwm1_end=0x1F`, `nsbootadd=0x80400` |
| `application/CMakeLists.txt` | Modify | Add mbedTLS FetchContent + per-dep populate |
| `application/bsp/stm/stm32h563/CMakeLists.txt` | Modify | mbedTLS ARM flags, sources, includes, link |
| `Secure/Core/Inc/mbedtls_config.h` | Create | Minimal mbedTLS config |
| `Secure/Core/Src/mbedtls_platform_alt.c` | Create | malloc/free allocator |
| `Secure/Core/Src/secure_nsc.c` | Modify | Add FOTA NSC functions + mbedTLS includes |
| `Secure_nsclib/secure_nsc.h` | Modify | Add FOTA_ERR_* + 4 NSC declarations |
| `application/src/fota_http_server.c` | Create | HTTP/1.1 POST /fota on port 8080 |
| `application/inc/fota_http_server.h` | Create | Header |
| `application/src/fota_task.c` | Modify | Startup check + HTTP server |
| `application/src/calibration_storage.c` | Modify | `fota_startup_check()`, `fota_mark_valid()` |
| `application/inc/calibration_storage.h` | Modify | Declare above |
| `tools/sign_firmware.py` | Create | X.509 signing tool |
| `keys/fota_ca.key` | Create (gitignored) | CA private key |
| `keys/fota_ca.crt` | Create (committed) | CA certificate |

---

## 10. Implementation Order

1. Memory layout changes (linker scripts, VTOR, SAU, flash_nucleo.py)
2. mbedTLS FetchContent in `application/CMakeLists.txt`
3. `mbedtls_config.h` + `mbedtls_platform_alt.c`
4. BSP CMakeLists: mbedTLS ARM flags, sources, includes, link
5. FOTA NSC functions in `secure_nsc.c`
6. FOTA declarations in `secure_nsc.h`
7. `fota_http_server.c` + `fota_http_server.h`
8. `fota_task.c` update
9. `calibration_storage.c/.h` update
10. `tools/sign_firmware.py`
11. Generate CA key pair
12. Build and verify: Secure FLASH should show ~111KB / 248KB

---

## 11. Verification

After full implementation, the build should show:
```
FLASH:  ~111KB / 248KB  (17.91% → ~45%)  Secure + mbedTLS
FLASH:  ~244KB / 704KB  (26.80%)          NS firmware
```

And `arm-none-eabi-nm jerry_secure_app.elf | grep SECURE_FOTA` should show 4 symbols.

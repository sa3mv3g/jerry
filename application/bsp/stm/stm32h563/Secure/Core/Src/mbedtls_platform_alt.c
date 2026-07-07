/*
 * Copyright (c) 2026
 * All rights reserved.
 */

/**
 * @file    mbedtls_platform_alt.c
 * @brief   mbedTLS platform support for the Secure firmware.
 *
 * Provides:
 *   1. Memory allocator: redirects mbedtls_calloc/free to standard malloc/free
 *      (ARM toolchain heap in Secure RAM). mbedTLS needs dynamic allocation
 *      only for X.509 certificate parsing during FOTA commit.
 *
 * Hardware acceleration (future scope):
 *   SHA-256 and ECDSA-P256 currently use mbedTLS software implementation.
 *   To enable hardware alt, add HAL HASH and PKA drivers to the project
 *   (stm32h5xx_hal_hash.h/.c, stm32h5xx_hal_pka.h/.c from STM32CubeMX),
 *   enable HAL_HASH_MODULE_ENABLED and HAL_PKA_MODULE_ENABLED in
 *   stm32h5xx_hal_conf.h, then define MBEDTLS_SHA256_ALT and
 *   MBEDTLS_ECDSA_VERIFY_ALT in mbedtls_config.h.
 */

#include "main.h"
#include <string.h>
#include <stdlib.h>

/* mbedTLS headers — available after FetchContent downloads mbedTLS */
#include "mbedtls/platform.h"

/* ==========================================================================
 * Platform memory allocator
 * The Secure firmware does not run FreeRTOS — it uses the ARM toolchain's
 * standard malloc/free (backed by the linker-defined heap in Secure RAM).
 * mbedTLS needs dynamic allocation only for X.509 certificate parsing.
 * We use mbedtls_platform_set_calloc_free() at runtime to redirect.
 * ========================================================================== */

static void *fota_calloc(size_t n, size_t size)
{
    size_t total = n * size;
    void *p = malloc(total);
    if (p != NULL)
    {
        memset(p, 0, total);
    }
    return p;
}

static void fota_free(void *ptr)
{
    free(ptr);
}

/**
 * @brief  Initialize mbedTLS platform (memory allocator).
 *         Call once before any mbedTLS operation (e.g. in Secure main()).
 *         Redirects mbedtls_calloc/free to standard malloc/free.
 */
void mbedtls_platform_init_fota(void)
{
    mbedtls_platform_set_calloc_free(fota_calloc, fota_free);
}

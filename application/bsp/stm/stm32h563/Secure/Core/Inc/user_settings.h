/**
 * @file    user_settings.h
 * @brief   wolfSSL configuration for FOTA firmware verification (Secure
 * firmware only).
 *
 * wolfSSL uses this file instead of a CMake config file when
 * WOLFSSL_USER_SETTINGS is defined.
 *
 * Configuration goals:
 *   - WOLFSSL_STATIC_MEMORY + WOLFSSL_NO_MALLOC: fully static, no heap
 *   - WOLFCRYPT_ONLY: crypto library only, no TLS stack
 *   - ECC (P-256) + SHA-256: ECDSA-P256 verify + SHA-256 hash
 *   - X.509 DER parsing: verify firmware certificate
 *   - Software crypto only (hardware accelerators deferred)
 *
 * Hardware acceleration upgrade path (future):
 *   Uncomment the following when HAL HASH/PKA drivers are added to the project:
 *     #define WOLFSSL_STM32H5
 *     #define WOLFSSL_STM32_HASH   // SHA-256 via STM32H5 HASH peripheral
 *     #define WOLFSSL_STM32_PKA    // ECDSA via STM32H5 PKA peripheral
 *   Also enable HAL_HASH_MODULE_ENABLED + HAL_PKA_MODULE_ENABLED in
 * stm32h5xx_hal_conf.h.
 */

#ifndef WOLFSSL_USER_SETTINGS_H
#define WOLFSSL_USER_SETTINGS_H

/* ==========================================================================
 * Static memory — no malloc/free
 * ========================================================================== */
#define WOLFSSL_STATIC_MEMORY
#define WOLFSSL_NO_MALLOC
#define WOLFSSL_SMALL_STACK       /* Move large stack buffers to static globals */
#define WOLFSSL_SMALL_STACK_CACHE /* Cache static buffers between calls */

/* ==========================================================================
 * Crypto library only — no TLS/DTLS stack
 * ========================================================================== */
#define WOLFCRYPT_ONLY

/* ==========================================================================
 * Target platform
 * ========================================================================== */
#define WOLFSSL_CORTEXM    /* Cortex-M optimisations */
#define WOLFSSL_ARM_ARCH 8 /* ARMv8-M (Cortex-M33) */

/* ==========================================================================
 * Hash — SHA-256 (software)
 * ========================================================================== */
#define NO_SHA         /* Disable SHA-1 (not needed) */
#define NO_SHA512      /* Disable SHA-512 (not needed) */
#define NO_MD5         /* Disable MD5 */
#define WOLFSSL_SHA256 /* Enable SHA-256 */
#define NO_SHA224      /* Disable SHA-224 */

/* ==========================================================================
 * ECC — ECDSA-P256 (software)
 * ========================================================================== */
#define HAVE_ECC
#define HAVE_ECC_VERIFY      /* Only need verify, not sign */
#define ECC_TIMING_RESISTANT /* Side-channel protection */
#define HAVE_ECC_SECPR1      /* secp256r1 = P-256 */
/* ECC_SHAMIR removed: causes 'lcl_precomp undeclared' compile error with
 * WOLFSSL_SMALL_STACK on this wolfSSL version. SP ECC is used instead. */
#define WOLFSSL_HAVE_SP_ECC  /* Use SP (single precision) ECC */
#define WOLFSSL_SP_SMALL     /* Smaller SP code */
#define WOLFSSL_SP_NO_MALLOC /* SP uses no malloc */

/* Disable curves we don't need */
#define NO_ECC192
#define NO_ECC224
#define NO_ECC384
#define NO_ECC521

/* ==========================================================================
 * X.509 certificate parsing
 * ========================================================================== */
#define WOLFSSL_CERT_GEN   /* Certificate generation support */
#define WOLFSSL_CERT_EXT   /* Certificate extensions (custom OID) */
#define WOLFSSL_X509_EXTRA /* Extra X.509 fields */
#define WOLFSSL_ASN_EXTRA  /* Extra ASN.1 support */

/* ==========================================================================
 * Disabled — not needed for FOTA verification
 * ========================================================================== */
#define NO_RSA
#define NO_DH
#define NO_DSA
#define NO_AES
#define NO_DES3
#define NO_RC4
#define NO_HMAC
#define NO_PWDBASED
#define NO_FILESYSTEM
/* Note: NO_STDIO_FILESYSTEM removed — wolfSSL's asn.c uses XSNPRINTF (→
 * snprintf) which requires <stdio.h>. Keeping stdio available for snprintf
 * only. */
#define NO_WOLFSSL_CLIENT
#define NO_WOLFSSL_SERVER
#define NO_SESSION_CACHE
#define NO_ERROR_STRINGS /* Save flash — remove error strings */

/* WC_NO_RNG: disable wolfSSL's RNG typedef (#define RNG WC_RNG).
 * STM32H5 HAL already defines RNG as RNG_S (the hardware peripheral).
 * We don't need random number generation for verify-only FOTA. */
#define WC_NO_RNG

/* NO_ASN_TIME: disable certificate validity period checking
 * (NotBefore/NotAfter). The Secure firmware has no reliable wall-clock time
 * source at FOTA verify time. The CA cert is trusted by embedding its DER bytes
 * — time checks are unnecessary. This also prevents wolfSSL from calling time()
 * → _gettimeofday (not available). */
#define NO_ASN_TIME

/* ==========================================================================
 * Misc
 * ========================================================================== */
#define SINGLE_THREADED /* Secure firmware is single-threaded */
#ifndef WOLFSSL_IGNORE_FILE_WARN
#define WOLFSSL_IGNORE_FILE_WARN /* Suppress file-not-found warnings */
#endif

#endif /* WOLFSSL_USER_SETTINGS_H */

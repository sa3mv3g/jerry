/*
 * Copyright (c) 2026
 * All rights reserved.
 */

/**
 * @file    fota_crypto.h
 * @brief   FOTA cryptographic verification interface (Secure firmware only).
 *
 * Provides SHA-256 and ECDSA-P256 verification using wolfSSL with
 * WOLFSSL_STATIC_MEMORY — fully static, no heap allocation.
 *
 * The 8KB static pool lives in .bss and is reset after each FOTA commit.
 *
 * Hardware acceleration upgrade path:
 *   Add WOLFSSL_STM32H5 + WOLFSSL_STM32_HASH + WOLFSSL_STM32_PKA to
 * user_settings.h and add HAL HASH/PKA drivers to the project. wolfSSL will
 * automatically use the hardware peripherals — no changes to this API required.
 */

#ifndef FOTA_CRYPTO_H
#define FOTA_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** SHA-256 digest size in bytes */
#define FOTA_SHA256_SIZE 32U

/** ECDSA-P256 signature size in bytes (r=32 + s=32) */
#define FOTA_ECDSA_SIG_SIZE 64U

    /**
     * @brief  Initialize the wolfSSL static memory pool.
     *         Must be called once before any fota_crypto_* function.
     *         Safe to call multiple times (idempotent).
     */
    void fota_crypto_init(void);

    /**
     * @brief  Reset the wolfSSL static memory pool.
     *         Call after SECURE_FOTA_Commit() completes (success or failure)
     *         to reclaim the 8KB pool for the next FOTA operation.
     */
    void fota_crypto_reset(void);

    /**
     * @brief  Compute SHA-256 hash of a memory region.
     * @param  data    Pointer to input data
     * @param  len     Length of input data in bytes
     * @param  digest  Output: 32-byte SHA-256 digest
     * @return 0 on success, non-zero on error
     */
    int fota_sha256(const uint8_t *data, uint32_t len,
                    uint8_t digest[FOTA_SHA256_SIZE]);

    /**
     * @brief  Verify an ECDSA-P256 signature over a SHA-256 hash.
     * @param  hash         32-byte SHA-256 hash to verify
     * @param  sig_der      DER-encoded ECDSA signature (variable length,
     * typically 70-72 bytes)
     * @param  sig_der_len  Length of sig_der in bytes
     * @param  pub_key_der  DER-encoded EC public key (SubjectPublicKeyInfo
     * format)
     * @param  pub_key_len  Length of pub_key_der in bytes
     * @return 0 on success (signature valid), non-zero on failure
     */
    int fota_ecdsa_verify(const uint8_t  hash[FOTA_SHA256_SIZE],
                          const uint8_t *sig_der, uint32_t sig_der_len,
                          const uint8_t *pub_key_der, uint32_t pub_key_len);

    /**
     * @brief  Parse an X.509 DER certificate and extract the firmware SHA-256
     * hash from the custom extension OID 1.3.6.1.4.1.99999.1.
     * @param  cert_der      DER-encoded X.509 certificate
     * @param  cert_der_len  Length of cert_der in bytes
     * @param  fw_hash_out   Output: 32-byte SHA-256 hash extracted from cert
     * extension
     * @param  pub_key_out   Output buffer for DER-encoded public key (caller
     * provides)
     * @param  pub_key_buf_len  Size of pub_key_out buffer (must be >= 91 bytes
     * for P-256)
     * @param  pub_key_len_out  Output: actual length of DER public key written
     * @return 0 on success, non-zero on error
     */
    int fota_x509_parse(const uint8_t *cert_der, uint32_t cert_der_len,
                        uint8_t  fw_hash_out[FOTA_SHA256_SIZE],
                        uint8_t *pub_key_out, uint32_t pub_key_buf_len,
                        uint32_t *pub_key_len_out);

    /**
     * @brief  Verify an X.509 certificate against a trusted CA certificate.
     * @param  cert_der      DER-encoded firmware certificate to verify
     * @param  cert_der_len  Length of cert_der
     * @param  ca_der        DER-encoded CA certificate (trusted root)
     * @param  ca_der_len    Length of ca_der
     * @return 0 on success (cert is signed by CA), non-zero on failure
     */
    int fota_x509_verify_cert(const uint8_t *cert_der, uint32_t cert_der_len,
                              const uint8_t *ca_der, uint32_t ca_der_len);

#ifdef __cplusplus
}
#endif

#endif /* FOTA_CRYPTO_H */

/*
 * Copyright (c) 2026
 * All rights reserved.
 */

/**
 * @file    fota_crypto.c
 * @brief   FOTA cryptographic verification using wolfSSL (Secure firmware only).
 *
 * Uses wolfSSL with WOLFSSL_STATIC_MEMORY — fully static, no heap.
 * The 8KB static pool lives in .bss and is reset after each FOTA commit.
 *
 * Custom X.509 extension OID for firmware SHA-256 hash:
 *   OID 1.3.6.1.4.1.99999.1 (private enterprise, Jerry FOTA)
 *   DER encoding: 06 0A 2B 06 01 04 01 86 8D 1F 01 01
 */

#define WOLFSSL_USER_SETTINGS
#include "user_settings.h"

#include "fota_crypto.h"
#include "main.h"
#include <string.h>

/* wolfSSL headers */
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include <wolfssl/wolfcrypt/asn_public.h>
#include <wolfssl/wolfcrypt/asn.h>
#include <wolfssl/wolfcrypt/memory.h>
#include <wolfssl/ssl.h>

/* ==========================================================================
 * Static memory pool
 * 8KB is sufficient for one X.509 parse + ECDSA-P256 verify.
 * Pool is reset after each FOTA commit via fota_crypto_reset().
 * ========================================================================== */

#define FOTA_WOLFSSL_HEAP_SIZE  (8U * 1024U)

static uint8_t  fota_wolfssl_heap[FOTA_WOLFSSL_HEAP_SIZE];
static uint8_t  fota_crypto_initialized = 0U;

/* ==========================================================================
 * Static wolfSSL contexts (no dynamic allocation)
 * ========================================================================== */

static wc_Sha256  fota_sha256_ctx;
static ecc_key    fota_ecc_key;
static DecodedCert fota_decoded_cert;

/* ==========================================================================
 * Custom OID for firmware SHA-256 hash in X.509 cert extension
 * OID 1.3.6.1.4.1.99999.1 — DER encoded (tag 0x06 + length + value)
 * ========================================================================== */
static const uint8_t FOTA_HASH_OID_DER[] = {
    0x06, 0x0A,                                      /* OID tag + length (10 bytes) */
    0x2B, 0x06, 0x01, 0x04, 0x01, 0x86, 0x8D, 0x1F, /* 1.3.6.1.4.1.99999 */
    0x01, 0x01                                        /* .1 */
};

/* ==========================================================================
 * Public API
 * ========================================================================== */

void fota_crypto_init(void)
{
    if (fota_crypto_initialized != 0U)
    {
        return;
    }

    wolfSSL_Init();

    /* Register static memory pool with wolfSSL */
    if (wolfSSL_StaticBufferSz(fota_wolfssl_heap, sizeof(fota_wolfssl_heap),
                                WOLFMEM_GENERAL) < 0)
    {
        /* Pool too small — increase FOTA_WOLFSSL_HEAP_SIZE */
        return;
    }

    fota_crypto_initialized = 1U;
}

void fota_crypto_reset(void)
{
    /* Reset the static pool by re-registering it.
     * This reclaims all allocations made during the previous FOTA operation. */
    fota_crypto_initialized = 0U;
    memset(fota_wolfssl_heap, 0, sizeof(fota_wolfssl_heap));
    fota_crypto_init();
}

int fota_sha256(const uint8_t *data, uint32_t len, uint8_t digest[FOTA_SHA256_SIZE])
{
    if (data == NULL || digest == NULL)
    {
        return -1;
    }

    int ret = wc_InitSha256(&fota_sha256_ctx);
    if (ret != 0) return ret;

    ret = wc_Sha256Update(&fota_sha256_ctx, data, len);
    if (ret != 0) return ret;

    ret = wc_Sha256Final(&fota_sha256_ctx, digest);
    wc_Sha256Free(&fota_sha256_ctx);
    return ret;
}

int fota_ecdsa_verify(const uint8_t hash[FOTA_SHA256_SIZE],
                      const uint8_t *sig_der,     uint32_t sig_der_len,
                      const uint8_t *pub_key_der,  uint32_t pub_key_len)
{
    if (hash == NULL || sig_der == NULL || pub_key_der == NULL)
    {
        return -1;
    }

    int ret = wc_ecc_init(&fota_ecc_key);
    if (ret != 0) return ret;

    /* Import SubjectPublicKeyInfo DER public key */
    uint32_t idx = 0U;
    ret = wc_EccPublicKeyDecode(pub_key_der, &idx, &fota_ecc_key, pub_key_len);
    if (ret != 0)
    {
        wc_ecc_free(&fota_ecc_key);
        return ret;
    }

    /* Verify ECDSA signature over SHA-256 hash */
    int verified = 0;
    ret = wc_ecc_verify_hash(sig_der, sig_der_len,
                              hash, FOTA_SHA256_SIZE,
                              &verified, &fota_ecc_key);
    wc_ecc_free(&fota_ecc_key);

    if (ret != 0 || verified != 1)
    {
        return -1;
    }
    return 0;
}

int fota_x509_parse(const uint8_t *cert_der,    uint32_t cert_der_len,
                    uint8_t fw_hash_out[FOTA_SHA256_SIZE],
                    uint8_t *pub_key_out,        uint32_t pub_key_buf_len,
                    uint32_t *pub_key_len_out)
{
    if (cert_der == NULL || fw_hash_out == NULL ||
        pub_key_out == NULL || pub_key_len_out == NULL)
    {
        return -1;
    }

    /* Initialize decoded cert using static context */
    InitDecodedCert(&fota_decoded_cert, cert_der, cert_der_len, NULL);

    int ret = ParseCert(&fota_decoded_cert, CERT_TYPE, NO_VERIFY, NULL);
    if (ret != 0)
    {
        FreeDecodedCert(&fota_decoded_cert);
        return ret;
    }

    /* Extract public key (SubjectPublicKeyInfo DER) */
    if (fota_decoded_cert.pubKeySize > pub_key_buf_len)
    {
        FreeDecodedCert(&fota_decoded_cert);
        return -1;
    }
    memcpy(pub_key_out, fota_decoded_cert.publicKey, fota_decoded_cert.pubKeySize);
    *pub_key_len_out = fota_decoded_cert.pubKeySize;

    /* Walk extensions to find our custom OID containing the firmware SHA-256 hash */
    bool hash_found = false;
    const uint8_t *ext = fota_decoded_cert.extensions;
    uint32_t ext_len   = fota_decoded_cert.extensionsSz;

    if (ext != NULL && ext_len > 0U)
    {
        const uint8_t *p   = ext;
        const uint8_t *end = ext + ext_len;

        while (p < end && !hash_found)
        {
            /* Each extension is a SEQUENCE { OID, [BOOLEAN,] OCTET STRING } */
            if (*p != 0x30U) { p++; continue; }  /* SEQUENCE tag */
            p++;
            if (p >= end) break;

            /* Read sequence length (simplified: assume < 128 bytes) */
            uint32_t seq_len = (uint32_t)*p++;
            if (p + seq_len > end) break;
            const uint8_t *seq_end = p + seq_len;

            /* Check if OID matches our custom OID */
            if ((uint32_t)(seq_end - p) >= sizeof(FOTA_HASH_OID_DER) &&
                memcmp(p, FOTA_HASH_OID_DER, sizeof(FOTA_HASH_OID_DER)) == 0)
            {
                p += sizeof(FOTA_HASH_OID_DER);

                /* Skip optional BOOLEAN (critical flag) */
                if (p < seq_end && *p == 0x01U) { p += 3U; }

                /* Expect OCTET STRING wrapping another OCTET STRING */
                if (p < seq_end && *p == 0x04U)
                {
                    p++;  /* tag */
                    uint32_t outer_len = (uint32_t)*p++;
                    if (p + outer_len <= seq_end)
                    {
                        /* Inner OCTET STRING containing the 32-byte hash */
                        if (*p == 0x04U)
                        {
                            p++;
                            uint32_t inner_len = (uint32_t)*p++;
                            if (inner_len == FOTA_SHA256_SIZE && p + inner_len <= seq_end)
                            {
                                memcpy(fw_hash_out, p, FOTA_SHA256_SIZE);
                                hash_found = true;
                            }
                        }
                    }
                }
            }
            p = seq_end;
        }
    }

    FreeDecodedCert(&fota_decoded_cert);

    return hash_found ? 0 : -1;
}

int fota_x509_verify_cert(const uint8_t *cert_der, uint32_t cert_der_len,
                           const uint8_t *ca_der,   uint32_t ca_der_len)
{
    if (cert_der == NULL || ca_der == NULL)
    {
        return -1;
    }

    /* Use wolfSSL certificate manager for chain verification */
    WOLFSSL_CERT_MANAGER *cm = wolfSSL_CertManagerNew();
    if (cm == NULL)
    {
        return -1;
    }

    /* Load CA certificate as trusted root */
    int ret = wolfSSL_CertManagerLoadCABuffer(cm, ca_der, (long)ca_der_len,
                                               WOLFSSL_FILETYPE_ASN1);
    if (ret != WOLFSSL_SUCCESS)
    {
        wolfSSL_CertManagerFree(cm);
        return -1;
    }

    /* Verify firmware certificate against CA */
    ret = wolfSSL_CertManagerVerifyBuffer(cm, cert_der, (long)cert_der_len,
                                           WOLFSSL_FILETYPE_ASN1);
    wolfSSL_CertManagerFree(cm);

    return (ret == WOLFSSL_SUCCESS) ? 0 : -1;
}

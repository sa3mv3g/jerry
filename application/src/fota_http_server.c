/*
 * Copyright (c) 2026
 * All rights reserved.
 */

/**
 * @file    fota_http_server.c
 * @brief   Minimal HTTP/1.1 FOTA server — POST /fota on port 8080.
 *
 * Uses lwIP netconn API (blocking, FreeRTOS-friendly).
 * Accepts one connection at a time. Streams the request body directly
 * to the inactive flash bank via SECURE_FOTA_WriteChunk() NSC calls.
 *
 * curl compatibility:
 *   - Handles "Expect: 100-continue" (responds 100 before reading body)
 *   - Saves body bytes that arrive in the same TCP segment as the headers
 *
 * Static allocation: chunk_buf and hdr_buf are static globals (no stack/heap).
 *
 * Usage:
 *   curl -X POST http://<device-ip>:8080/fota \
 *        -H "Content-Type: application/octet-stream" \
 *        --data-binary @jerry_app_signed.bin
 */

#include "fota_http_server.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lwip/api.h"
#include "lwip/sys.h"
#include "secure_nsc.h"

/* ==========================================================================
 * Static buffers — in .bss, no stack/heap
 * ========================================================================== */

/** 4KB chunk buffer, 16-byte aligned — written to flash via WriteChunk */
static uint8_t chunk_buf[FOTA_CHUNK_SIZE] __attribute__((aligned(16)));

/** Header accumulation buffer — 512 bytes is enough for any HTTP/1.1 request */
static char hdr_buf[512];

/* ==========================================================================
 * HTTP response strings
 * ========================================================================== */

/** Sent immediately when Expect: 100-continue is present */
static const char HTTP_100[] = "HTTP/1.1 100 Continue\r\n\r\n";

/** Sent before SECURE_FOTA_Commit() — device resets on success */
static const char HTTP_200[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 22\r\n"
    "Connection: close\r\n"
    "\r\n"
    "FOTA accepted, rebooting";

static const char HTTP_404[] =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 9\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Not Found";

static const char HTTP_405[] =
    "HTTP/1.1 405 Method Not Allowed\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 18\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Method Not Allowed";

static const char HTTP_411[] =
    "HTTP/1.1 411 Length Required\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 15\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Length Required";

static const char HTTP_500[] =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 21\r\n"
    "Connection: close\r\n"
    "\r\n"
    "Flash operation failed";

/* ==========================================================================
 * Private helpers
 * ========================================================================== */

static void send_response(struct netconn *conn, const char *resp, size_t len)
{
    netconn_write(conn, resp, len, NETCONN_COPY);
}

static void send_close(struct netconn *conn, const char *resp, size_t len)
{
    netconn_write(conn, resp, len, NETCONN_COPY);
    netconn_close(conn);
}

/**
 * @brief  Parse Content-Length from a null-terminated header buffer.
 * @return Content-Length value, or -1 if not found.
 */
static int32_t parse_content_length(const char *hdr)
{
    const char *p = hdr;
    while (*p != '\0')
    {
        if (strncasecmp(p, "Content-Length:", 15) == 0)
        {
            p += 15;
            while (*p == ' ')
            {
                p++;
            }
            return (int32_t)strtol(p, NULL, 10);
        }
        p = strchr(p, '\n');
        if (p == NULL)
        {
            break;
        }
        p++;
    }
    return -1;
}

/**
 * @brief  Check if "Expect: 100-continue" is present in headers.
 */
static bool has_expect_continue(const char *hdr)
{
    const char *p = hdr;
    while (*p != '\0')
    {
        if (strncasecmp(p, "Expect:", 7) == 0)
        {
            p += 7;
            while (*p == ' ')
            {
                p++;
            }
            return (strncasecmp(p, "100-continue", 12) == 0);
        }
        p = strchr(p, '\n');
        if (p == NULL)
        {
            break;
        }
        p++;
    }
    return false;
}

/**
 * @brief  Write a body fragment to the flash chunk buffer, flushing full
 * chunks.
 * @param  data        Pointer to body bytes
 * @param  data_len    Number of bytes
 * @param  offset      Current flash write offset (updated on flush)
 * @param  chunk_fill  Current fill level of chunk_buf (updated)
 * @return true on success, false on flash write error
 */
static bool write_body_fragment(const uint8_t *data, uint32_t data_len,
                                uint32_t *offset, uint32_t *chunk_fill)
{
    uint32_t src_off = 0U;

    while (src_off < data_len)
    {
        uint32_t space = FOTA_CHUNK_SIZE - *chunk_fill;
        uint32_t copy =
            (data_len - src_off < space) ? (data_len - src_off) : space;
        memcpy(chunk_buf + *chunk_fill, data + src_off, copy);
        *chunk_fill += copy;
        src_off += copy;

        if (*chunk_fill == FOTA_CHUNK_SIZE)
        {
            if (SECURE_FOTA_WriteChunk(*offset, chunk_buf, FOTA_CHUNK_SIZE) !=
                FOTA_OK)
            {
                return false;
            }
            *offset += FOTA_CHUNK_SIZE;
            *chunk_fill = 0U;
        }
    }
    return true;
}

/**
 * @brief  Handle a single FOTA POST connection.
 *
 * Protocol:
 *   1. Accumulate HTTP headers until \r\n\r\n
 *   2. Validate: must be POST /fota with Content-Length
 *   3. If Expect: 100-continue → send 100 Continue
 *   4. Erase inactive flash bank
 *   5. Stream body to flash (including any body bytes in the header netbuf)
 *   6. Flush final partial chunk (pad to 16-byte boundary)
 *   7. Send HTTP 200, then call SECURE_FOTA_Commit (resets on success)
 */
static void handle_fota_connection(struct netconn *conn)
{
    /* -----------------------------------------------------------------------
     * Phase 1: Accumulate headers until \r\n\r\n
     * Body bytes that arrive in the same TCP segment are saved in body_start.
     * -----------------------------------------------------------------------
     */
    uint32_t hdr_len        = 0U;
    bool     headers_done   = false;
    int32_t  content_length = -1;

    /* Pointer into hdr_buf where body bytes start (after \r\n\r\n) */
    const char *body_start     = NULL;
    uint32_t    body_start_len = 0U;

    memset(hdr_buf, 0, sizeof(hdr_buf));

    while (!headers_done)
    {
        struct netbuf *inbuf = NULL;
        if (netconn_recv(conn, &inbuf) != ERR_OK)
        {
            send_close(conn, HTTP_500, sizeof(HTTP_500) - 1U);
            return;
        }

        void *data     = NULL;
        u16_t data_len = 0U;
        netbuf_data(inbuf, &data, &data_len);

        for (u16_t i = 0U; i < data_len; i++)
        {
            if (hdr_len < (sizeof(hdr_buf) - 1U))
            {
                hdr_buf[hdr_len++] = ((char *)data)[i];
            }

            /* Detect end of headers: \r\n\r\n */
            if (hdr_len >= 4U && hdr_buf[hdr_len - 4] == '\r' &&
                hdr_buf[hdr_len - 3] == '\n' && hdr_buf[hdr_len - 2] == '\r' &&
                hdr_buf[hdr_len - 1] == '\n')
            {
                headers_done = true;

                /* Save any body bytes that follow in this same netbuf */
                uint32_t consumed = (uint32_t)(i + 1U);
                if (consumed < (uint32_t)data_len)
                {
                    body_start     = (const char *)data + consumed;
                    body_start_len = (uint32_t)data_len - consumed;
                }
                break;
            }
        }

        if (!headers_done)
        {
            netbuf_delete(inbuf);
            continue;
        }

        /* -----------------------------------------------------------------------
         * Validate request
         * -----------------------------------------------------------------------
         */
        bool is_post_fota = (strncmp(hdr_buf, "POST /fota", 10) == 0);
        content_length    = parse_content_length(hdr_buf);

        if (!is_post_fota)
        {
            netbuf_delete(inbuf);
            if (strncmp(hdr_buf, "POST", 4) == 0)
                send_close(conn, HTTP_404, sizeof(HTTP_404) - 1U);
            else
                send_close(conn, HTTP_405, sizeof(HTTP_405) - 1U);
            return;
        }
        if (content_length <= 0)
        {
            netbuf_delete(inbuf);
            send_close(conn, HTTP_411, sizeof(HTTP_411) - 1U);
            return;
        }

        /* -----------------------------------------------------------------------
         * Respond to Expect: 100-continue before reading body
         * curl sends this by default for large POSTs; without the 100 response
         * curl waits ~1 second before sending the body anyway.
         * -----------------------------------------------------------------------
         */
        if (has_expect_continue(hdr_buf))
        {
            send_response(conn, HTTP_100, sizeof(HTTP_100) - 1U);
        }

        /* -----------------------------------------------------------------------
         * Erase inactive flash bank
         * -----------------------------------------------------------------------
         */
        if (SECURE_FOTA_EraseTarget() != FOTA_OK)
        {
            netbuf_delete(inbuf);
            send_close(conn, HTTP_500, sizeof(HTTP_500) - 1U);
            return;
        }

        /* -----------------------------------------------------------------------
         * Write body bytes that arrived in the same netbuf as the headers
         * -----------------------------------------------------------------------
         */
        uint32_t offset     = 0U;
        uint32_t chunk_fill = 0U;
        uint32_t remaining  = (uint32_t)content_length;

        if (body_start != NULL && body_start_len > 0U)
        {
            uint32_t to_write =
                (body_start_len < remaining) ? body_start_len : remaining;
            if (!write_body_fragment((const uint8_t *)body_start, to_write,
                                     &offset, &chunk_fill))
            {
                netbuf_delete(inbuf);
                send_close(conn, HTTP_500, sizeof(HTTP_500) - 1U);
                return;
            }
            remaining -= to_write;
        }

        netbuf_delete(inbuf);

        /* -----------------------------------------------------------------------
         * Phase 2: Stream remaining body to flash
         * -----------------------------------------------------------------------
         */
        while (remaining > 0U)
        {
            struct netbuf *body_buf = NULL;
            if (netconn_recv(conn, &body_buf) != ERR_OK)
            {
                send_close(conn, HTTP_500, sizeof(HTTP_500) - 1U);
                return;
            }

            void *bdata     = NULL;
            u16_t bdata_len = 0U;
            netbuf_data(body_buf, &bdata, &bdata_len);

            uint32_t to_write = ((uint32_t)bdata_len < remaining)
                                    ? (uint32_t)bdata_len
                                    : remaining;

            if (!write_body_fragment((const uint8_t *)bdata, to_write, &offset,
                                     &chunk_fill))
            {
                netbuf_delete(body_buf);
                send_close(conn, HTTP_500, sizeof(HTTP_500) - 1U);
                return;
            }

            remaining -= to_write;
            netbuf_delete(body_buf);
        }

        /* -----------------------------------------------------------------------
         * Flush final partial chunk (pad to 16-byte boundary with 0xFF)
         * -----------------------------------------------------------------------
         */
        if (chunk_fill > 0U)
        {
            uint32_t padded = (chunk_fill + 15U) & ~15U;
            memset(chunk_buf + chunk_fill, 0xFFU, padded - chunk_fill);
            if (SECURE_FOTA_WriteChunk(offset, chunk_buf, padded) != FOTA_OK)
            {
                send_close(conn, HTTP_500, sizeof(HTTP_500) - 1U);
                return;
            }
            offset += padded;
        }

        /* -----------------------------------------------------------------------
         * Phase 3: Verify and commit
         * Send HTTP 200 BEFORE calling Commit — the device resets on success
         * so the response must be flushed to the client first.
         * SECURE_FOTA_Commit does NOT return on success.
         * On failure it returns a FOTA_ERR_* code.
         * -----------------------------------------------------------------------
         */
        send_close(conn, HTTP_200, sizeof(HTTP_200) - 1U);

        uint32_t commit_ret = SECURE_FOTA_Commit((uint32_t)content_length);

        /* Only reached on verification failure — connection already closed
         * above, but log the error code for debugging via a new connection
         * attempt. */
        (void)commit_ret;
        return;
    }
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

void fota_http_server_run(void)
{
    struct netconn *listen_conn = netconn_new(NETCONN_TCP);
    if (listen_conn == NULL)
    {
        return;
    }

    netconn_bind(listen_conn, IP_ADDR_ANY, FOTA_HTTP_PORT);
    netconn_listen(listen_conn);

    for (;;)
    {
        struct netconn *client_conn = NULL;
        if (netconn_accept(listen_conn, &client_conn) == ERR_OK &&
            client_conn != NULL)
        {
            /* 30-second receive timeout — prevents hanging on slow/stalled
             * uploads */
            netconn_set_recvtimeout(client_conn, 30000U);
            handle_fota_connection(client_conn);
            netconn_close(client_conn);
            netconn_delete(client_conn);
        }
    }
}

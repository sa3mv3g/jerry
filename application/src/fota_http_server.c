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
 * Static allocation: chunk_buf is a static global (no stack/heap).
 */

#include "fota_http_server.h"
#include "secure_nsc.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/* ==========================================================================
 * Static chunk buffer — 4KB, 16-byte aligned, in .bss
 * ========================================================================== */
static uint8_t chunk_buf[FOTA_CHUNK_SIZE] __attribute__((aligned(16)));

/* ==========================================================================
 * HTTP response strings
 * ========================================================================== */
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
            while (*p == ' ') { p++; }
            return (int32_t)strtol(p, NULL, 10);
        }
        p = strchr(p, '\n');
        if (p == NULL) break;
        p++;
    }
    return -1;
}

/**
 * @brief  Handle a single FOTA POST connection.
 */
static void handle_fota_connection(struct netconn *conn)
{
    /* -----------------------------------------------------------------------
     * Phase 1: Accumulate headers until \r\n\r\n
     * ----------------------------------------------------------------------- */
    static char hdr_buf[512];
    uint32_t hdr_len = 0U;
    bool headers_done = false;
    bool is_post_fota = false;
    int32_t content_length = -1;

    memset(hdr_buf, 0, sizeof(hdr_buf));

    while (!headers_done)
    {
        struct netbuf *inbuf = NULL;
        if (netconn_recv(conn, &inbuf) != ERR_OK)
        {
            send_close(conn, HTTP_500, sizeof(HTTP_500) - 1U);
            return;
        }

        void *data = NULL;
        u16_t data_len = 0U;
        netbuf_data(inbuf, &data, &data_len);

        for (u16_t i = 0U; i < data_len && hdr_len < (sizeof(hdr_buf) - 1U); i++)
        {
            hdr_buf[hdr_len++] = ((char *)data)[i];
            if (hdr_len >= 4U &&
                hdr_buf[hdr_len - 4] == '\r' && hdr_buf[hdr_len - 3] == '\n' &&
                hdr_buf[hdr_len - 2] == '\r' && hdr_buf[hdr_len - 1] == '\n')
            {
                headers_done = true;
                break;
            }
        }

        netbuf_delete(inbuf);

        if (headers_done)
        {
            is_post_fota   = (strncmp(hdr_buf, "POST /fota", 10) == 0);
            content_length = parse_content_length(hdr_buf);

            if (!is_post_fota)
            {
                if (strncmp(hdr_buf, "POST", 4) == 0)
                    send_close(conn, HTTP_404, sizeof(HTTP_404) - 1U);
                else
                    send_close(conn, HTTP_405, sizeof(HTTP_405) - 1U);
                return;
            }
            if (content_length <= 0)
            {
                send_close(conn, HTTP_411, sizeof(HTTP_411) - 1U);
                return;
            }

            /* Erase inactive bank before writing */
            if (SECURE_FOTA_EraseTarget() != FOTA_OK)
            {
                send_close(conn, HTTP_500, sizeof(HTTP_500) - 1U);
                return;
            }
        }
    }

    /* -----------------------------------------------------------------------
     * Phase 2: Stream body to flash in FOTA_CHUNK_SIZE chunks
     * ----------------------------------------------------------------------- */
    uint32_t offset    = 0U;
    uint32_t remaining = (uint32_t)content_length;
    uint32_t chunk_fill = 0U;

    while (remaining > 0U)
    {
        struct netbuf *inbuf = NULL;
        if (netconn_recv(conn, &inbuf) != ERR_OK)
        {
            send_close(conn, HTTP_500, sizeof(HTTP_500) - 1U);
            return;
        }

        void *data = NULL;
        u16_t data_len = 0U;
        netbuf_data(inbuf, &data, &data_len);

        uint32_t to_copy = (data_len < remaining) ? data_len : remaining;
        uint32_t src_off = 0U;

        while (src_off < to_copy)
        {
            uint32_t space = FOTA_CHUNK_SIZE - chunk_fill;
            uint32_t copy  = (to_copy - src_off < space) ? (to_copy - src_off) : space;
            memcpy(chunk_buf + chunk_fill, (uint8_t *)data + src_off, copy);
            chunk_fill += copy;
            src_off    += copy;

            if (chunk_fill == FOTA_CHUNK_SIZE)
            {
                if (SECURE_FOTA_WriteChunk(offset, chunk_buf, FOTA_CHUNK_SIZE) != FOTA_OK)
                {
                    netbuf_delete(inbuf);
                    send_close(conn, HTTP_500, sizeof(HTTP_500) - 1U);
                    return;
                }
                offset     += FOTA_CHUNK_SIZE;
                chunk_fill  = 0U;
            }
        }

        remaining -= to_copy;
        netbuf_delete(inbuf);
    }

    /* Flush final partial chunk (pad to 16-byte boundary with 0xFF) */
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
     * SECURE_FOTA_Commit does not return on success (device resets).
     * On failure it returns a FOTA_ERR_* code.
     * ----------------------------------------------------------------------- */
    uint32_t commit_ret = SECURE_FOTA_Commit((uint32_t)content_length);

    /* Only reached on verification failure */
    static char err_body[64];
    int err_len = snprintf(err_body, sizeof(err_body),
                           "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n"
                           "Connection: close\r\n\r\nFOTA error: %lu\r\n",
                           (unsigned long)commit_ret);
    if (err_len > 0)
    {
        netconn_write(conn, err_body, (size_t)err_len, NETCONN_COPY);
    }
    netconn_close(conn);
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

void fota_http_server_run(void)
{
    struct netconn *listen_conn = netconn_new(NETCONN_TCP);
    if (listen_conn == NULL) { return; }

    netconn_bind(listen_conn, IP_ADDR_ANY, FOTA_HTTP_PORT);
    netconn_listen(listen_conn);

    for (;;)
    {
        struct netconn *client_conn = NULL;
        if (netconn_accept(listen_conn, &client_conn) == ERR_OK && client_conn != NULL)
        {
            netconn_set_recvtimeout(client_conn, 30000U);
            handle_fota_connection(client_conn);
            netconn_close(client_conn);
            netconn_delete(client_conn);
        }
    }
}

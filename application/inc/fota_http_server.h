/*
 * Copyright (c) 2026
 * All rights reserved.
 */

#ifndef FOTA_HTTP_SERVER_H
#define FOTA_HTTP_SERVER_H

/**
 * @file    fota_http_server.h
 * @brief   Minimal HTTP/1.1 FOTA server — POST /fota endpoint on port 8080.
 *
 * curl usage:
 *   python3 tools/sign_firmware.py --input jerry_app.bin --output
 * jerry_app_signed.bin
 *   curl -X POST http://<device-ip>:8080/fota \
 *        -H "Content-Type: application/octet-stream" \
 *        --data-binary @jerry_app_signed.bin
 */

/** TCP port the FOTA HTTP server listens on */
#define FOTA_HTTP_PORT 8080U

/** Chunk buffer size for streaming firmware to flash (must be multiple of 16)
 */
#define FOTA_CHUNK_SIZE 4096U

/**
 * @brief  Start the FOTA HTTP server.
 *         Blocks indefinitely, accepting one connection at a time.
 *         Call from vFotaTask after startup validation.
 */
void fota_http_server_run(void);

#endif /* FOTA_HTTP_SERVER_H */

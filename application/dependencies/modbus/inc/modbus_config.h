/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * Modbus Library Configuration
 * This file contains compile-time configuration options for the Modbus library.
 */

#ifndef MODBUS_CONFIG_H
#define MODBUS_CONFIG_H

/* ==========================================================================
 * Protocol Enable/Disable
 * These can be overridden via CMake definitions
 * ========================================================================== */
#ifndef MODBUS_ENABLE_RTU
#define MODBUS_ENABLE_RTU 1
#endif

#ifndef MODBUS_ENABLE_ASCII
#define MODBUS_ENABLE_ASCII 1
#endif

#ifndef MODBUS_ENABLE_TCP
#define MODBUS_ENABLE_TCP 1
#endif

/* ==========================================================================
 * Mode Enable/Disable
 * ========================================================================== */
#ifndef MODBUS_ENABLE_SLAVE
#define MODBUS_ENABLE_SLAVE 1
#endif

#ifndef MODBUS_ENABLE_MASTER
#define MODBUS_ENABLE_MASTER 0
#endif

/* ==========================================================================
 * Buffer Sizes
 * ========================================================================== */
/* Maximum PDU size (without address and CRC) per Modbus specification */
#define MODBUS_MAX_PDU_SIZE 253U

/* Maximum ADU sizes for each protocol */
#define MODBUS_RTU_MAX_ADU_SIZE   256U /* 1 (addr) + 253 (PDU) + 2 (CRC) */
#define MODBUS_ASCII_MAX_ADU_SIZE 513U /* : + 2*256 + CR + LF */
#define MODBUS_TCP_MAX_ADU_SIZE   260U /* 7 (MBAP) + 253 (PDU) */

/* Maximum frame buffer size (use largest) */
#define MODBUS_MAX_FRAME_SIZE MODBUS_ASCII_MAX_ADU_SIZE

/* Receive and transmit buffer sizes */
#define MODBUS_RX_BUFFER_SIZE MODBUS_MAX_FRAME_SIZE
#define MODBUS_TX_BUFFER_SIZE MODBUS_MAX_FRAME_SIZE

/* ==========================================================================
 * Register Limits
 * ========================================================================== */
/* Maximum number of coils that can be read in one request */
#define MODBUS_MAX_READ_COILS 2000U

/* Maximum number of discrete inputs that can be read in one request */
#define MODBUS_MAX_READ_DISCRETE 2000U

/* Maximum number of registers that can be read in one request */
#define MODBUS_MAX_READ_REGISTERS 125U

/* Maximum number of coils that can be written in one request */
#define MODBUS_MAX_WRITE_COILS 1968U

/* Maximum number of registers that can be written in one request */
#define MODBUS_MAX_WRITE_REGISTERS 123U

/* ==========================================================================
 * Timing Configuration
 * ========================================================================== */
/* Default response timeout in milliseconds */
#define MODBUS_DEFAULT_RESPONSE_TIMEOUT_MS 1000U

/* RTU inter-frame delay in microseconds (3.5 character times at 19200 baud) */
#define MODBUS_RTU_INTER_FRAME_DELAY_US 1750U

/* RTU inter-character timeout in microseconds (1.5 character times) */
#define MODBUS_RTU_INTER_CHAR_TIMEOUT_US 750U

/* ==========================================================================
 * TCP Configuration
 * ========================================================================== */
/* Default Modbus TCP port */
#define MODBUS_TCP_DEFAULT_PORT 502U

/* Maximum number of simultaneous TCP connections */
#define MODBUS_TCP_MAX_CONNECTIONS 4U

/* TCP connection timeout in milliseconds */
#define MODBUS_TCP_CONNECT_TIMEOUT_MS 5000U

/* ==========================================================================
 * Serial Configuration (RTU/ASCII)
 * ========================================================================== */
/* Default baud rate */
#define MODBUS_SERIAL_DEFAULT_BAUDRATE 19200U

/* Default stop bits (1 or 2) */
#define MODBUS_SERIAL_DEFAULT_STOPBITS 2U

/* Default parity (0=none, 1=odd, 2=even) */
#define MODBUS_SERIAL_DEFAULT_PARITY 0U

/* ==========================================================================
 * Debug Configuration
 * ========================================================================== */
#ifndef MODBUS_DEBUG
#define MODBUS_DEBUG 0
#endif

#if MODBUS_DEBUG
#include <stdio.h>
#define MODBUS_LOG(fmt, ...) printf("[MODBUS] " fmt "\n", ##__VA_ARGS__)
#else
#define MODBUS_LOG(fmt, ...) ((void)0)
#endif

#endif /* MODBUS_CONFIG_H */

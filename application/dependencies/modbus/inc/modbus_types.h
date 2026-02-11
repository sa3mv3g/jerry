/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 *
 * Modbus Library Type Definitions
 * This file contains all type definitions used throughout the Modbus library.
 */

#ifndef MODBUS_TYPES_H
#define MODBUS_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "modbus_config.h"

/* ==========================================================================
 * Error Codes
 * ========================================================================== */

/**
 * @brief Modbus library error codes
 */
typedef enum
{
    MODBUS_OK = 0,                /**< Operation successful */
    MODBUS_ERROR_INVALID_PARAM,   /**< Invalid parameter */
    MODBUS_ERROR_INVALID_STATE,   /**< Invalid state for operation */
    MODBUS_ERROR_TIMEOUT,         /**< Operation timed out */
    MODBUS_ERROR_CRC,             /**< CRC/LRC check failed */
    MODBUS_ERROR_FRAME,           /**< Invalid frame format */
    MODBUS_ERROR_TRANSPORT,       /**< Transport layer error */
    MODBUS_ERROR_BUFFER_OVERFLOW, /**< Buffer overflow */
    MODBUS_ERROR_NOT_INITIALIZED, /**< Module not initialized */
    MODBUS_ERROR_BUSY,            /**< Module is busy */
    MODBUS_ERROR_NO_RESPONSE,     /**< No response received */
    MODBUS_ERROR_EXCEPTION        /**< Modbus exception received */
} modbus_error_t;

/* ==========================================================================
 * Modbus Exception Codes (per Modbus specification)
 * ========================================================================== */

/**
 * @brief Modbus exception codes as defined in the Modbus specification
 */
typedef enum
{
    MODBUS_EXCEPTION_NONE = 0x00, /**< No exception */
    MODBUS_EXCEPTION_ILLEGAL_FUNCTION =
        0x01, /**< Function code not supported */
    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS = 0x02, /**< Invalid data address */
    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE   = 0x03, /**< Invalid data value */
    MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE = 0x04, /**< Slave device failure */
    MODBUS_EXCEPTION_ACKNOWLEDGE          = 0x05, /**< Request acknowledged */
    MODBUS_EXCEPTION_SLAVE_DEVICE_BUSY    = 0x06, /**< Slave device busy */
    MODBUS_EXCEPTION_MEMORY_PARITY_ERROR  = 0x08, /**< Memory parity error */
    MODBUS_EXCEPTION_GATEWAY_PATH_UNAVAIL =
        0x0A, /**< Gateway path unavailable */
    MODBUS_EXCEPTION_GATEWAY_TARGET_FAILED = 0x0B /**< Gateway target failed */
} modbus_exception_t;

/* ==========================================================================
 * Modbus Function Codes
 * ========================================================================== */

/**
 * @brief Modbus function codes
 */
typedef enum
{
    /* Bit access - Physical Discrete Inputs */
    MODBUS_FC_READ_COILS           = 0x01, /**< Read Coils (FC01) */
    MODBUS_FC_READ_DISCRETE_INPUTS = 0x02, /**< Read Discrete Inputs (FC02) */

    /* Bit access - Internal Bits or Physical Coils */
    MODBUS_FC_WRITE_SINGLE_COIL    = 0x05, /**< Write Single Coil (FC05) */
    MODBUS_FC_WRITE_MULTIPLE_COILS = 0x0F, /**< Write Multiple Coils (FC15) */

    /* 16-bit access - Physical Input Registers */
    MODBUS_FC_READ_INPUT_REGISTERS = 0x04, /**< Read Input Registers (FC04) */

    /* 16-bit access - Internal Registers or Physical Output Registers */
    MODBUS_FC_READ_HOLDING_REGISTERS =
        0x03, /**< Read Holding Registers (FC03) */
    MODBUS_FC_WRITE_SINGLE_REGISTER = 0x06, /**< Write Single Register (FC06) */
    MODBUS_FC_WRITE_MULTIPLE_REGISTERS =
        0x10, /**< Write Multiple Registers (FC16) */

    /* Diagnostics */
    MODBUS_FC_READ_EXCEPTION_STATUS = 0x07, /**< Read Exception Status (FC07) */
    MODBUS_FC_DIAGNOSTICS           = 0x08, /**< Diagnostics (FC08) */
    MODBUS_FC_GET_COMM_EVENT_COUNTER =
        0x0B,                            /**< Get Comm Event Counter (FC11) */
    MODBUS_FC_GET_COMM_EVENT_LOG = 0x0C, /**< Get Comm Event Log (FC12) */
    MODBUS_FC_REPORT_SLAVE_ID    = 0x11, /**< Report Slave ID (FC17) */

    /* File Record Access */
    MODBUS_FC_READ_FILE_RECORD  = 0x14, /**< Read File Record (FC20) */
    MODBUS_FC_WRITE_FILE_RECORD = 0x15, /**< Write File Record (FC21) */

    /* Register Mask Write */
    MODBUS_FC_MASK_WRITE_REGISTER = 0x16, /**< Mask Write Register (FC22) */
    MODBUS_FC_READ_WRITE_MULTIPLE_REGS =
        0x17 /**< Read/Write Multiple Regs (FC23) */
} modbus_function_code_t;

/* ==========================================================================
 * Protocol and Mode Types
 * ========================================================================== */

/**
 * @brief Modbus protocol types
 */
typedef enum
{
    MODBUS_PROTOCOL_RTU = 0, /**< Modbus RTU (binary, CRC-16) */
    MODBUS_PROTOCOL_ASCII,   /**< Modbus ASCII (hex encoded, LRC) */
    MODBUS_PROTOCOL_TCP      /**< Modbus TCP/IP */
} modbus_protocol_t;

/**
 * @brief Modbus operating modes
 */
typedef enum
{
    MODBUS_MODE_SLAVE = 0, /**< Slave/Server mode */
    MODBUS_MODE_MASTER     /**< Master/Client mode */
} modbus_mode_t;

/* ==========================================================================
 * PDU and ADU Structures
 * ========================================================================== */

/**
 * @brief Modbus Protocol Data Unit (PDU)
 *
 * The PDU is protocol-independent and contains the function code and data.
 * Maximum PDU size is 253 bytes (1 byte function code + 252 bytes data).
 */
typedef struct
{
    uint8_t  function_code;                  /**< Function code */
    uint8_t  data[MODBUS_MAX_PDU_SIZE - 1U]; /**< PDU data (max 252 bytes) */
    uint16_t data_length;                    /**< Length of data in bytes */
} modbus_pdu_t;

/**
 * @brief Modbus Application Data Unit (ADU)
 *
 * The ADU includes the PDU plus protocol-specific addressing and framing.
 */
typedef struct
{
    uint8_t      unit_id;        /**< Unit identifier (slave address) */
    modbus_pdu_t pdu;            /**< Protocol Data Unit */
    uint16_t     transaction_id; /**< Transaction ID (TCP only) */
    uint16_t     protocol_id;    /**< Protocol ID (TCP only, always 0) */
} modbus_adu_t;

/* ==========================================================================
 * Request/Response Structures
 * ========================================================================== */

/**
 * @brief Read coils/discrete inputs request
 */
typedef struct
{
    uint16_t start_address; /**< Starting address */
    uint16_t quantity;      /**< Number of coils/inputs to read */
} modbus_read_bits_request_t;

/**
 * @brief Read registers request
 */
typedef struct
{
    uint16_t start_address; /**< Starting address */
    uint16_t quantity;      /**< Number of registers to read */
} modbus_read_registers_request_t;

/**
 * @brief Write single coil request
 */
typedef struct
{
    uint16_t address; /**< Coil address */
    bool     value;   /**< Coil value (true = ON, false = OFF) */
} modbus_write_single_coil_request_t;

/**
 * @brief Write single register request
 */
typedef struct
{
    uint16_t address; /**< Register address */
    uint16_t value;   /**< Register value */
} modbus_write_single_register_request_t;

/**
 * @brief Write multiple coils request
 */
typedef struct
{
    uint16_t       start_address; /**< Starting address */
    uint16_t       quantity;      /**< Number of coils to write */
    const uint8_t *values;        /**< Coil values (bit-packed) */
} modbus_write_multiple_coils_request_t;

/**
 * @brief Write multiple registers request
 */
typedef struct
{
    uint16_t        start_address; /**< Starting address */
    uint16_t        quantity;      /**< Number of registers to write */
    const uint16_t *values;        /**< Register values */
} modbus_write_multiple_registers_request_t;

/* ==========================================================================
 * State Machine Types
 * ========================================================================== */

/**
 * @brief Modbus state machine states
 */
typedef enum
{
    MODBUS_STATE_IDLE = 0,         /**< Idle, waiting for request/response */
    MODBUS_STATE_RECEIVING,        /**< Receiving frame data */
    MODBUS_STATE_PROCESSING,       /**< Processing received frame */
    MODBUS_STATE_SENDING,          /**< Sending response/request */
    MODBUS_STATE_WAITING_RESPONSE, /**< Master: waiting for slave response */
    MODBUS_STATE_ERROR             /**< Error state */
} modbus_state_t;

/* ==========================================================================
 * Serial Configuration
 * ========================================================================== */

/**
 * @brief Serial port parity options
 */
typedef enum
{
    MODBUS_PARITY_NONE = 0, /**< No parity */
    MODBUS_PARITY_ODD,      /**< Odd parity */
    MODBUS_PARITY_EVEN      /**< Even parity */
} modbus_parity_t;

/**
 * @brief Serial port configuration
 */
typedef struct
{
    uint32_t        baudrate;  /**< Baud rate (e.g., 9600, 19200, 115200) */
    uint8_t         data_bits; /**< Data bits (7 or 8) */
    uint8_t         stop_bits; /**< Stop bits (1 or 2) */
    modbus_parity_t parity;    /**< Parity setting */
} modbus_serial_config_t;

/* ==========================================================================
 * TCP Configuration
 * ========================================================================== */

/**
 * @brief TCP connection configuration
 */
typedef struct
{
    uint16_t port;       /**< TCP port number */
    uint32_t timeout_ms; /**< Connection timeout in milliseconds */
} modbus_tcp_config_t;

/* ==========================================================================
 * Context Configuration
 * ========================================================================== */

/**
 * @brief Modbus context configuration
 */
typedef struct
{
    modbus_mode_t     mode;        /**< Operating mode (slave/master) */
    modbus_protocol_t protocol;    /**< Protocol type (RTU/ASCII/TCP) */
    uint8_t           unit_id;     /**< Unit ID (slave address) */
    uint32_t response_timeout_ms;  /**< Response timeout in milliseconds */
    uint32_t inter_frame_delay_us; /**< Inter-frame delay (RTU only) */

    /* Protocol-specific configuration */
    union
    {
        modbus_serial_config_t serial; /**< Serial config (RTU/ASCII) */
        modbus_tcp_config_t    tcp;    /**< TCP config */
    } transport;
} modbus_config_t;

/* ==========================================================================
 * Forward Declarations
 * ========================================================================== */

/* Forward declare context structure (defined in modbus_core.c) */
typedef struct modbus_context modbus_context_t;

/* Forward declare protocol operations (defined in protocol headers) */
typedef struct modbus_protocol_ops modbus_protocol_ops_t;

/* Forward declare transport operations (defined in transport headers) */
typedef struct modbus_transport_ops modbus_transport_ops_t;

/* Forward declare HAL operations (defined in HAL headers) */
typedef struct modbus_hal modbus_hal_t;

#endif /* MODBUS_TYPES_H */

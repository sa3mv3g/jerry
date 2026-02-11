# Modbus Library Test Plan

## 1. Overview

This document outlines the comprehensive testing strategy for the custom Modbus library. The testing approach includes:

1. **Unit Tests** - Using Unity test framework for C code
2. **Integration Tests** - Using Python pymodbus for end-to-end communication testing
3. **Protocol Conformance Tests** - Verifying compliance with Modbus specification

## 2. Test Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Test Architecture                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    Level 1: Unit Tests (Unity)                       │    │
│  │  - CRC-16 calculation and verification                               │    │
│  │  - LRC calculation and verification                                  │    │
│  │  - PDU encoding/decoding for all function codes                      │    │
│  │  - RTU frame building and parsing                                    │    │
│  │  - ASCII frame building and parsing                                  │    │
│  │  - TCP frame building and parsing                                    │    │
│  │  - Core state machine transitions                                    │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                Level 2: Integration Tests (pymodbus)                 │    │
│  │  - TCP/IP communication with STM32 target                            │    │
│  │  - Read/Write coils (FC01, FC05, FC15)                               │    │
│  │  - Read discrete inputs (FC02)                                       │    │
│  │  - Read/Write holding registers (FC03, FC06, FC16)                   │    │
│  │  - Read input registers (FC04)                                       │    │
│  │  - Exception response handling                                       │    │
│  │  - Stress testing and performance                                    │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │              Level 3: Protocol Conformance Tests                     │    │
│  │  - Modbus specification compliance                                   │    │
│  │  - Boundary condition testing                                        │    │
│  │  - Error handling verification                                       │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 3. Unit Test Plan (Unity Framework)

### 3.1 CRC-16 Module Tests (`test_modbus_crc.c`)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| CRC_001 | test_crc16_empty_data | CRC of empty buffer | Returns 0xFFFF |
| CRC_002 | test_crc16_single_byte | CRC of single byte (0x01) | Known CRC value |
| CRC_003 | test_crc16_known_vector_1 | CRC of "123456789" | 0x4B37 |
| CRC_004 | test_crc16_modbus_example | CRC of Modbus example frame | Matches specification |
| CRC_005 | test_crc16_verify_valid | Verify valid frame with CRC | Returns true |
| CRC_006 | test_crc16_verify_invalid | Verify frame with wrong CRC | Returns false |
| CRC_007 | test_crc16_null_pointer | CRC with NULL data pointer | Returns 0xFFFF |
| CRC_008 | test_crc16_large_buffer | CRC of 256-byte buffer | Correct CRC |

### 3.2 LRC Module Tests (`test_modbus_lrc.c`)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| LRC_001 | test_lrc_empty_data | LRC of empty buffer | Returns 0x00 |
| LRC_002 | test_lrc_single_byte | LRC of single byte | Two's complement |
| LRC_003 | test_lrc_known_vector | LRC of known data | Matches expected |
| LRC_004 | test_lrc_verify_valid | Verify valid frame with LRC | Returns true |
| LRC_005 | test_lrc_verify_invalid | Verify frame with wrong LRC | Returns false |
| LRC_006 | test_byte_to_ascii | Convert 0xAB to ASCII | Returns 'A', 'B' |
| LRC_007 | test_ascii_to_byte_valid | Convert "AB" to 0xAB | Returns MODBUS_OK |
| LRC_008 | test_ascii_to_byte_invalid | Convert "GH" (invalid hex) | Returns error |
| LRC_009 | test_binary_to_ascii | Convert binary array to ASCII | Correct hex string |
| LRC_010 | test_ascii_to_binary | Convert ASCII hex to binary | Correct binary array |

### 3.3 PDU Module Tests (`test_modbus_pdu.c`)

#### 3.3.1 Read Coils (FC01)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| PDU_FC01_001 | test_pdu_read_coils_request | Encode read coils request | Correct PDU format |
| PDU_FC01_002 | test_pdu_read_coils_response | Encode read coils response | Correct PDU format |
| PDU_FC01_003 | test_pdu_read_coils_decode | Decode read coils request | Correct parameters |
| PDU_FC01_004 | test_pdu_read_coils_max_quantity | Request max 2000 coils | Success |
| PDU_FC01_005 | test_pdu_read_coils_overflow | Request > 2000 coils | Error |

#### 3.3.2 Read Discrete Inputs (FC02)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| PDU_FC02_001 | test_pdu_read_discrete_inputs_request | Encode request | Correct PDU |
| PDU_FC02_002 | test_pdu_read_discrete_inputs_response | Encode response | Correct PDU |
| PDU_FC02_003 | test_pdu_read_discrete_inputs_decode | Decode request | Correct params |

#### 3.3.3 Read Holding Registers (FC03)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| PDU_FC03_001 | test_pdu_read_holding_regs_request | Encode request | Correct PDU |
| PDU_FC03_002 | test_pdu_read_holding_regs_response | Encode response | Correct PDU |
| PDU_FC03_003 | test_pdu_read_holding_regs_decode | Decode request | Correct params |
| PDU_FC03_004 | test_pdu_read_holding_regs_max | Request max 125 regs | Success |
| PDU_FC03_005 | test_pdu_read_holding_regs_overflow | Request > 125 regs | Error |

#### 3.3.4 Read Input Registers (FC04)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| PDU_FC04_001 | test_pdu_read_input_regs_request | Encode request | Correct PDU |
| PDU_FC04_002 | test_pdu_read_input_regs_response | Encode response | Correct PDU |
| PDU_FC04_003 | test_pdu_read_input_regs_decode | Decode request | Correct params |

#### 3.3.5 Write Single Coil (FC05)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| PDU_FC05_001 | test_pdu_write_single_coil_on | Write coil ON (0xFF00) | Correct PDU |
| PDU_FC05_002 | test_pdu_write_single_coil_off | Write coil OFF (0x0000) | Correct PDU |
| PDU_FC05_003 | test_pdu_write_single_coil_invalid | Write invalid value | Error |
| PDU_FC05_004 | test_pdu_write_single_coil_decode | Decode request | Correct params |

#### 3.3.6 Write Single Register (FC06)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| PDU_FC06_001 | test_pdu_write_single_reg_request | Encode request | Correct PDU |
| PDU_FC06_002 | test_pdu_write_single_reg_response | Encode response (echo) | Matches request |
| PDU_FC06_003 | test_pdu_write_single_reg_decode | Decode request | Correct params |

#### 3.3.7 Write Multiple Coils (FC15)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| PDU_FC15_001 | test_pdu_write_multi_coils_request | Encode request | Correct PDU |
| PDU_FC15_002 | test_pdu_write_multi_coils_response | Encode response | Correct PDU |
| PDU_FC15_003 | test_pdu_write_multi_coils_decode | Decode request | Correct params |
| PDU_FC15_004 | test_pdu_write_multi_coils_max | Write max 1968 coils | Success |

#### 3.3.8 Write Multiple Registers (FC16)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| PDU_FC16_001 | test_pdu_write_multi_regs_request | Encode request | Correct PDU |
| PDU_FC16_002 | test_pdu_write_multi_regs_response | Encode response | Correct PDU |
| PDU_FC16_003 | test_pdu_write_multi_regs_decode | Decode request | Correct params |
| PDU_FC16_004 | test_pdu_write_multi_regs_max | Write max 123 regs | Success |

#### 3.3.9 Exception Responses

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| PDU_EXC_001 | test_pdu_exception_illegal_function | Encode exception 01 | Correct format |
| PDU_EXC_002 | test_pdu_exception_illegal_address | Encode exception 02 | Correct format |
| PDU_EXC_003 | test_pdu_exception_illegal_value | Encode exception 03 | Correct format |
| PDU_EXC_004 | test_pdu_exception_device_failure | Encode exception 04 | Correct format |
| PDU_EXC_005 | test_pdu_exception_decode | Decode exception response | Correct exception |

### 3.4 RTU Module Tests (`test_modbus_rtu.c`)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| RTU_001 | test_rtu_build_frame_fc03 | Build FC03 request frame | Correct ADU with CRC |
| RTU_002 | test_rtu_parse_frame_fc03 | Parse FC03 response frame | Correct ADU extraction |
| RTU_003 | test_rtu_build_frame_fc16 | Build FC16 request frame | Correct ADU with CRC |
| RTU_004 | test_rtu_parse_frame_invalid_crc | Parse frame with bad CRC | MODBUS_ERROR_CRC |
| RTU_005 | test_rtu_parse_frame_too_short | Parse frame < 4 bytes | MODBUS_ERROR_FRAME |
| RTU_006 | test_rtu_parse_frame_too_long | Parse frame > 256 bytes | MODBUS_ERROR_FRAME |
| RTU_007 | test_rtu_interframe_delay_9600 | Calculate t3.5 at 9600 baud | ~4010 µs |
| RTU_008 | test_rtu_interframe_delay_19200 | Calculate t3.5 at 19200 baud | ~2005 µs |
| RTU_009 | test_rtu_interframe_delay_38400 | Calculate t3.5 at 38400 baud | 1750 µs (fixed) |
| RTU_010 | test_rtu_interchar_timeout_9600 | Calculate t1.5 at 9600 baud | ~1719 µs |
| RTU_011 | test_rtu_address_match_direct | Direct address match | Returns true |
| RTU_012 | test_rtu_address_match_broadcast | Broadcast address (0) | Returns true |
| RTU_013 | test_rtu_address_mismatch | Address mismatch | Returns false |
| RTU_014 | test_rtu_rx_state_machine | Test receiver state machine | Correct transitions |

### 3.5 ASCII Module Tests (`test_modbus_ascii.c`)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| ASCII_001 | test_ascii_build_frame_fc03 | Build FC03 request frame | Correct format with LRC |
| ASCII_002 | test_ascii_parse_frame_fc03 | Parse FC03 response frame | Correct ADU extraction |
| ASCII_003 | test_ascii_build_frame_fc16 | Build FC16 request frame | Correct format |
| ASCII_004 | test_ascii_parse_frame_invalid_lrc | Parse frame with bad LRC | MODBUS_ERROR_CRC |
| ASCII_005 | test_ascii_parse_frame_no_start | Parse frame without ':' | MODBUS_ERROR_FRAME |
| ASCII_006 | test_ascii_parse_frame_no_crlf | Parse frame without CR LF | MODBUS_ERROR_FRAME |
| ASCII_007 | test_ascii_parse_frame_odd_length | Parse frame with odd hex chars | MODBUS_ERROR_FRAME |
| ASCII_008 | test_ascii_rx_state_machine | Test receiver state machine | Correct transitions |
| ASCII_009 | test_ascii_address_match | Address matching | Correct behavior |

### 3.6 TCP Module Tests (`test_modbus_tcp.c`)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| TCP_001 | test_tcp_build_frame_fc03 | Build FC03 request frame | Correct MBAP header |
| TCP_002 | test_tcp_parse_frame_fc03 | Parse FC03 response frame | Correct ADU extraction |
| TCP_003 | test_tcp_build_frame_fc16 | Build FC16 request frame | Correct MBAP header |
| TCP_004 | test_tcp_parse_frame_wrong_protocol | Parse frame with protocol != 0 | MODBUS_ERROR_FRAME |
| TCP_005 | test_tcp_parse_frame_length_mismatch | Parse frame with wrong length | MODBUS_ERROR_FRAME |
| TCP_006 | test_tcp_parse_frame_too_short | Parse frame < 8 bytes | MODBUS_ERROR_FRAME |
| TCP_007 | test_tcp_transaction_id | Verify transaction ID handling | Correct ID in response |
| TCP_008 | test_tcp_unit_id | Verify unit ID handling | Correct unit ID |

### 3.7 Core Module Tests (`test_modbus_core.c`)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| CORE_001 | test_core_init_slave | Initialize as slave | MODBUS_OK |
| CORE_002 | test_core_init_master | Initialize as master | MODBUS_OK |
| CORE_003 | test_core_init_null_ctx | Initialize with NULL context | MODBUS_ERROR_INVALID_PARAM |
| CORE_004 | test_core_deinit | Deinitialize context | MODBUS_OK |
| CORE_005 | test_core_slave_process_fc01 | Process FC01 request | Correct response |
| CORE_006 | test_core_slave_process_fc02 | Process FC02 request | Correct response |
| CORE_007 | test_core_slave_process_fc03 | Process FC03 request | Correct response |
| CORE_008 | test_core_slave_process_fc04 | Process FC04 request | Correct response |
| CORE_009 | test_core_slave_process_fc05 | Process FC05 request | Correct response |
| CORE_010 | test_core_slave_process_fc06 | Process FC06 request | Correct response |
| CORE_011 | test_core_slave_process_fc15 | Process FC15 request | Correct response |
| CORE_012 | test_core_slave_process_fc16 | Process FC16 request | Correct response |
| CORE_013 | test_core_slave_invalid_function | Process unsupported FC | Exception 01 |
| CORE_014 | test_core_slave_wrong_unit_id | Process wrong unit ID | No response |
| CORE_015 | test_core_slave_broadcast | Process broadcast request | No response |

## 4. Integration Test Plan (Python pymodbus)

### 4.1 Test Environment Setup

```
┌─────────────────────┐         TCP/IP          ┌─────────────────────┐
│   Test PC           │◄───────────────────────►│   STM32H563         │
│   (pymodbus)        │       Port 502          │   (Modbus Slave)    │
│                     │                         │                     │
│   - pytest          │                         │   - FreeRTOS        │
│   - pymodbus        │                         │   - lwIP            │
│   - asyncio         │                         │   - Custom Modbus   │
└─────────────────────┘                         └─────────────────────┘
```

### 4.2 Test Configuration

```python
# test_config.py
MODBUS_HOST = "192.168.1.100"  # STM32 IP address
MODBUS_PORT = 502
MODBUS_UNIT_ID = 1
TIMEOUT = 3.0  # seconds
```

### 4.3 Integration Test Cases

#### 4.3.1 Connection Tests

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| INT_CONN_001 | test_tcp_connect | Establish TCP connection | Connection successful |
| INT_CONN_002 | test_tcp_disconnect | Close TCP connection | Clean disconnect |
| INT_CONN_003 | test_tcp_reconnect | Reconnect after disconnect | Reconnection successful |
| INT_CONN_004 | test_tcp_timeout | Connection timeout handling | Timeout exception |

#### 4.3.2 Coil Tests (FC01, FC05, FC15)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| INT_COIL_001 | test_read_single_coil | Read single coil | Correct value |
| INT_COIL_002 | test_read_multiple_coils | Read 10 coils | Correct values |
| INT_COIL_003 | test_write_single_coil_on | Write coil ON | Coil set to ON |
| INT_COIL_004 | test_write_single_coil_off | Write coil OFF | Coil set to OFF |
| INT_COIL_005 | test_write_multiple_coils | Write 8 coils | All coils set |
| INT_COIL_006 | test_read_coil_invalid_addr | Read invalid address | Exception 02 |
| INT_COIL_007 | test_write_coil_invalid_addr | Write invalid address | Exception 02 |

#### 4.3.3 Discrete Input Tests (FC02)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| INT_DI_001 | test_read_single_discrete_input | Read single input | Correct value |
| INT_DI_002 | test_read_multiple_discrete_inputs | Read 10 inputs | Correct values |
| INT_DI_003 | test_read_discrete_input_invalid | Read invalid address | Exception 02 |

#### 4.3.4 Holding Register Tests (FC03, FC06, FC16)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| INT_HR_001 | test_read_single_holding_reg | Read single register | Correct value |
| INT_HR_002 | test_read_multiple_holding_regs | Read 10 registers | Correct values |
| INT_HR_003 | test_write_single_holding_reg | Write single register | Register updated |
| INT_HR_004 | test_write_multiple_holding_regs | Write 5 registers | All registers updated |
| INT_HR_005 | test_read_write_verify | Write then read back | Values match |
| INT_HR_006 | test_holding_reg_invalid_addr | Access invalid address | Exception 02 |
| INT_HR_007 | test_holding_reg_invalid_value | Write out-of-range value | Exception 03 |

#### 4.3.5 Input Register Tests (FC04)

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| INT_IR_001 | test_read_single_input_reg | Read single register | Correct value |
| INT_IR_002 | test_read_multiple_input_regs | Read 10 registers | Correct values |
| INT_IR_003 | test_input_reg_invalid_addr | Read invalid address | Exception 02 |

#### 4.3.6 Exception Handling Tests

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| INT_EXC_001 | test_illegal_function | Send unsupported FC | Exception 01 |
| INT_EXC_002 | test_illegal_data_address | Access invalid address | Exception 02 |
| INT_EXC_003 | test_illegal_data_value | Send invalid value | Exception 03 |

#### 4.3.7 Stress Tests

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| INT_STRESS_001 | test_rapid_requests | 100 requests in quick succession | All succeed |
| INT_STRESS_002 | test_large_read | Read maximum registers (125) | Success |
| INT_STRESS_003 | test_large_write | Write maximum registers (123) | Success |
| INT_STRESS_004 | test_concurrent_clients | Multiple simultaneous connections | All succeed |
| INT_STRESS_005 | test_long_duration | 1000 requests over 10 minutes | No failures |

#### 4.3.8 Boundary Tests

| Test ID | Test Name | Description | Expected Result |
|---------|-----------|-------------|-----------------|
| INT_BOUND_001 | test_address_zero | Access address 0 | Success or Exception 02 |
| INT_BOUND_002 | test_address_max | Access address 65535 | Exception 02 |
| INT_BOUND_003 | test_quantity_zero | Request 0 items | Exception 03 |
| INT_BOUND_004 | test_quantity_max_coils | Request 2000 coils | Success |
| INT_BOUND_005 | test_quantity_max_regs | Request 125 registers | Success |
| INT_BOUND_006 | test_quantity_overflow | Request > max items | Exception 03 |

## 5. Test Data

### 5.1 Known CRC-16 Test Vectors

```c
/* Test vector 1: Standard Modbus example */
uint8_t data1[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
uint16_t expected_crc1 = 0xC5CD;

/* Test vector 2: ASCII "123456789" */
uint8_t data2[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
uint16_t expected_crc2 = 0x4B37;

/* Test vector 3: Single byte */
uint8_t data3[] = {0x01};
uint16_t expected_crc3 = 0x807E;
```

### 5.2 Known LRC Test Vectors

```c
/* Test vector 1: Address + FC03 + Start + Quantity */
uint8_t data1[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
uint8_t expected_lrc1 = 0xF2;

/* Test vector 2: Single byte */
uint8_t data2[] = {0x01};
uint8_t expected_lrc2 = 0xFF;
```

### 5.3 Sample Modbus Frames

```c
/* RTU Frame: Read Holding Registers (FC03) */
/* Address=1, FC=03, Start=0, Quantity=10 */
uint8_t rtu_fc03_request[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD};

/* ASCII Frame: Read Holding Registers (FC03) */
/* :010300000000AF<CR><LF> */
char ascii_fc03_request[] = ":0103000000000AF2\r\n";

/* TCP Frame: Read Holding Registers (FC03) */
/* Transaction=1, Protocol=0, Length=6, Unit=1, FC=03, Start=0, Qty=10 */
uint8_t tcp_fc03_request[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x06,
                               0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
```

## 6. Test Execution

### 6.1 Unit Test Execution

```bash
# Build and run unit tests
cmake -S . -B build_test -DMODBUS_BUILD_TESTS=ON
cmake --build build_test --target modbus_tests
./build_test/tests/modbus_tests
```

### 6.2 Integration Test Execution

```bash
# Install Python dependencies
pip install pymodbus pytest pytest-asyncio

# Run integration tests (requires STM32 target connected)
pytest tests/integration/ -v --tb=short

# Run specific test category
pytest tests/integration/test_holding_registers.py -v

# Run stress tests
pytest tests/integration/test_stress.py -v --timeout=600
```

### 6.3 Test Reports

```bash
# Generate JUnit XML report
pytest tests/integration/ --junitxml=test_results.xml

# Generate HTML report
pytest tests/integration/ --html=test_report.html
```

## 7. Test Coverage Requirements

| Module | Minimum Coverage |
|--------|-----------------|
| modbus_crc.c | 100% |
| modbus_lrc.c | 100% |
| modbus_pdu.c | 90% |
| modbus_rtu.c | 85% |
| modbus_ascii.c | 85% |
| modbus_tcp.c | 90% |
| modbus_core.c | 85% |

## 8. Pass/Fail Criteria

### 8.1 Unit Tests
- All unit tests must pass (100%)
- No memory leaks detected
- Code coverage meets minimum requirements

### 8.2 Integration Tests
- All functional tests must pass (100%)
- Stress tests: < 0.1% failure rate
- Response time: < 100ms for single register operations

## 9. Test Schedule

| Phase | Duration | Description |
|-------|----------|-------------|
| Phase 1 | 2 days | Unit test development and execution |
| Phase 2 | 2 days | Integration test development |
| Phase 3 | 1 day | Integration test execution |
| Phase 4 | 1 day | Bug fixes and regression testing |

## 10. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Hardware unavailable | High | Use mock/simulator for initial testing |
| Network issues | Medium | Use local loopback for development |
| Timing-sensitive tests | Medium | Add appropriate delays and retries |
| Resource constraints | Low | Optimize test execution order |

## 11. Appendix

### A. Unity Test Framework Setup

Unity is a lightweight C test framework suitable for embedded systems.

Repository: https://github.com/ThrowTheSwitch/Unity

### B. pymodbus Installation

```bash
pip install pymodbus[serial,repl]
```

### C. Test File Structure

```
tests/
├── unit/
│   ├── CMakeLists.txt
│   ├── test_modbus_crc.c
│   ├── test_modbus_lrc.c
│   ├── test_modbus_pdu.c
│   ├── test_modbus_rtu.c
│   ├── test_modbus_ascii.c
│   ├── test_modbus_tcp.c
│   ├── test_modbus_core.c
│   └── test_runner.c
├── integration/
│   ├── conftest.py
│   ├── test_config.py
│   ├── test_connection.py
│   ├── test_coils.py
│   ├── test_discrete_inputs.py
│   ├── test_holding_registers.py
│   ├── test_input_registers.py
│   ├── test_exceptions.py
│   ├── test_stress.py
│   └── test_boundary.py
└── README.md
```

# Robot Framework HIL Tests

Hardware-in-the-Loop (HIL) test suite for the Jerry STM32H563ZI firmware using
**Robot Framework** as the test orchestration layer and the **Analog Discovery 3 (AD3)**
as the programmable bench instrument.

---

## Directory Structure

```
tests/robot/
├── CMakeLists.txt              # FetchContent for RF, pymodbus, dwf, pyserial
├── README.md                   # This file
├── config/
│   └── hardware_config.yaml    # Hardware connection map and test parameters
├── libraries/                  # Python keyword libraries
│   ├── __init__.py
│   ├── AD3Library.py           # Analog Discovery 3 (Wavegen, Scope, DIO, UART, I2C)
│   ├── ModbusTcpLibrary.py     # Modbus TCP client keywords
│   ├── ModbusRtuLibrary.py     # Modbus RTU client keywords (FUTURE)
│   └── FlashLibrary.py         # Firmware flash keywords
├── resources/                  # Shared Robot Framework resource files
│   ├── common.resource         # Common keywords, variables, suite setup/teardown
│   ├── ad3.resource            # AD3 hardware control keywords
│   └── modbus_tcp.resource     # Modbus TCP helper keywords
└── suites/                     # Test suites
    ├── 01_digital_io/
    │   ├── test_digital_outputs.robot       # DO0-DO15 via Modbus coils + AD3 DIO + I2C snoop
    │   ├── test_digital_inputs.robot        # DI0-DI7 via AD3 DIO stimulus + Modbus FC02
    │   └── test_digital_io_negative.robot   # Invalid addresses, read-only violations
    ├── 02_adc/
    │   ├── test_adc_accuracy.robot          # Voltage sweep accuracy (±2%)
    │   ├── test_adc_filter.robot            # 50Hz notch filter verification
    │   └── test_adc_negative.robot          # Write-to-read-only, NaN/Inf values
    ├── 03_pwm/
    │   ├── test_pwm_frequency.robot         # Frequency accuracy (±1%)
    │   ├── test_pwm_duty_cycle.robot        # Duty cycle accuracy (±1%)
    │   └── test_pwm_negative.robot          # Invalid freq/duty values
    ├── 04_i2c/
    │   ├── test_i2c_do_expander.robot       # PCF8574/PCF8574A via AD3 I2C snoop + master
    │   └── test_i2c_negative.robot          # NACK, bus fault, conflicting master
    ├── 05_modbus_rtu/
    │   └── test_modbus_rtu_stub.robot       # FUTURE: requires vModbusRtuTask in firmware
    ├── 06_modbus_tcp/
    │   ├── test_modbus_tcp_coils.robot      # FC01, FC05, FC15 positive tests
    │   ├── test_modbus_tcp_registers.robot  # FC03, FC04, FC06, FC16 positive tests
    │   └── test_modbus_tcp_negative.robot   # Invalid addresses, quantities, transport errors
    └── 07_system/
        └── test_system_integration.robot    # End-to-end cross-domain tests
```

---

## Hardware Setup

### Required Hardware

| Item | Description |
|---|---|
| STM32H563ZI Nucleo | Device Under Test (DUT) |
| Analog Discovery 3 | Programmable instrument (Wavegen, Scope, DIO, I2C, UART) |
| Ethernet cable | DUT to host PC (for Modbus TCP) |
| USB cable | AD3 to host PC |
| Jumper wires | AD3 to DUT connections |

### Wiring

| AD3 Channel | AD3 Function | STM32H5 Nucleo Pin | STM32 Function |
|---|---|---|---|
| W1 (Wavegen 1) | Analog Out 0-3.3V | CN7 A0 (PA0) | ADC1 Channel A0 |
| W2 (Wavegen 2) | Analog Out 0-3.3V | CN7 A1 (PA1) | ADC1 Channel A1 |
| Scope CH1+ | Oscilloscope | CN10 PWM_0 | TIM PWM Out 0 |
| Scope CH2+ | Oscilloscope | CN10 PWM_1 | TIM PWM Out 1 |
| DIO 0-7 | Digital Output | CN8 DI0-DI7 | GPIO Digital Inputs |
| DIO 8-15 | Digital Input | PCF8574 DO0-DO7 | I2C Expander outputs |
| I2C SDA | I2C Analyzer | I2C4 SDA (PB7) | PCF8574/PCF8574A bus |
| I2C SCL | I2C Analyzer | I2C4 SCL (PB6) | PCF8574/PCF8574A bus |
| UART TX | UART | CN3 USART RX | Modbus RTU (FUTURE) |
| UART RX | UART | CN3 USART TX | Modbus RTU (FUTURE) |
| GND | Common Ground | GND | GND |

> **Note:** Use 4.7kΩ pull-ups on I2C SDA/SCL if not already present on the Nucleo board.

### System Prerequisites

1. **Digilent WaveForms SDK** installed on host OS:
   - Linux: `libdwf.so` from [Digilent WaveForms](https://digilent.com/reference/software/waveforms/waveforms-3/start)
   - Windows: `dwf.dll` from the same installer
2. **STM32_Programmer_CLI** on PATH (from STM32CubeCLT or STM32CubeProgrammer)
3. **uv** Python project manager installed

---

## Building and Running

### 1. Configure the project (fetches all dependencies from GitHub)

```bash
cmake -S . -B build -G Ninja
```

This runs `FetchContent` for:
- `robotframework` v7.2.2
- `pymodbus` v3.9.2
- `dwf` Python bindings
- `pyserial` v3.5

### 2. Install fetched dependencies into uv venv

```bash
cmake --build build --target robot_fetch_deps
```

### 3. Flash firmware to DUT

```bash
cmake --build build --target jerry_app
python tools/flash_nucleo.py
```

### 4. Run all HIL tests

```bash
cmake --build build --target robot_tests
```

Results are written to `build/robot_results/`.

### 5. Run only positive tests (CI fast path)

```bash
cmake --build build --target robot_tests_positive_only
```

### 6. Run only negative tests (nightly)

```bash
cmake --build build --target robot_tests_negative_only
```

### 7. Run directly with robot command

```bash
# All tests
uv run python -m robot --outputdir build/robot_results tests/robot/suites/

# Specific suite
uv run python -m robot --outputdir build/robot_results tests/robot/suites/02_adc/

# Override DUT IP
uv run python -m robot --variable DUT_HOST:192.168.1.50 \
    --outputdir build/robot_results tests/robot/suites/

# Skip firmware flash (DUT already programmed)
uv run python -m robot --variable SKIP_FLASH:True \
    --outputdir build/robot_results tests/robot/suites/

# Only negative tests
uv run python -m robot --include negative \
    --outputdir build/robot_results tests/robot/suites/
```

---

## Test Tags

| Tag | Description |
|---|---|
| `hardware` | Requires physical hardware (AD3 + DUT) |
| `positive` | Normal operation tests |
| `negative` | Error handling and boundary tests |
| `digital_io` | Digital I/O tests |
| `adc` | ADC tests |
| `pwm` | PWM tests |
| `i2c` | I2C expander tests |
| `modbus_tcp` | Modbus TCP tests |
| `modbus_rtu` | Modbus RTU tests (FUTURE) |
| `system` | End-to-end system integration tests |
| `future` | Not yet implemented (requires firmware changes) |
| `invalid_address` | Tests for out-of-range register addresses |
| `invalid_quantity` | Tests for invalid count values |
| `read_only_violation` | Tests for writes to read-only registers |
| `invalid_data_value` | Tests for NaN, Inf, out-of-range values |
| `bus_fault` | Tests for I2C bus fault conditions |
| `transport_error` | Tests for TCP connection errors |
| `framing_error` | Tests for RTU framing errors (FUTURE) |
| `boundary_condition` | Tests for min/max boundary values |

---

## Configuration

Edit `config/hardware_config.yaml` to match your lab setup:

```yaml
dut:
  modbus_tcp_host: "192.168.1.100"   # Update to your DUT IP
  modbus_tcp_port: 502
  modbus_unit_id: 1
```

---

## Modbus RTU Status

The Modbus RTU test suite (`05_modbus_rtu/`) is **FUTURE WORK**. The Jerry firmware
currently only exposes Modbus TCP via `vModbusTask`. The RTU library
(`modbus_rtu.c`) is compiled but no application task wires it to a UART peripheral.

To activate RTU tests:
1. Implement `vModbusRtuTask` in `application/src/`
2. Wire it to a UART peripheral (e.g., USART1)
3. Connect AD3 UART TX/RX to the STM32 UART RX/TX pins
4. Update `hardware_config.yaml` with the correct serial port
5. Replace `test_modbus_rtu_stub.robot` with the actual RTU test files

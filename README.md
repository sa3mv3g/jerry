# Jerry

Jerry is a data acquisition firmware designed for microcontrollers, specifically targeting the STM32H5xxx series. It is built with high reliability and security in mind, utilizing FreeRTOS with static allocation and adhering to strict coding standards.

## Project Architecture

-   **Firmware**: C and Assembly (No dynamic memory allocation).
-   **OS**: FreeRTOS (Statically allocated).
-   **Build System**: CMake.
-   **Tooling**: Python scripts managed by `uv`.
-   **Testing**: Unity (Firmware), Pytest (Tools).

## Key Features

-   **System Monitoring**: Stack overflow and usage tracking.
-   **FOTA**: Secure Firmware Over The Air updates.
-   **Communication**:
    -   Modbus TCP/IP (Ethernet).
    -   Modbus RTU (UART).
    -   Logging via dedicated UART.
-   **I/O Capabilities**:
    -   8x Digital Inputs.
    -   16x Digital Outputs.
    -   4x ADC Inputs.
    -   4x PWM Outputs.

## Development Setup

### Prerequisites

Assumption is that all the tools listed below are on terminal path.

-   **CMake**: >= 3.20
-   **Python**: >= 3.10
-   **uv**: Python project manager.
-   **Ninja**: Ninja build system
-   **CppCheck**: Cppcheck (https://cppcheck.sourceforge.io/) is used for static analysis. During installation, ensure you select the option to install Python addons (MISRA, etc.), or manually download them from the [cppcheck GitHub repository](https://github.com/danmar/cppcheck/tree/main/addons).

Vendor specific tools:
-   **STM32CubeCLT**: expects `STM32CLT_PATH` environment variable in shell.
-   **STM32CubeIDE**: expects `STM32CubeIDE` environment variable in shell.

### Build Instructions

1.  **Configure the project**:
    ```bash
    cmake -S . -B build -G Ninja -DCPPCHECK_USE_ADDONS=ON
    ```

2.  **Build the firmware**:
    ```bash
    cmake --build build --target jerry_app
    ```

This will produce two binaries:
-   `build/application/bsp/stm/stm32h563/jerry_secure_app.elf` (Secure Application)
-   `build/application/jerry_app.elf` (Non-Secure Application)

### CMake Configuration Options

The following CMake options can be passed during configuration to customize the build:

#### Core Options

| Option | Default | Description |
|--------|---------|-------------|
| `VENDOR` | `stm` | Microcontroller vendor selection |
| `CMAKE_BUILD_TYPE` | - | Build type: `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` |
| `CPPCHECK_USE_ADDONS` | `OFF` | Enable cppcheck MISRA and naming addons |
| `PYTHON_EXECUTABLE` | Auto-detected | Python interpreter for cppcheck addons (`py` on Windows, `python3` on Unix) |
| `UV_COMMAND` | `uv` | Command to invoke uv (e.g., `uv` or `py;-m;uv` for Windows) |

**Example with custom options:**
```bash
cmake -S . -B build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=application/bsp/toolchain.cmake \
    -DVENDOR=stm \
    -DCMAKE_BUILD_TYPE=Debug
```

## Available Commands

This project leverages CMake custom targets to integrate quality assurance tools.

### Code Formatting

Format C/C++ and Python code:
```bash
cmake --build build --target format
```

### Static Analysis & Linting

Run all quality checks (CppCheck, Lizard, Pylint, Ruff):
```bash
cmake --build build --target lint
```

Run individual tools:

-   **CppCheck** (Static Analysis):
    ```bash
    cmake --build build --target cppcheck
    ```
-   **CppCheck with MISRA** (MISRA C:2012 Compliance):
    ```bash
    # Configure with MISRA addon enabled
    cmake -S . -B build -G Ninja \
        -DCPPCHECK_USE_ADDONS=ON

    # Run cppcheck with MISRA checking
    cmake --build build --target cppcheck

    # View MISRA violations (Windows)
    type build\cppcheck_report.txt | findstr /i "misra"

    # View MISRA violations (Linux/macOS)
    grep -i "misra" build/cppcheck_report.txt
    ```
    **Note:** MISRA checking requires:
    - cppcheck with MISRA addon installed (typically at `<cppcheck-install>/share/cppcheck/addons/misra.py`)
    - Python interpreter (auto-detected, or override with `-DPYTHON_EXECUTABLE=/path/to/python`)
    - MISRA rule texts file at `refs/misra_c_2012__headlines_for_cppcheck.txt`

-   **Lizard** (Complexity Metrics):
    ```bash
    cmake --build build --target lizard
    ```
-   **Pylint** (Python Linting):
    ```bash
    cmake --build build --target pylint
    ```
-   **Ruff** (Python Formatting/Linting):
    ```bash
    cmake --build build --target ruff
    ```

## Coding Standards

-   **C/C++**: [Google CPP Coding Style](refs/cpp_coding_style.md)
-   **Python**: [Google Python Coding Style](refs/python_coding_style.md)
-   **Shell**: [Google Shell Coding Style](refs/shell_style_guide.md)

## Modbus Library

Jerry includes a custom Modbus library supporting RTU, ASCII, and TCP/IP protocols. The library is designed following SOLID principles with a callback-based architecture for maximum flexibility.

### Features

- **Protocol Support**: Modbus RTU, ASCII, and TCP/IP
- **Operating Modes**: Slave (Server) and Master (Client)
- **Function Codes**: FC01-FC06, FC15, FC16 (Read/Write Coils, Discrete Inputs, Holding Registers, Input Registers)
- **No Dynamic Allocation**: All memory is statically allocated
- **FreeRTOS Compatible**: Thread-safe design with static allocation

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│              (User Callbacks & Register Data)                │
├─────────────────────────────────────────────────────────────┤
│                      Core Layer                              │
│           (modbus_core.c, modbus_pdu.c)                     │
├─────────────────────────────────────────────────────────────┤
│                    Protocol Layer                            │
│      (modbus_rtu.c, modbus_ascii.c, modbus_tcp.c)           │
├─────────────────────────────────────────────────────────────┤
│                    Utility Layer                             │
│            (modbus_crc.c, modbus_lrc.c)                     │
└─────────────────────────────────────────────────────────────┘
```

### Code Generation Tool

A Python-based code generator is provided to create register definitions from JSON configuration:

```bash
# Install dependencies
pip install jinja2 jsonschema

# Generate code from JSON configuration
python tools/modbus_codegen/modbus_codegen.py \
    tools/modbus_codegen/examples/jerry_registers.json \
    --output-dir application/src/generated
```

This generates:
- `<device>_registers.h` - Register address definitions and data structures
- `<device>_registers.c` - Register data storage and initialization
- `<device>_callbacks.c` - Modbus callback implementations

### Modbus CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `MODBUS_ENABLE_RTU` | `ON` | Enable Modbus RTU protocol support |
| `MODBUS_ENABLE_ASCII` | `ON` | Enable Modbus ASCII protocol support |
| `MODBUS_ENABLE_TCP` | `ON` | Enable Modbus TCP/IP protocol support |

### Usage Example

```c
#include "modbus.h"
#include "modbus_callbacks.h"

/* Initialize Modbus context */
modbus_context_t* ctx;
modbus_config_t config = {
    .mode = MODBUS_MODE_SLAVE,
    .protocol = MODBUS_PROTOCOL_TCP,
    .unit_id = 1,
    .response_timeout_ms = 1000,
};

modbus_init(&ctx, &config);

/* Process incoming request (in your communication task) */
modbus_adu_t request, response;
bool send_response;

/* Parse received frame into request ADU */
modbus_tcp_parse_frame(rx_buffer, rx_length, &request);

/* Process request and generate response */
modbus_slave_process_adu(ctx, &request, &response, &send_response);

/* Build response frame */
if (send_response) {
    modbus_tcp_build_frame(&response, tx_buffer, sizeof(tx_buffer), &tx_length);
    /* Send tx_buffer... */
}
```

## Vendor Specific Notes

### STM32H563 (NUCLEO-H563ZI)

#### Pin Mapping

The following tables show the pin assignments for the NUCLEO-H563ZI board.

##### Digital Inputs (DI0-DI7)

| Channel | MCU Pin | GPIO Port | Nucleo Connector | Pin Number | Modbus Address (FC02) |
|---------|---------|-----------|------------------|------------|----------------------|
| DI0 | PB2 | GPIOB | CN10 | 22 | 0 |
| DI1 | PE9 | GPIOE | CN10 | 4 | 1 |
| DI2 | PF2 | GPIOF | CN9 | 28 | 2 |
| DI3 | PF1 | GPIOF | CN9 | 26 | 3 |
| DI4 | PF0 | GPIOF | CN9 | 24 | 4 |
| DI5 | PD0 | GPIOD | CN9 | 25 | 5 |
| DI6 | PD1 | GPIOD | CN9 | 27 | 6 |
| DI7 | PG0 | GPIOG | CN10 | 30 | 7 |

**Note:** All digital inputs are configured as GPIO inputs with no internal pull-up/pull-down resistors (`GPIO_NOPULL`). External pull resistors may be required depending on your signal source.

##### Digital Outputs (DO0-DO15)

Digital outputs are controlled via I2C GPIO expanders (PCF8574 and PCF8574A).

| Channel | I2C Device | I2C Address | Bit | Modbus Address (FC05/FC15) |
|---------|------------|-------------|-----|---------------------------|
| DO0 | PCF8574 | 0x20 | 0 | 0 |
| DO1 | PCF8574 | 0x20 | 1 | 1 |
| DO2 | PCF8574 | 0x20 | 2 | 2 |
| DO3 | PCF8574 | 0x20 | 3 | 3 |
| DO4 | PCF8574 | 0x20 | 4 | 4 |
| DO5 | PCF8574 | 0x20 | 5 | 5 |
| DO6 | PCF8574 | 0x20 | 6 | 6 |
| DO7 | PCF8574 | 0x20 | 7 | 7 |
| DO8 | PCF8574A | 0x21 | 0 | 8 |
| DO9 | PCF8574A | 0x21 | 1 | 9 |
| DO10 | PCF8574A | 0x21 | 2 | 10 |
| DO11 | PCF8574A | 0x21 | 3 | 11 |
| DO12 | PCF8574A | 0x21 | 4 | 12 |
| DO13 | PCF8574A | 0x21 | 5 | 13 |
| DO14 | PCF8574A | 0x21 | 6 | 14 |
| DO15 | PCF8574A | 0x21 | 7 | 15 |

**Note:** I2C3 is used for communication with the GPIO expanders (SDA: PB4, SCL: PA8).

##### Analog Inputs (ADC1)

| Channel | MCU Pin | ADC Channel | Nucleo Connector | Pin Number | Modbus Address (FC03) |
|---------|---------|-------------|------------------|------------|----------------------|
| A0 | PF11 | ADC1_IN2 | CN9 | 9 | 12 |
| A1 | PA6 | ADC1_IN3 | CN12 | 12 | 13 |
| A2 | PC0 | ADC1_IN10 | CN9 | 3 | 14 |
| A3 | PC2 | ADC1_IN12 | CN9 | 5 | 15 |
| A4 | PC3 | ADC1_IN13 | CN9 | 7 | - |
| A5 | PB1 | ADC1_IN5 | CN10 | 24 | - |

**Note:** ADC values are filtered using a 12-stage biquad cascade filter (4th order Butterworth LPF + 10 notch filters for 50Hz mains rejection). The filter runs continuously at 10kHz.

#### Device Configuration & Flashing (First Time Setup)

Since this project uses **TrustZone**, the STM32H563 device option bytes **MUST** be configured correctly before flashing. If the device is in a default state (TZEN=0), the application will not boot.

**WARNING:** Modifying Option Bytes, specifically enabling TZEN, may perform a mass erase of the flash memory.

##### 1. Enable TrustZone and Configure Secure Boot

Use `STM32_Programmer_CLI` (part of STM32CubeCLT) to set the Option Bytes. Here, it assumes that `STM32CubeCLT` is installed as this application sets up and put `STM32_Programmer_CLI` on terminal on path.

**Step 1: Enable TrustZone**
```bash
STM32_Programmer_CLI -c port=SWD -ob TZEN=0xB4
```
*Note: This enables TrustZone. The device may reset.*

**Step 2: Configure Secure Areas and Boot Address**
Once TrustZone is enabled, you must define the Secure memory regions and the Secure Boot Address.
-   **Bank 1**: Secure (default behavior with `SECWM1_STRT=0`, `SECWM1_END=0x7F`).
-   **Bank 2**: Non-Secure (configured by `SECWM2_STRT=0x7F`, `SECWM2_END=0x0`).
-   **Secure Boot Address**: Points to the start of Secure Flash (Bank 1).

Run this command:
```bash
STM32_Programmer_CLI -c port=SWD -ob SECWM2_STRT=0x7F SECWM2_END=0x0 SECBOOTADD=0x0C0000
```

##### 2. Flash the Firmware

You must flash **both** the Secure and Non-Secure applications.

**Flash Secure App (to 0x0C000000):**
```bash
STM32_Programmer_CLI -c port=SWD -w build/application/bsp/stm/stm32h563/jerry_secure_app.elf -v -rst
```

**Flash Non-Secure App (to 0x08100000):**
```bash
STM32_Programmer_CLI -c port=SWD -w build/application/jerry_app.elf -v -rst
```

#### 3. Debugging tips

Although a sample vscode's cortex-debug extension configuration has been provided. It is in `.vscode/launch.json`. User can select `OpenOCD STM32H563` launch config in `Run and Debug` view of vscode.

However, the recommendation is to use STM32CubeIDE environment. To set up the debug configuration for this project, follow the following steps:
1. import any example of Nucleo-H563 board, here, `Nx_perf` example has been imported.
2. Open `debug configuration` of this project.
3. In `Debugger` tab, `Debug Probe` should be `ST-LINK (OpenOCD)`
4. In `Startup` tab, make sure to have the configs as per the following image
![Startup configs](refs/imgs/debug_configuration_statup.png)
    As per these settings, `Nx_Perf` app binary will not be loaded, but rather we are loading only the symbol of our non-secure application. **Imp:** this assumes that both secure and non-secure part of this application has been downloaded (see `Flash the Firmware` section). Also, it does not runs to any symbol or resumes.

Once the above debug configuration file has been set
1. download the both secure and non-secure part of this application (see `Flash the Firmware` section).
2. Debug the `Nx_perf` as `STM32 C/C++ Application`.

#### 4. System Timer (SysTick) Behavior

In this TrustZone configuration:
-   **Secure World**: Uses the standard **Core SysTick** timer for its timebase.
-   **Non-Secure World**: Uses **TIM6** (Basic Timer) for its timebase (HAL Tick).

**Behavior Fix**:
By default, the Secure SysTick remains active after jumping to the Non-Secure application, causing periodic interrupts that preempt the Non-Secure code. To prevent this overhead (if Secure world background tasks are not needed), the Secure SysTick is explicitly **disabled** (`SysTick->CTRL = 0`) in `Secure/Core/Src/main.c` immediately before the jump to the Non-Secure vector table.

## TODO

1. Currently, `MX_Device.h` file has to be generated using the following command:
```bash
uv run tools/MX_Device_gen.py application/bsp/stm/stm32h563/stm32h563.ioc --logfile build/mx_device_gen.log.txt
```
This generation step is good to be automated.

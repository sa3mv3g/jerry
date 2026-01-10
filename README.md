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

-   **CMake**: >= 3.20
-   **Python**: >= 3.10
-   **uv**: Python project manager.
-   **Compiler**: Appropriate toolchain for the target microcontroller.

### Build Instructions

1.  **Configure the project**:
    ```bash
    cmake -S . -B build
    ```

2.  **Build the firmware**:
    ```bash
    cmake --build build
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

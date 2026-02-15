# Jerry Firmware Package

Jerry is a data acquisition firmware for the NUCLEO-H563ZI development board (STM32H563 microcontroller).

## Package Contents

| File | Description |
|------|-------------|
| `jerry_secure_app.elf` | Secure application (TrustZone secure world) |
| `jerry_app.elf` | Non-secure application (main firmware) |
| `flash_nucleo.py` | Automated flashing script |
| `jerry_device_register_map.txt` | Modbus register documentation |

## Requirements

### Hardware
- NUCLEO-H563ZI development board
- USB cable (Type-A to Micro-B)

### Software
- **Python 3.10+** - For running the flashing script
- **STM32CubeCLT** or **STM32CubeProgrammer** - For device programming
  - Download from: https://www.st.com/en/development-tools/stm32cubeprog.html
  - Ensure `STM32_Programmer_CLI` is on your system PATH

## Quick Start

### 1. Connect the Board

Connect the NUCLEO-H563ZI board to your computer via USB. The ST-LINK debugger should be recognized automatically.

### 2. Flash the Firmware

For a **virgin (new) board**, run the full flashing process:

```bash
python flash_nucleo.py
```

This will:
1. Enable TrustZone security
2. Configure secure memory regions
3. Flash the secure application
4. Flash the non-secure application

**Note:** Enabling TrustZone may trigger a mass erase of the device flash.

### 3. Verify Operation

After flashing, the board will reset and start running. The green LED should indicate normal operation.

## Flashing Options

### Skip TrustZone Configuration

If the board has already been configured for TrustZone (e.g., previously flashed):

```bash
python flash_nucleo.py --skip-option-bytes
```

### Dry Run Mode

To see what commands would be executed without actually running them:

```bash
python flash_nucleo.py --dry-run
```

### Custom Firmware Paths

To flash firmware from a different location:

```bash
python flash_nucleo.py --secure-app /path/to/secure.elf --nonsecure-app /path/to/app.elf
```

### All Options

```
usage: flash_nucleo.py [-h] [--build-dir BUILD_DIR] [--secure-app SECURE_APP]
                       [--nonsecure-app NONSECURE_APP]
                       [--programmer-path PROGRAMMER_PATH]
                       [--skip-option-bytes] [--option-bytes-only] [--force]
                       [--dry-run] [--verbose]

Options:
  --build-dir BUILD_DIR     Path to firmware directory (default: current dir)
  --secure-app SECURE_APP   Path to secure application ELF file
  --nonsecure-app NONSECURE_APP
                            Path to non-secure application ELF file
  --programmer-path PATH    Path to STM32_Programmer_CLI executable
  --skip-option-bytes       Skip TrustZone configuration
  --option-bytes-only       Only configure TrustZone, don't flash firmware
  --force, -f               Skip confirmation prompts
  --dry-run, -n             Print commands without executing
  --verbose, -v             Enable verbose output
```

## Communication Interfaces

### Modbus TCP/IP

- **Port:** 502
- **IP Address:** Configured via DHCP or static (see device configuration)
- **Unit ID:** 1

### Modbus RTU

- **UART:** USART3 (ST-LINK Virtual COM Port)
- **Baud Rate:** 115200
- **Data Bits:** 8
- **Parity:** None
- **Stop Bits:** 1

### Logging UART

- **UART:** USART1
- **Baud Rate:** 115200
- **Format:** Text-based log messages

## Modbus Register Map

See `jerry_device_register_map.txt` for the complete register map including:
- Coils (FC01/FC05/FC15)
- Discrete Inputs (FC02)
- Holding Registers (FC03/FC06/FC16)
- Input Registers (FC04)

## I/O Capabilities

| Type | Count | Description |
|------|-------|-------------|
| Digital Inputs | 8 | DI0-DI7 via GPIO |
| Digital Outputs | 16 | DO0-DO15 via I2C GPIO expanders |
| Analog Inputs | 4 | ADC1 channels with filtering |
| PWM Outputs | 4 | Timer-based PWM |

## Troubleshooting

### "STM32_Programmer_CLI not found"

Ensure STM32CubeProgrammer or STM32CubeCLT is installed and the `bin` directory is in your system PATH.

**Windows:**
```
C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin
```

**Linux:**
```
/opt/st/stm32cubeclt/STM32CubeProgrammer/bin
```

### "Failed to connect to target"

1. Check USB cable connection
2. Ensure ST-LINK drivers are installed
3. Try pressing the reset button on the board
4. Check if another application is using the ST-LINK

### "TrustZone already enabled" errors

If the board was previously configured, use `--skip-option-bytes`:

```bash
python flash_nucleo.py --skip-option-bytes
```

### Board not responding after flash

1. Press the reset button
2. If still unresponsive, try a full reflash without `--skip-option-bytes`

## Manual Flashing

If you prefer to flash manually using STM32_Programmer_CLI:

### Step 1: Enable TrustZone
```bash
STM32_Programmer_CLI -c port=SWD -ob TZEN=0xB4
```

### Step 2: Configure Secure Regions
```bash
STM32_Programmer_CLI -c port=SWD -ob SECWM2_STRT=0x7F SECWM2_END=0x0 SECBOOTADD=0x0C0000
```

### Step 3: Flash Secure Application
```bash
STM32_Programmer_CLI -c port=SWD -w jerry_secure_app.elf -v -rst
```

### Step 4: Flash Non-Secure Application
```bash
STM32_Programmer_CLI -c port=SWD -w jerry_app.elf -v -rst
```

## Support

For issues and feature requests, please contact the development team.

## License

Copyright (c) 2026 Advance Instrumentation 'n' Control Systems. All rights reserved.

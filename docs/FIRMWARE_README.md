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

## Pin Mapping

### Digital Inputs (DI0-DI7)

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

### Digital Outputs (DO0-DO15)

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

**Note:** I2C3 is used for communication with the GPIO expanders (SDA: PC9, SCL: PA8).

### Analog Inputs (ADC1)

| Channel | MCU Pin | ADC Channel | Nucleo Connector | Pin Number | Modbus Address (FC03) |
|---------|---------|-------------|------------------|------------|----------------------|
| A0 | PF11 | ADC1_IN2 | CN9 | 9 | 12 |
| A1 | PA6 | ADC1_IN3 | CN12 | 12 | 13 |
| A2 | PC0 | ADC1_IN10 | CN9 | 3 | 14 |
| A3 | PC2 | ADC1_IN12 | CN9 | 5 | 15 |
| A4 | PC3 | ADC1_IN13 | CN9 | 7 | - |
| A5 | PB1 | ADC1_IN5 | CN10 | 24 | - |

**Note:** ADC values are filtered using a 12-stage biquad cascade filter (4th order Butterworth LPF + 10 notch filters for 50Hz mains rejection). The filter runs continuously at 10kHz.

### Device Address Selection Pins (DEVADDR0-DEVADDR3)

The device address is configured via 4 GPIO input pins with internal pull-up resistors. These pins allow setting a unique Modbus unit ID for each device in a multi-device setup.

| Pin Name | MCU Pin | GPIO Port | Nucleo Connector | Pin Number | Pull Configuration |
|----------|---------|-----------|------------------|------------|-------------------|
| DEVADDR0 | PG14 | GPIOG | CN10 | 32 | Internal Pull-Up |
| DEVADDR1 | PE13 | GPIOE | CN10 | 10 | Internal Pull-Up |
| DEVADDR2 | PE14 | GPIOE | CN10 | 28 | Internal Pull-Up |
| DEVADDR3 | PE11 | GPIOE | CN10 | 6 | Internal Pull-Up |

**Note:** The device address is read as a 4-bit value (0-15) where DEVADDR0 is the LSB and DEVADDR3 is the MSB. Pins are active-low (connect to GND to set bit to 1). With all pins floating (pulled high), the device address is 0.

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

Copyright (c) 2026 . All rights reserved.

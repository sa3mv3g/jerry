"""
Modbus Integration Test Configuration

This module contains configuration settings for the pymodbus integration tests.
Register addresses are sourced from config/jerry_registers.json via RegisterMap
so they stay in sync with the firmware register map automatically.

Connection parameters are resolved in this priority order:
  1. pytest CLI options (--modbus-host, --modbus-port, --modbus-unit-id)
  2. Environment variables (MODBUS_HOST, MODBUS_PORT, MODBUS_UNIT_ID)
  3. Defaults defined below

To override for a specific test run:
    # Via CLI:
    uv run pytest tests/integration/ -m hardware \\
        --modbus-host 10.0.0.50 --modbus-unit-id 3

    # Via environment:
    MODBUS_HOST=10.0.0.50 MODBUS_UNIT_ID=3 uv run pytest tests/integration/ -m hardware
"""

import os

from register_map import RegisterMap

# ---------------------------------------------------------------------------
# Target device configuration
# Defaults can be overridden by environment variables or pytest CLI options.
# ---------------------------------------------------------------------------
MODBUS_HOST = os.environ.get("MODBUS_HOST", "192.168.1.100")
MODBUS_PORT = int(os.environ.get("MODBUS_PORT", "502"))
MODBUS_UNIT_ID = int(os.environ.get("MODBUS_UNIT_ID", "1"))

# Timeouts
TIMEOUT = float(os.environ.get("MODBUS_TIMEOUT", "3.0"))  # seconds
RECONNECT_DELAY = 1.0  # seconds

# ---------------------------------------------------------------------------
# Register addresses — sourced from jerry_registers.json via RegisterMap.
# Do NOT hardcode addresses here; use reg_map lookups so that address changes
# in the JSON are automatically reflected in all tests.
# ---------------------------------------------------------------------------
_reg_map = RegisterMap()

# Coils (FC01 read / FC05 write single / FC15 write multiple)
COIL_SYSTEM_ENABLE = _reg_map.coil("digital_output_0")
COIL_OUTPUT_1 = _reg_map.coil("digital_output_1")
COIL_OUTPUT_2 = _reg_map.coil("digital_output_2")
COIL_OUTPUT_3 = _reg_map.coil("digital_output_3")
COIL_OUTPUT_4 = _reg_map.coil("digital_output_4")
COIL_MIN_ADDR = 0
COIL_MAX_ADDR = 4

# Discrete Inputs (FC02)
DI_INPUT_1 = _reg_map.di("digital_input_0")
DI_INPUT_2 = _reg_map.di("digital_input_1")
DI_INPUT_3 = _reg_map.di("digital_input_2")
DI_INPUT_4 = _reg_map.di("digital_input_3")
DI_MIN_ADDR = 0
DI_MAX_ADDR = 4

# Holding Registers (FC03, FC06, FC16)
HR_DEVICE_ID = 0
HR_FIRMWARE_VERSION = 1
HR_SETPOINT_1 = _reg_map.hr("pwm_0_duty_cycle")
HR_SETPOINT_2 = _reg_map.hr("pwm_1_duty_cycle")
HR_CONTROL_MODE = 4
HR_ALARM_THRESHOLD = 5
HR_MIN_ADDR = 0
HR_MAX_ADDR = 5

# Application Version Holding Registers (FC03, read-only mirrors of input registers)
HR_APP_VERSION_MAJOR = _reg_map.hr("app_version_major")
HR_APP_VERSION_MINOR = _reg_map.hr("app_version_minor")
HR_APP_VERSION_PATCH = _reg_map.hr("app_version_patch")
HR_APP_BUILD_NUMBER = _reg_map.hr("app_build_number")
HR_APP_BUILD_NUMBER_LOW = HR_APP_BUILD_NUMBER + 1
HR_APP_BUILD_NUMBER_SIZE = _reg_map.hr_size("app_build_number")

# Input Registers (FC04)
IR_TEMPERATURE = 0
IR_HUMIDITY = 2
IR_PRESSURE = 4
IR_ADC_VALUE = _reg_map.ir("adc_0_value")
IR_UPTIME = 7
IR_MIN_ADDR = 0
IR_MAX_ADDR = 8

# Application Version Input Registers (FC04, read-only)
IR_APP_VERSION_MAJOR = _reg_map.ir("app_version_major")
IR_APP_VERSION_MINOR = _reg_map.ir("app_version_minor")
IR_APP_VERSION_PATCH = _reg_map.ir("app_version_patch")
IR_APP_BUILD_NUMBER = _reg_map.ir("app_build_number")
IR_APP_BUILD_NUMBER_LOW = IR_APP_BUILD_NUMBER + 1
IR_APP_BUILD_NUMBER_SIZE = _reg_map.ir_size("app_build_number")

# Test limits
MAX_COILS_READ = 2000
MAX_COILS_WRITE = 1968
MAX_REGISTERS_READ = 125
MAX_REGISTERS_WRITE = 123

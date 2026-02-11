"""
Modbus Integration Test Configuration

This module contains configuration settings for the pymodbus integration tests.
"""

# Target device configuration
MODBUS_HOST = "192.168.1.100"  # STM32 IP address - update as needed
MODBUS_PORT = 502
MODBUS_UNIT_ID = 1

# Timeouts
TIMEOUT = 3.0  # seconds
RECONNECT_DELAY = 1.0  # seconds

# Test register addresses (based on jerry_registers.json example)
# Coils (FC01, FC05, FC15)
COIL_SYSTEM_ENABLE = 0
COIL_OUTPUT_1 = 1
COIL_OUTPUT_2 = 2
COIL_OUTPUT_3 = 3
COIL_OUTPUT_4 = 4
COIL_MIN_ADDR = 0
COIL_MAX_ADDR = 4

# Discrete Inputs (FC02)
DI_INPUT_1 = 0
DI_INPUT_2 = 1
DI_INPUT_3 = 2
DI_INPUT_4 = 3
DI_FAULT_STATUS = 4
DI_MIN_ADDR = 0
DI_MAX_ADDR = 4

# Holding Registers (FC03, FC06, FC16)
HR_DEVICE_ID = 0
HR_FIRMWARE_VERSION = 1
HR_SETPOINT_1 = 2
HR_SETPOINT_2 = 3
HR_CONTROL_MODE = 4
HR_ALARM_THRESHOLD = 5
HR_MIN_ADDR = 0
HR_MAX_ADDR = 5

# Input Registers (FC04)
IR_TEMPERATURE = 0
IR_HUMIDITY = 2
IR_PRESSURE = 4
IR_ADC_VALUE = 6
IR_UPTIME = 7
IR_MIN_ADDR = 0
IR_MAX_ADDR = 8

# Test limits
MAX_COILS_READ = 2000
MAX_COILS_WRITE = 1968
MAX_REGISTERS_READ = 125
MAX_REGISTERS_WRITE = 123

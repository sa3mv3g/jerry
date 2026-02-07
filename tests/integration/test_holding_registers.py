"""
Modbus Holding Registers Integration Tests

Tests for reading and writing holding registers (FC03, FC06, FC16).
"""

import pytest
from pymodbus.exceptions import ModbusException

from test_config import (
    MODBUS_UNIT_ID,
    HR_DEVICE_ID,
    HR_FIRMWARE_VERSION,
    HR_SETPOINT_1,
    HR_SETPOINT_2,
    HR_CONTROL_MODE,
    HR_ALARM_THRESHOLD,
    HR_MIN_ADDR,
    HR_MAX_ADDR,
    MAX_REGISTERS_READ,
    MAX_REGISTERS_WRITE,
)


@pytest.mark.hardware
class TestReadHoldingRegisters:
    """Tests for Read Holding Registers (FC03)."""

    def test_read_single_holding_register(self, modbus_client, unit_id):
        """Test reading a single holding register."""
        result = modbus_client.read_holding_registers(
            address=HR_DEVICE_ID,
            count=1,
            slave=unit_id
        )

        assert not result.isError(), f"Read failed: {result}"
        assert len(result.registers) == 1

    def test_read_multiple_holding_registers(self, modbus_client, unit_id):
        """Test reading multiple holding registers."""
        count = HR_MAX_ADDR - HR_MIN_ADDR + 1
        result = modbus_client.read_holding_registers(
            address=HR_MIN_ADDR,
            count=count,
            slave=unit_id
        )

        assert not result.isError(), f"Read failed: {result}"
        assert len(result.registers) == count

    def test_read_holding_register_device_id(self, modbus_client, unit_id):
        """Test reading device ID register."""
        result = modbus_client.read_holding_registers(
            address=HR_DEVICE_ID,
            count=1,
            slave=unit_id
        )

        assert not result.isError()
        # Device ID should be a valid value
        assert 0 <= result.registers[0] <= 65535

    def test_read_holding_register_invalid_address(self, modbus_client, unit_id):
        """Test reading from invalid address returns exception."""
        result = modbus_client.read_holding_registers(
            address=65535,  # Invalid address
            count=1,
            slave=unit_id
        )

        # Should return Modbus exception (illegal data address)
        assert result.isError()

    def test_read_holding_registers_max_quantity(self, modbus_client, unit_id):
        """Test reading maximum allowed registers (125)."""
        # This test may fail if device doesn't have 125 consecutive registers
        result = modbus_client.read_holding_registers(
            address=0,
            count=MAX_REGISTERS_READ,
            slave=unit_id
        )

        # Either succeeds or returns illegal data address
        # (depending on device register map)
        if not result.isError():
            assert len(result.registers) == MAX_REGISTERS_READ


@pytest.mark.hardware
class TestWriteSingleRegister:
    """Tests for Write Single Register (FC06)."""

    def test_write_single_register(self, modbus_client, unit_id):
        """Test writing a single register."""
        test_value = 1234

        # Write value
        result = modbus_client.write_register(
            address=HR_SETPOINT_1,
            value=test_value,
            slave=unit_id
        )

        assert not result.isError(), f"Write failed: {result}"

        # Read back and verify
        read_result = modbus_client.read_holding_registers(
            address=HR_SETPOINT_1,
            count=1,
            slave=unit_id
        )

        assert not read_result.isError()
        assert read_result.registers[0] == test_value

    def test_write_single_register_zero(self, modbus_client, unit_id):
        """Test writing zero to a register."""
        result = modbus_client.write_register(
            address=HR_SETPOINT_1,
            value=0,
            slave=unit_id
        )

        assert not result.isError()

        # Verify
        read_result = modbus_client.read_holding_registers(
            address=HR_SETPOINT_1,
            count=1,
            slave=unit_id
        )

        assert read_result.registers[0] == 0

    def test_write_single_register_max_value(self, modbus_client, unit_id):
        """Test writing maximum value (65535) to a register."""
        result = modbus_client.write_register(
            address=HR_SETPOINT_1,
            value=65535,
            slave=unit_id
        )

        assert not result.isError()

        # Verify
        read_result = modbus_client.read_holding_registers(
            address=HR_SETPOINT_1,
            count=1,
            slave=unit_id
        )

        assert read_result.registers[0] == 65535

    def test_write_single_register_invalid_address(self, modbus_client, unit_id):
        """Test writing to invalid address returns exception."""
        result = modbus_client.write_register(
            address=65535,
            value=100,
            slave=unit_id
        )

        assert result.isError()


@pytest.mark.hardware
class TestWriteMultipleRegisters:
    """Tests for Write Multiple Registers (FC16)."""

    def test_write_multiple_registers(self, modbus_client, unit_id):
        """Test writing multiple registers."""
        test_values = [100, 200, 300]

        result = modbus_client.write_registers(
            address=HR_SETPOINT_1,
            values=test_values,
            slave=unit_id
        )

        assert not result.isError(), f"Write failed: {result}"

        # Read back and verify
        read_result = modbus_client.read_holding_registers(
            address=HR_SETPOINT_1,
            count=len(test_values),
            slave=unit_id
        )

        assert not read_result.isError()
        assert read_result.registers == test_values

    def test_write_multiple_registers_single(self, modbus_client, unit_id):
        """Test writing single register using FC16."""
        test_value = [5678]

        result = modbus_client.write_registers(
            address=HR_SETPOINT_2,
            values=test_value,
            slave=unit_id
        )

        assert not result.isError()

    def test_write_multiple_registers_invalid_address(self, modbus_client, unit_id):
        """Test writing to invalid address range."""
        result = modbus_client.write_registers(
            address=65530,
            values=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
            slave=unit_id
        )

        assert result.isError()


@pytest.mark.hardware
class TestReadWriteVerify:
    """Tests that write values and verify by reading back."""

    def test_write_read_verify_pattern(self, modbus_client, unit_id):
        """Test write-read-verify with various patterns."""
        patterns = [
            [0x0000],
            [0xFFFF],
            [0x5555],
            [0xAAAA],
            [0x1234],
        ]

        for pattern in patterns:
            # Write
            result = modbus_client.write_registers(
                address=HR_SETPOINT_1,
                values=pattern,
                slave=unit_id
            )
            assert not result.isError(), f"Write failed for pattern {pattern}"

            # Read
            read_result = modbus_client.read_holding_registers(
                address=HR_SETPOINT_1,
                count=1,
                slave=unit_id
            )
            assert not read_result.isError()
            assert read_result.registers == pattern, \
                f"Mismatch: wrote {pattern}, read {read_result.registers}"

    def test_write_read_verify_sequence(self, modbus_client, unit_id):
        """Test write-read-verify with sequential values."""
        values = list(range(1, 4))  # [1, 2, 3]

        # Write
        result = modbus_client.write_registers(
            address=HR_SETPOINT_1,
            values=values,
            slave=unit_id
        )
        assert not result.isError()

        # Read
        read_result = modbus_client.read_holding_registers(
            address=HR_SETPOINT_1,
            count=len(values),
            slave=unit_id
        )
        assert not read_result.isError()
        assert read_result.registers == values

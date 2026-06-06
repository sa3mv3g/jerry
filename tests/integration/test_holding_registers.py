"""
Modbus Holding Registers Integration Tests

Tests for reading and writing holding registers (FC03, FC06, FC16).
"""

import pytest

from test_config import (
    HR_DEVICE_ID,
    HR_SETPOINT_1,
    HR_SETPOINT_2,
    HR_MIN_ADDR,
    HR_MAX_ADDR,
    HR_APP_VERSION_MAJOR,
    HR_APP_VERSION_MINOR,
    HR_APP_VERSION_PATCH,
    HR_APP_BUILD_NUMBER,
    HR_APP_BUILD_NUMBER_SIZE,
    MAX_REGISTERS_READ,
)


@pytest.mark.hardware
class TestReadHoldingRegisters:
    """Tests for Read Holding Registers (FC03)."""

    def test_read_single_holding_register(self, modbus_client, unit_id):
        """Test reading a single holding register."""
        result = modbus_client.read_holding_registers(
            address=HR_DEVICE_ID, count=1, device_id=unit_id
        )

        assert not result.isError(), f"Read failed: {result}"
        assert len(result.registers) == 1

    def test_read_multiple_holding_registers(self, modbus_client, unit_id):
        """Test reading multiple holding registers."""
        count = HR_MAX_ADDR - HR_MIN_ADDR + 1
        result = modbus_client.read_holding_registers(
            address=HR_MIN_ADDR, count=count, device_id=unit_id
        )

        assert not result.isError(), f"Read failed: {result}"
        assert len(result.registers) == count

    def test_read_holding_register_device_id(self, modbus_client, unit_id):
        """Test reading device ID register."""
        result = modbus_client.read_holding_registers(
            address=HR_DEVICE_ID, count=1, device_id=unit_id
        )

        assert not result.isError()
        # Device ID should be a valid value
        assert 0 <= result.registers[0] <= 65535

    def test_read_holding_register_invalid_address(self, modbus_client, unit_id):
        """Test reading from invalid address returns exception."""
        result = modbus_client.read_holding_registers(
            address=65535,  # Invalid address
            count=1,
            device_id=unit_id,
        )

        # Should return Modbus exception (illegal data address)
        assert result.isError()

    def test_read_holding_registers_max_quantity(self, modbus_client, unit_id):
        """Test reading maximum allowed registers (125)."""
        # This test may fail if device doesn't have 125 consecutive registers
        result = modbus_client.read_holding_registers(
            address=0, count=MAX_REGISTERS_READ, device_id=unit_id
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
            address=HR_SETPOINT_1, value=test_value, device_id=unit_id
        )

        assert not result.isError(), f"Write failed: {result}"

        # Read back and verify
        read_result = modbus_client.read_holding_registers(
            address=HR_SETPOINT_1, count=1, device_id=unit_id
        )

        assert not read_result.isError()
        assert read_result.registers[0] == test_value

    def test_write_single_register_zero(self, modbus_client, unit_id):
        """Test writing zero to a register."""
        result = modbus_client.write_register(
            address=HR_SETPOINT_1, value=0, device_id=unit_id
        )

        assert not result.isError()

        # Verify
        read_result = modbus_client.read_holding_registers(
            address=HR_SETPOINT_1, count=1, device_id=unit_id
        )

        assert read_result.registers[0] == 0

    def test_write_single_register_max_value(self, modbus_client, unit_id):
        """Test writing maximum value (65535) to a register."""
        result = modbus_client.write_register(
            address=HR_SETPOINT_1, value=65535, device_id=unit_id
        )

        assert not result.isError()

        # Verify
        read_result = modbus_client.read_holding_registers(
            address=HR_SETPOINT_1, count=1, device_id=unit_id
        )

        assert read_result.registers[0] == 65535

    def test_write_single_register_invalid_address(self, modbus_client, unit_id):
        """Test writing to invalid address returns exception."""
        result = modbus_client.write_register(address=65535, value=100, device_id=unit_id)

        assert result.isError()


@pytest.mark.hardware
class TestWriteMultipleRegisters:
    """Tests for Write Multiple Registers (FC16)."""

    def test_write_multiple_registers(self, modbus_client, unit_id):
        """Test writing multiple registers."""
        test_values = [100, 200, 300]

        result = modbus_client.write_registers(
            address=HR_SETPOINT_1, values=test_values, device_id=unit_id
        )

        assert not result.isError(), f"Write failed: {result}"

        # Read back and verify
        read_result = modbus_client.read_holding_registers(
            address=HR_SETPOINT_1, count=len(test_values), device_id=unit_id
        )

        assert not read_result.isError()
        assert read_result.registers == test_values

    def test_write_multiple_registers_single(self, modbus_client, unit_id):
        """Test writing single register using FC16."""
        test_value = [5678]

        result = modbus_client.write_registers(
            address=HR_SETPOINT_2, values=test_value, device_id=unit_id
        )

        assert not result.isError()

    def test_write_multiple_registers_invalid_address(self, modbus_client, unit_id):
        """Test writing to invalid address range."""
        result = modbus_client.write_registers(
            address=65530, values=[1, 2, 3, 4, 5, 6, 7, 8, 9, 10], device_id=unit_id
        )

        assert result.isError()


@pytest.mark.hardware
class TestAppVersionRegisters:
    """Tests for application version and build number holding registers (FC03).

    These registers are read-only mirrors of the input registers.
    Addresses: HR 300 (major), 301 (minor), 302 (patch), 303-304 (build number).
    """

    def test_read_version_major(self, modbus_client, unit_id):
        """Test reading application version major from holding register 300."""
        result = modbus_client.read_holding_registers(
            address=HR_APP_VERSION_MAJOR,
            count=1,
            device_id=unit_id,
        )

        assert not result.isError(), f"Read HR {HR_APP_VERSION_MAJOR} failed: {result}"
        assert len(result.registers) == 1
        # Major version must be a valid uint16 (0–65535)
        assert 0 <= result.registers[0] <= 65535

    def test_read_version_minor(self, modbus_client, unit_id):
        """Test reading application version minor from holding register 301."""
        result = modbus_client.read_holding_registers(
            address=HR_APP_VERSION_MINOR,
            count=1,
            device_id=unit_id,
        )

        assert not result.isError(), f"Read HR {HR_APP_VERSION_MINOR} failed: {result}"
        assert 0 <= result.registers[0] <= 65535

    def test_read_version_patch(self, modbus_client, unit_id):
        """Test reading application version patch from holding register 302."""
        result = modbus_client.read_holding_registers(
            address=HR_APP_VERSION_PATCH,
            count=1,
            device_id=unit_id,
        )

        assert not result.isError(), f"Read HR {HR_APP_VERSION_PATCH} failed: {result}"
        assert 0 <= result.registers[0] <= 65535

    def test_read_build_number(self, modbus_client, unit_id):
        """Test reading 32-bit build number (Unix timestamp) from HR 303-304.

        The build number is stored big-endian across two consecutive registers:
          HR 303 = high 16 bits
          HR 304 = low  16 bits
        Reconstructed as: (high << 16) | low
        """
        result = modbus_client.read_holding_registers(
            address=HR_APP_BUILD_NUMBER,
            count=HR_APP_BUILD_NUMBER_SIZE,
            device_id=unit_id,
        )

        assert not result.isError(), f"Read HR {HR_APP_BUILD_NUMBER} failed: {result}"
        assert len(result.registers) == HR_APP_BUILD_NUMBER_SIZE

        high = result.registers[0]
        low = result.registers[1]
        build_number = (high << 16) | low

        # Build number is a Unix timestamp — must be > 0 and a plausible epoch value
        # (> 2020-01-01 = 1577836800, < 2100-01-01 = 4102444800)
        assert build_number > 1_577_836_800, (
            f"Build number {build_number} is too small to be a valid Unix timestamp"
        )
        assert build_number < 4_102_444_800, (
            f"Build number {build_number} is too large to be a valid Unix timestamp"
        )

    def test_read_all_version_registers(self, modbus_client, unit_id):
        """Test reading all 5 version registers in a single FC03 request (HR 300-304)."""
        result = modbus_client.read_holding_registers(
            address=HR_APP_VERSION_MAJOR,
            count=5,  # major, minor, patch, build_high, build_low
            device_id=unit_id,
        )

        assert not result.isError(), f"Bulk read of version registers failed: {result}"
        assert len(result.registers) == 5

        major = result.registers[0]
        minor = result.registers[1]
        patch = result.registers[2]
        build_number = (result.registers[3] << 16) | result.registers[4]

        print(
            f"\n  Firmware version: {major}.{minor}.{patch}"
            f"  Build number: {build_number}"
        )

        assert 0 <= major <= 65535
        assert 0 <= minor <= 65535
        assert 0 <= patch <= 65535
        assert build_number > 0

    def test_write_version_major_is_silently_ignored(self, modbus_client, unit_id):
        """Test that writing to version major register is silently ignored (read-only).

        The device accepts the write (returns EXCEPTION_NONE) but the value
        is not stored — a subsequent read must return the original firmware value.
        """
        # Read current value
        read_before = modbus_client.read_holding_registers(
            address=HR_APP_VERSION_MAJOR,
            count=1,
            device_id=unit_id,
        )
        assert not read_before.isError()
        original_value = read_before.registers[0]

        # Attempt to write a different value
        write_result = modbus_client.write_register(
            address=HR_APP_VERSION_MAJOR,
            value=0xBEEF,
            device_id=unit_id,
        )
        # Write must succeed (no exception) — silent ignore behaviour
        assert not write_result.isError(), (
            f"Write to read-only version register raised unexpected exception: {write_result}"
        )

        # Read back — value must be unchanged
        read_after = modbus_client.read_holding_registers(
            address=HR_APP_VERSION_MAJOR,
            count=1,
            device_id=unit_id,
        )
        assert not read_after.isError()
        assert read_after.registers[0] == original_value, (
            f"Version major changed after write: "
            f"before={original_value}, after={read_after.registers[0]}"
        )

    def test_write_build_number_is_silently_ignored(self, modbus_client, unit_id):
        """Test that writing to build number registers is silently ignored."""
        # Read current build number
        read_before = modbus_client.read_holding_registers(
            address=HR_APP_BUILD_NUMBER,
            count=HR_APP_BUILD_NUMBER_SIZE,
            device_id=unit_id,
        )
        assert not read_before.isError()
        original_high = read_before.registers[0]
        original_low = read_before.registers[1]

        # Attempt to overwrite both build number registers
        write_result = modbus_client.write_registers(
            address=HR_APP_BUILD_NUMBER,
            values=[0xDEAD, 0xBEEF],
            device_id=unit_id,
        )
        assert not write_result.isError()

        # Read back — values must be unchanged
        read_after = modbus_client.read_holding_registers(
            address=HR_APP_BUILD_NUMBER,
            count=HR_APP_BUILD_NUMBER_SIZE,
            device_id=unit_id,
        )
        assert not read_after.isError()
        assert read_after.registers[0] == original_high
        assert read_after.registers[1] == original_low


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
                address=HR_SETPOINT_1, values=pattern, device_id=unit_id
            )
            assert not result.isError(), f"Write failed for pattern {pattern}"

            # Read
            read_result = modbus_client.read_holding_registers(
                address=HR_SETPOINT_1, count=1, device_id=unit_id
            )
            assert not read_result.isError()
            assert read_result.registers == pattern, (
                f"Mismatch: wrote {pattern}, read {read_result.registers}"
            )

    def test_write_read_verify_sequence(self, modbus_client, unit_id):
        """Test write-read-verify with sequential values."""
        values = list(range(1, 4))  # [1, 2, 3]

        # Write
        result = modbus_client.write_registers(
            address=HR_SETPOINT_1, values=values, device_id=unit_id
        )
        assert not result.isError()

        # Read
        read_result = modbus_client.read_holding_registers(
            address=HR_SETPOINT_1, count=len(values), device_id=unit_id
        )
        assert not read_result.isError()
        assert read_result.registers == values

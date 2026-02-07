"""
Modbus Coils Integration Tests

Tests for reading and writing coils (FC01, FC05, FC15).
"""

import pytest
from pymodbus.exceptions import ModbusException

from test_config import (
    MODBUS_UNIT_ID,
    COIL_SYSTEM_ENABLE,
    COIL_OUTPUT_1,
    COIL_OUTPUT_2,
    COIL_OUTPUT_3,
    COIL_OUTPUT_4,
    COIL_MIN_ADDR,
    COIL_MAX_ADDR,
    MAX_COILS_READ,
    MAX_COILS_WRITE,
)


@pytest.mark.hardware
class TestReadCoils:
    """Tests for Read Coils (FC01)."""

    def test_read_single_coil(self, modbus_client, unit_id):
        """Test reading a single coil."""
        result = modbus_client.read_coils(
            address=COIL_SYSTEM_ENABLE,
            count=1,
            slave=unit_id
        )

        assert not result.isError(), f"Read failed: {result}"
        assert len(result.bits) >= 1
        assert result.bits[0] in [True, False]

    def test_read_multiple_coils(self, modbus_client, unit_id):
        """Test reading multiple coils."""
        count = COIL_MAX_ADDR - COIL_MIN_ADDR + 1
        result = modbus_client.read_coils(
            address=COIL_MIN_ADDR,
            count=count,
            slave=unit_id
        )

        assert not result.isError(), f"Read failed: {result}"
        assert len(result.bits) >= count

    def test_read_coil_invalid_address(self, modbus_client, unit_id):
        """Test reading from invalid address returns exception."""
        result = modbus_client.read_coils(
            address=65535,
            count=1,
            slave=unit_id
        )

        assert result.isError()


@pytest.mark.hardware
class TestWriteSingleCoil:
    """Tests for Write Single Coil (FC05)."""

    def test_write_single_coil_on(self, modbus_client, unit_id):
        """Test writing coil ON."""
        result = modbus_client.write_coil(
            address=COIL_OUTPUT_1,
            value=True,
            slave=unit_id
        )

        assert not result.isError(), f"Write failed: {result}"

        # Read back and verify
        read_result = modbus_client.read_coils(
            address=COIL_OUTPUT_1,
            count=1,
            slave=unit_id
        )

        assert not read_result.isError()
        assert read_result.bits[0] == True

    def test_write_single_coil_off(self, modbus_client, unit_id):
        """Test writing coil OFF."""
        result = modbus_client.write_coil(
            address=COIL_OUTPUT_1,
            value=False,
            slave=unit_id
        )

        assert not result.isError()

        # Read back and verify
        read_result = modbus_client.read_coils(
            address=COIL_OUTPUT_1,
            count=1,
            slave=unit_id
        )

        assert not read_result.isError()
        assert read_result.bits[0] == False

    def test_write_single_coil_toggle(self, modbus_client, unit_id):
        """Test toggling a coil ON and OFF."""
        # Turn ON
        modbus_client.write_coil(address=COIL_OUTPUT_2, value=True, slave=unit_id)
        result = modbus_client.read_coils(address=COIL_OUTPUT_2, count=1, slave=unit_id)
        assert result.bits[0] == True

        # Turn OFF
        modbus_client.write_coil(address=COIL_OUTPUT_2, value=False, slave=unit_id)
        result = modbus_client.read_coils(address=COIL_OUTPUT_2, count=1, slave=unit_id)
        assert result.bits[0] == False

    def test_write_single_coil_invalid_address(self, modbus_client, unit_id):
        """Test writing to invalid address returns exception."""
        result = modbus_client.write_coil(
            address=65535,
            value=True,
            slave=unit_id
        )

        assert result.isError()


@pytest.mark.hardware
class TestWriteMultipleCoils:
    """Tests for Write Multiple Coils (FC15)."""

    def test_write_multiple_coils(self, modbus_client, unit_id):
        """Test writing multiple coils."""
        values = [True, False, True, False]

        result = modbus_client.write_coils(
            address=COIL_OUTPUT_1,
            values=values,
            slave=unit_id
        )

        assert not result.isError(), f"Write failed: {result}"

        # Read back and verify
        read_result = modbus_client.read_coils(
            address=COIL_OUTPUT_1,
            count=len(values),
            slave=unit_id
        )

        assert not read_result.isError()
        for i, expected in enumerate(values):
            assert read_result.bits[i] == expected

    def test_write_multiple_coils_all_on(self, modbus_client, unit_id):
        """Test writing all coils ON."""
        count = COIL_MAX_ADDR - COIL_MIN_ADDR + 1
        values = [True] * count

        result = modbus_client.write_coils(
            address=COIL_MIN_ADDR,
            values=values,
            slave=unit_id
        )

        assert not result.isError()

    def test_write_multiple_coils_all_off(self, modbus_client, unit_id):
        """Test writing all coils OFF."""
        count = COIL_MAX_ADDR - COIL_MIN_ADDR + 1
        values = [False] * count

        result = modbus_client.write_coils(
            address=COIL_MIN_ADDR,
            values=values,
            slave=unit_id
        )

        assert not result.isError()

    def test_write_multiple_coils_invalid_address(self, modbus_client, unit_id):
        """Test writing to invalid address range."""
        result = modbus_client.write_coils(
            address=65530,
            values=[True, False, True, False, True, False],
            slave=unit_id
        )

        assert result.isError()


@pytest.mark.hardware
class TestCoilPatterns:
    """Tests with various coil patterns."""

    def test_alternating_pattern(self, modbus_client, unit_id):
        """Test alternating ON/OFF pattern."""
        pattern = [True, False, True, False]

        modbus_client.write_coils(
            address=COIL_OUTPUT_1,
            values=pattern,
            slave=unit_id
        )

        result = modbus_client.read_coils(
            address=COIL_OUTPUT_1,
            count=len(pattern),
            slave=unit_id
        )

        assert not result.isError()
        for i, expected in enumerate(pattern):
            assert result.bits[i] == expected

    def test_inverse_alternating_pattern(self, modbus_client, unit_id):
        """Test inverse alternating pattern."""
        pattern = [False, True, False, True]

        modbus_client.write_coils(
            address=COIL_OUTPUT_1,
            values=pattern,
            slave=unit_id
        )

        result = modbus_client.read_coils(
            address=COIL_OUTPUT_1,
            count=len(pattern),
            slave=unit_id
        )

        assert not result.isError()
        for i, expected in enumerate(pattern):
            assert result.bits[i] == expected

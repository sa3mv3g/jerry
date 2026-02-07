"""
Unit Tests for Modbus Code Generator

This module tests the modbus_codegen.py tool, specifically:
- MAX_ADDR calculation for multi-register values (uint32)
- Address range calculations
- Configuration preprocessing
"""

import sys
from pathlib import Path

import pytest

# Add tools directory to path for importing modbus_codegen
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "tools" / "modbus_codegen"))

from modbus_codegen import ModbusCodeGenerator


class TestMaxAddrCalculation:
    """Tests for MAX_ADDR calculation with multi-register values."""

    @pytest.fixture
    def generator(self, tmp_path):
        """Create a ModbusCodeGenerator instance with templates."""
        template_dir = Path(__file__).parent.parent.parent / "tools" / "modbus_codegen" / "templates"
        return ModbusCodeGenerator(template_dir=template_dir)

    def test_single_register_max_addr(self, generator):
        """Test MAX_ADDR calculation with single-register (uint16) values only."""
        config = {
            "device": {"name": "test_device"},
            "registers": {
                "holding_registers": [
                    {"name": "reg1", "address": 100, "data_type": "uint16"},
                    {"name": "reg2", "address": 101, "data_type": "uint16"},
                    {"name": "reg3", "address": 102, "data_type": "uint16"},
                ]
            }
        }

        processed = generator._preprocess_config(config)

        # With single registers, max_addr should be the highest address
        assert processed["stats"]["holding_registers_max_addr"] == 102
        assert processed["stats"]["holding_registers_min_addr"] == 100

    def test_multi_register_max_addr_uint32(self, generator):
        """Test MAX_ADDR calculation with uint32 (2 registers) at the end.

        This is the bug that was fixed: when a uint32 is at address 303,
        it spans addresses 303-304, so MAX_ADDR should be 304, not 303.
        """
        config = {
            "device": {"name": "test_device"},
            "registers": {
                "holding_registers": [
                    {"name": "reg1", "address": 100, "data_type": "uint16"},
                    {"name": "reg2", "address": 200, "data_type": "uint16"},
                    {"name": "build_number", "address": 303, "data_type": "uint32", "size": 2},
                ]
            }
        }

        processed = generator._preprocess_config(config)

        # uint32 at address 303 spans 303-304, so max_addr should be 304
        assert processed["stats"]["holding_registers_max_addr"] == 304
        assert processed["stats"]["holding_registers_min_addr"] == 100

    def test_multi_register_max_addr_in_middle(self, generator):
        """Test MAX_ADDR when uint32 is in the middle, not at the end."""
        config = {
            "device": {"name": "test_device"},
            "registers": {
                "holding_registers": [
                    {"name": "reg1", "address": 100, "data_type": "uint16"},
                    {"name": "build_number", "address": 200, "data_type": "uint32", "size": 2},
                    {"name": "reg3", "address": 500, "data_type": "uint16"},
                ]
            }
        }

        processed = generator._preprocess_config(config)

        # reg3 at 500 is the highest, so max_addr should be 500
        assert processed["stats"]["holding_registers_max_addr"] == 500
        assert processed["stats"]["holding_registers_min_addr"] == 100

    def test_input_registers_max_addr(self, generator):
        """Test MAX_ADDR calculation for input registers with uint32."""
        config = {
            "device": {"name": "test_device"},
            "registers": {
                "input_registers": [
                    {"name": "reg1", "address": 0, "data_type": "uint16"},
                    {"name": "reg2", "address": 50, "data_type": "uint16"},
                    {"name": "build_number", "address": 103, "data_type": "uint32", "size": 2},
                ]
            }
        }

        processed = generator._preprocess_config(config)

        # uint32 at address 103 spans 103-104, so max_addr should be 104
        assert processed["stats"]["input_registers_max_addr"] == 104
        assert processed["stats"]["input_registers_min_addr"] == 0

    def test_coils_max_addr(self, generator):
        """Test MAX_ADDR calculation for coils (always size 1)."""
        config = {
            "device": {"name": "test_device"},
            "registers": {
                "coils": [
                    {"name": "coil1", "address": 0},
                    {"name": "coil2", "address": 5},
                    {"name": "coil3", "address": 10},
                ]
            }
        }

        processed = generator._preprocess_config(config)

        # Coils are always size 1
        assert processed["stats"]["coils_max_addr"] == 10
        assert processed["stats"]["coils_min_addr"] == 0

    def test_discrete_inputs_max_addr(self, generator):
        """Test MAX_ADDR calculation for discrete inputs (always size 1)."""
        config = {
            "device": {"name": "test_device"},
            "registers": {
                "discrete_inputs": [
                    {"name": "di1", "address": 0},
                    {"name": "di2", "address": 7},
                    {"name": "di3", "address": 15},
                ]
            }
        }

        processed = generator._preprocess_config(config)

        # Discrete inputs are always size 1
        assert processed["stats"]["discrete_inputs_max_addr"] == 15
        assert processed["stats"]["discrete_inputs_min_addr"] == 0

    def test_empty_registers(self, generator):
        """Test MAX_ADDR calculation with empty register lists."""
        config = {
            "device": {"name": "test_device"},
            "registers": {
                "holding_registers": [],
                "input_registers": [],
                "coils": [],
                "discrete_inputs": [],
            }
        }

        processed = generator._preprocess_config(config)

        # Empty lists should have 0 for both min and max
        assert processed["stats"]["holding_registers_max_addr"] == 0
        assert processed["stats"]["holding_registers_min_addr"] == 0
        assert processed["stats"]["input_registers_max_addr"] == 0
        assert processed["stats"]["input_registers_min_addr"] == 0

    def test_missing_size_defaults_to_one(self, generator):
        """Test that missing 'size' field defaults to 1."""
        config = {
            "device": {"name": "test_device"},
            "registers": {
                "holding_registers": [
                    {"name": "reg1", "address": 100},  # No size specified
                    {"name": "reg2", "address": 200},  # No size specified
                ]
            }
        }

        processed = generator._preprocess_config(config)

        # Without size, each register is size 1
        assert processed["stats"]["holding_registers_max_addr"] == 200
        assert processed["stats"]["holding_registers_min_addr"] == 100

    def test_jerry_device_scenario(self, generator):
        """Test the actual Jerry device scenario that exposed the bug.

        The jerry_registers.json has:
        - Holding register app_build_number at address 303 with size 2 (uint32)
        - Input register app_build_number at address 103 with size 2 (uint32)

        Before the fix:
        - JERRY_DEVICE_HR_MAX_ADDR was 303 (wrong)
        - JERRY_DEVICE_IR_MAX_ADDR was 103 (wrong)

        After the fix:
        - JERRY_DEVICE_HR_MAX_ADDR should be 304
        - JERRY_DEVICE_IR_MAX_ADDR should be 104
        """
        config = {
            "device": {"name": "jerry_device"},
            "registers": {
                "holding_registers": [
                    {"name": "device_id", "address": 0, "data_type": "uint16", "size": 1},
                    {"name": "firmware_major", "address": 300, "data_type": "uint16", "size": 1},
                    {"name": "firmware_minor", "address": 301, "data_type": "uint16", "size": 1},
                    {"name": "firmware_patch", "address": 302, "data_type": "uint16", "size": 1},
                    {"name": "app_build_number", "address": 303, "data_type": "uint32", "size": 2},
                ],
                "input_registers": [
                    {"name": "device_status", "address": 0, "data_type": "uint16", "size": 1},
                    {"name": "firmware_major", "address": 100, "data_type": "uint16", "size": 1},
                    {"name": "firmware_minor", "address": 101, "data_type": "uint16", "size": 1},
                    {"name": "firmware_patch", "address": 102, "data_type": "uint16", "size": 1},
                    {"name": "app_build_number", "address": 103, "data_type": "uint32", "size": 2},
                ],
            }
        }

        processed = generator._preprocess_config(config)

        # This is the critical assertion that validates the bug fix
        assert processed["stats"]["holding_registers_max_addr"] == 304, \
            "HR MAX_ADDR should be 304 (303 + 2 - 1) for uint32 at address 303"
        assert processed["stats"]["input_registers_max_addr"] == 104, \
            "IR MAX_ADDR should be 104 (103 + 2 - 1) for uint32 at address 103"


class TestHelperFunctions:
    """Tests for helper functions in ModbusCodeGenerator."""

    def test_to_upper_snake_case(self):
        """Test conversion to UPPER_SNAKE_CASE."""
        assert ModbusCodeGenerator._to_upper_snake_case("deviceId") == "DEVICE_ID"
        assert ModbusCodeGenerator._to_upper_snake_case("firmwareMajor") == "FIRMWARE_MAJOR"
        assert ModbusCodeGenerator._to_upper_snake_case("simple") == "SIMPLE"
        assert ModbusCodeGenerator._to_upper_snake_case("ABC") == "ABC"

    def test_to_camel_case(self):
        """Test conversion to CamelCase."""
        assert ModbusCodeGenerator._to_camel_case("device_id") == "DeviceId"
        assert ModbusCodeGenerator._to_camel_case("firmware_major") == "FirmwareMajor"
        assert ModbusCodeGenerator._to_camel_case("simple") == "Simple"


class TestStatisticsCalculation:
    """Tests for statistics calculation in preprocessing."""

    @pytest.fixture
    def generator(self):
        """Create a ModbusCodeGenerator instance."""
        template_dir = Path(__file__).parent.parent.parent / "tools" / "modbus_codegen" / "templates"
        return ModbusCodeGenerator(template_dir=template_dir)

    def test_register_counts(self, generator):
        """Test that register counts are calculated correctly."""
        config = {
            "device": {"name": "test_device"},
            "registers": {
                "coils": [{"name": "c1", "address": 0}, {"name": "c2", "address": 1}],
                "discrete_inputs": [{"name": "di1", "address": 0}],
                "holding_registers": [
                    {"name": "hr1", "address": 0},
                    {"name": "hr2", "address": 1},
                    {"name": "hr3", "address": 2},
                ],
                "input_registers": [
                    {"name": "ir1", "address": 0},
                    {"name": "ir2", "address": 1},
                ],
            }
        }

        processed = generator._preprocess_config(config)

        assert processed["stats"]["num_coils"] == 2
        assert processed["stats"]["num_discrete_inputs"] == 1
        assert processed["stats"]["num_holding_registers"] == 3
        assert processed["stats"]["num_input_registers"] == 2

    def test_default_data_type_added(self, generator):
        """Test that default data_type is added to registers."""
        config = {
            "device": {"name": "test_device"},
            "registers": {
                "holding_registers": [
                    {"name": "reg1", "address": 0},  # No data_type
                ],
                "input_registers": [
                    {"name": "reg2", "address": 0},  # No data_type
                ],
            }
        }

        processed = generator._preprocess_config(config)

        # Check that data_type was added
        assert processed["registers"]["holding_registers"][0]["data_type"] == "uint16"
        assert processed["registers"]["input_registers"][0]["data_type"] == "uint16"

    def test_default_size_added(self, generator):
        """Test that default size is added to registers."""
        config = {
            "device": {"name": "test_device"},
            "registers": {
                "holding_registers": [
                    {"name": "reg1", "address": 0},  # No size
                ],
                "input_registers": [
                    {"name": "reg2", "address": 0},  # No size
                ],
            }
        }

        processed = generator._preprocess_config(config)

        # Check that size was added
        assert processed["registers"]["holding_registers"][0]["size"] == 1
        assert processed["registers"]["input_registers"][0]["size"] == 1

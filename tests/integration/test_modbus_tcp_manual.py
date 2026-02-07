#!/usr/bin/env python3
# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.
"""
Dynamic Modbus TCP test script for STM32H563.

This script reads the register configuration from a JSON file and automatically
generates test cases for all defined registers.

Usage:
    python test_modbus_tcp_manual.py --config config/jerry_registers.json --host 169.254.4.100

Requirements:
    pip install pymodbus>=3.0
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Any

try:
    from pymodbus.client import ModbusTcpClient
    from pymodbus.exceptions import ModbusException
except ImportError:
    print("Error: pymodbus not installed. Run: pip install pymodbus")
    sys.exit(1)


# Default connection settings
DEFAULT_HOST = "192.168.1.100"
DEFAULT_PORT = 502
DEFAULT_CONFIG = "config/jerry_registers.json"


class ModbusRegisterConfig:
    """Parses and holds Modbus register configuration from JSON."""

    def __init__(self, config_path: str):
        """Load configuration from JSON file."""
        with open(config_path, 'r') as f:
            self.config = json.load(f)

        self.device = self.config.get('device', {})
        self.registers = self.config.get('registers', {})

        # Parse register types
        self.coils = self.registers.get('coils', [])
        self.discrete_inputs = self.registers.get('discrete_inputs', [])
        self.holding_registers = self.registers.get('holding_registers', [])
        self.input_registers = self.registers.get('input_registers', [])

    @property
    def device_name(self) -> str:
        return self.device.get('name', 'unknown')

    @property
    def slave_id(self) -> int:
        return self.device.get('slave_id', 1)

    def get_coil_ranges(self) -> list[tuple[int, int, str, bool]]:
        """Get contiguous coil ranges for testing.

        Returns list of (start_addr, count, description, is_writable)
        """
        if not self.coils:
            return []

        # Group by access type and find contiguous ranges
        ranges = []
        sorted_coils = sorted(self.coils, key=lambda x: x['address'])

        current_start = sorted_coils[0]['address']
        current_count = 1
        current_access = sorted_coils[0].get('access', 'read_write')
        current_group = sorted_coils[0].get('group', 'default')

        for i in range(1, len(sorted_coils)):
            coil = sorted_coils[i]
            access = coil.get('access', 'read_write')
            group = coil.get('group', 'default')

            # Check if contiguous and same access type
            if (coil['address'] == current_start + current_count and
                access == current_access and group == current_group):
                current_count += 1
            else:
                # Save current range
                is_writable = current_access == 'read_write'
                ranges.append((current_start, current_count, current_group, is_writable))
                # Start new range
                current_start = coil['address']
                current_count = 1
                current_access = access
                current_group = group

        # Don't forget the last range
        is_writable = current_access == 'read_write'
        ranges.append((current_start, current_count, current_group, is_writable))

        return ranges

    def get_discrete_input_ranges(self) -> list[tuple[int, int, str]]:
        """Get contiguous discrete input ranges for testing."""
        if not self.discrete_inputs:
            return []

        ranges = []
        sorted_inputs = sorted(self.discrete_inputs, key=lambda x: x['address'])

        current_start = sorted_inputs[0]['address']
        current_count = 1
        current_group = sorted_inputs[0].get('group', 'default')

        for i in range(1, len(sorted_inputs)):
            inp = sorted_inputs[i]
            group = inp.get('group', 'default')

            if inp['address'] == current_start + current_count and group == current_group:
                current_count += 1
            else:
                ranges.append((current_start, current_count, current_group))
                current_start = inp['address']
                current_count = 1
                current_group = group

        ranges.append((current_start, current_count, current_group))
        return ranges

    def get_holding_register_ranges(self) -> list[tuple[int, int, str, bool]]:
        """Get contiguous holding register ranges for testing.

        Returns list of (start_addr, count, description, is_writable)
        """
        if not self.holding_registers:
            return []

        ranges = []
        sorted_regs = sorted(self.holding_registers, key=lambda x: x['address'])

        current_start = sorted_regs[0]['address']
        current_size = sorted_regs[0].get('size', 1)
        current_access = sorted_regs[0].get('access', 'read_write')
        current_group = sorted_regs[0].get('group', 'default')

        for i in range(1, len(sorted_regs)):
            reg = sorted_regs[i]
            access = reg.get('access', 'read_write')
            group = reg.get('group', 'default')
            size = reg.get('size', 1)

            # Check if contiguous and same access type
            if (reg['address'] == current_start + current_size and
                access == current_access and group == current_group):
                current_size += size
            else:
                # Save current range
                is_writable = current_access == 'read_write'
                ranges.append((current_start, current_size, current_group, is_writable))
                # Start new range
                current_start = reg['address']
                current_size = size
                current_access = access
                current_group = group

        # Don't forget the last range
        is_writable = current_access == 'read_write'
        ranges.append((current_start, current_size, current_group, is_writable))

        return ranges

    def get_input_register_ranges(self) -> list[tuple[int, int, str]]:
        """Get contiguous input register ranges for testing."""
        if not self.input_registers:
            return []

        ranges = []
        sorted_regs = sorted(self.input_registers, key=lambda x: x['address'])

        current_start = sorted_regs[0]['address']
        current_size = sorted_regs[0].get('size', 1)
        current_group = sorted_regs[0].get('group', 'default')

        for i in range(1, len(sorted_regs)):
            reg = sorted_regs[i]
            group = reg.get('group', 'default')
            size = reg.get('size', 1)

            if reg['address'] == current_start + current_size and group == current_group:
                current_size += size
            else:
                ranges.append((current_start, current_size, current_group))
                current_start = reg['address']
                current_size = size
                current_group = group

        ranges.append((current_start, current_size, current_group))
        return ranges

    def get_writable_coils(self) -> list[dict]:
        """Get list of writable coils for write tests."""
        return [c for c in self.coils if c.get('access', 'read_write') == 'read_write']

    def get_writable_holding_registers(self) -> list[dict]:
        """Get list of writable holding registers for write tests."""
        return [r for r in self.holding_registers if r.get('access', 'read_write') == 'read_write']


def test_read_holding_registers(client: ModbusTcpClient, config: ModbusRegisterConfig) -> bool:
    """Test reading holding registers."""
    print("\n=== Testing Read Holding Registers (FC03) ===")

    ranges = config.get_holding_register_ranges()
    if not ranges:
        print("  No holding registers defined in config")
        return True

    all_passed = True
    for start_addr, count, group, _ in ranges:
        try:
            result = client.read_holding_registers(address=start_addr, count=count)
            if result.isError():
                print(f"  FAIL: {group} @ {start_addr} (count={count}) - Error: {result}")
                all_passed = False
            else:
                values = result.registers
                print(f"  PASS: {group} @ {start_addr} (count={count})")
                print(f"        Values: {values}")
        except ModbusException as e:
            print(f"  FAIL: {group} @ {start_addr} - Exception: {e}")
            all_passed = False
        except Exception as e:
            print(f"  FAIL: {group} @ {start_addr} - Unexpected error: {e}")
            all_passed = False

    return all_passed


def test_read_input_registers(client: ModbusTcpClient, config: ModbusRegisterConfig) -> bool:
    """Test reading input registers."""
    print("\n=== Testing Read Input Registers (FC04) ===")

    ranges = config.get_input_register_ranges()
    if not ranges:
        print("  No input registers defined in config")
        return True

    all_passed = True
    for start_addr, count, group in ranges:
        try:
            result = client.read_input_registers(address=start_addr, count=count)
            if result.isError():
                print(f"  FAIL: {group} @ {start_addr} (count={count}) - Error: {result}")
                all_passed = False
            else:
                values = result.registers
                print(f"  PASS: {group} @ {start_addr} (count={count})")
                print(f"        Values: {values}")
        except ModbusException as e:
            print(f"  FAIL: {group} @ {start_addr} - Exception: {e}")
            all_passed = False
        except Exception as e:
            print(f"  FAIL: {group} @ {start_addr} - Unexpected error: {e}")
            all_passed = False

    return all_passed


def test_read_coils(client: ModbusTcpClient, config: ModbusRegisterConfig) -> bool:
    """Test reading coils."""
    print("\n=== Testing Read Coils (FC01) ===")

    ranges = config.get_coil_ranges()
    if not ranges:
        print("  No coils defined in config")
        return True

    all_passed = True
    for start_addr, count, group, _ in ranges:
        try:
            result = client.read_coils(address=start_addr, count=count)
            if result.isError():
                print(f"  FAIL: {group} @ {start_addr} (count={count}) - Error: {result}")
                all_passed = False
            else:
                values = result.bits[:count]
                print(f"  PASS: {group} @ {start_addr} (count={count})")
                print(f"        Values: {values}")
        except ModbusException as e:
            print(f"  FAIL: {group} @ {start_addr} - Exception: {e}")
            all_passed = False
        except Exception as e:
            print(f"  FAIL: {group} @ {start_addr} - Unexpected error: {e}")
            all_passed = False

    return all_passed


def test_read_discrete_inputs(client: ModbusTcpClient, config: ModbusRegisterConfig) -> bool:
    """Test reading discrete inputs."""
    print("\n=== Testing Read Discrete Inputs (FC02) ===")

    ranges = config.get_discrete_input_ranges()
    if not ranges:
        print("  No discrete inputs defined in config")
        return True

    all_passed = True
    for start_addr, count, group in ranges:
        try:
            result = client.read_discrete_inputs(address=start_addr, count=count)
            if result.isError():
                print(f"  FAIL: {group} @ {start_addr} (count={count}) - Error: {result}")
                all_passed = False
            else:
                values = result.bits[:count]
                print(f"  PASS: {group} @ {start_addr} (count={count})")
                print(f"        Values: {values}")
        except ModbusException as e:
            print(f"  FAIL: {group} @ {start_addr} - Exception: {e}")
            all_passed = False
        except Exception as e:
            print(f"  FAIL: {group} @ {start_addr} - Unexpected error: {e}")
            all_passed = False

    return all_passed


def test_write_single_coil(client: ModbusTcpClient, config: ModbusRegisterConfig) -> bool:
    """Test writing single coil."""
    print("\n=== Testing Write Single Coil (FC05) ===")

    writable_coils = config.get_writable_coils()
    if not writable_coils:
        print("  No writable coils defined in config")
        return True

    # Test first few writable coils
    test_coils = writable_coils[:3]

    all_passed = True
    for coil in test_coils:
        addr = coil['address']
        name = coil['name']

        # Test ON
        try:
            result = client.write_coil(address=addr, value=True)
            if result.isError():
                print(f"  FAIL: {name} @ {addr} ON - Error: {result}")
                all_passed = False
            else:
                print(f"  PASS: {name} @ {addr} ON")
                verify = client.read_coils(address=addr, count=1)
                if not verify.isError():
                    print(f"        Verified: {verify.bits[0]}")
        except Exception as e:
            print(f"  FAIL: {name} @ {addr} ON - Error: {e}")
            all_passed = False

        # Test OFF
        try:
            result = client.write_coil(address=addr, value=False)
            if result.isError():
                print(f"  FAIL: {name} @ {addr} OFF - Error: {result}")
                all_passed = False
            else:
                print(f"  PASS: {name} @ {addr} OFF")
                verify = client.read_coils(address=addr, count=1)
                if not verify.isError():
                    print(f"        Verified: {verify.bits[0]}")
        except Exception as e:
            print(f"  FAIL: {name} @ {addr} OFF - Error: {e}")
            all_passed = False

    return all_passed


def test_write_single_register(client: ModbusTcpClient, config: ModbusRegisterConfig) -> bool:
    """Test writing single register."""
    print("\n=== Testing Write Single Register (FC06) ===")

    writable_regs = config.get_writable_holding_registers()
    if not writable_regs:
        print("  No writable holding registers defined in config")
        return True

    # Test first few writable registers (only size=1)
    test_regs = [r for r in writable_regs if r.get('size', 1) == 1][:3]

    all_passed = True
    for reg in test_regs:
        addr = reg['address']
        name = reg['name']
        default_val = reg.get('default_value', 0)
        min_val = reg.get('min_value', 0)
        max_val = reg.get('max_value', 65535)

        # Calculate a test value in valid range
        test_val = min(max_val, max(min_val, (min_val + max_val) // 2))

        try:
            result = client.write_register(address=addr, value=test_val)
            if result.isError():
                print(f"  FAIL: {name} @ {addr} = {test_val} - Error: {result}")
                all_passed = False
            else:
                print(f"  PASS: {name} @ {addr} = {test_val}")
                verify = client.read_holding_registers(address=addr, count=1)
                if not verify.isError():
                    print(f"        Verified: {verify.registers[0]}")
        except Exception as e:
            print(f"  FAIL: {name} @ {addr} - Error: {e}")
            all_passed = False

    return all_passed


def test_write_multiple_coils(client: ModbusTcpClient, config: ModbusRegisterConfig) -> bool:
    """Test writing multiple coils."""
    print("\n=== Testing Write Multiple Coils (FC15) ===")

    # Find a contiguous range of writable coils
    ranges = config.get_coil_ranges()
    writable_ranges = [(s, c, g) for s, c, g, w in ranges if w and c >= 4]

    if not writable_ranges:
        print("  No suitable writable coil ranges found")
        return True

    start_addr, count, group = writable_ranges[0]
    test_count = min(count, 8)

    all_passed = True

    # Test alternating pattern
    pattern = [i % 2 == 0 for i in range(test_count)]
    try:
        result = client.write_coils(address=start_addr, values=pattern)
        if result.isError():
            print(f"  FAIL: {group} @ {start_addr} alternating - Error: {result}")
            all_passed = False
        else:
            print(f"  PASS: {group} @ {start_addr} alternating pattern")
            verify = client.read_coils(address=start_addr, count=test_count)
            if not verify.isError():
                print(f"        Verified: {verify.bits[:test_count]}")
    except Exception as e:
        print(f"  FAIL: {group} @ {start_addr} - Error: {e}")
        all_passed = False

    # Test all OFF
    try:
        result = client.write_coils(address=start_addr, values=[False] * test_count)
        if result.isError():
            print(f"  FAIL: {group} @ {start_addr} all OFF - Error: {result}")
            all_passed = False
        else:
            print(f"  PASS: {group} @ {start_addr} all OFF")
            verify = client.read_coils(address=start_addr, count=test_count)
            if not verify.isError():
                print(f"        Verified: {verify.bits[:test_count]}")
    except Exception as e:
        print(f"  FAIL: {group} @ {start_addr} - Error: {e}")
        all_passed = False

    return all_passed


def test_write_multiple_registers(client: ModbusTcpClient, config: ModbusRegisterConfig) -> bool:
    """Test writing multiple registers."""
    print("\n=== Testing Write Multiple Registers (FC16) ===")

    # Find a contiguous range of writable registers
    ranges = config.get_holding_register_ranges()
    writable_ranges = [(s, c, g) for s, c, g, w in ranges if w and c >= 2]

    if not writable_ranges:
        print("  No suitable writable register ranges found")
        return True

    start_addr, count, group = writable_ranges[0]
    test_count = min(count, 4)

    all_passed = True

    # Test with incrementing values
    values = [1000 + i * 100 for i in range(test_count)]
    try:
        result = client.write_registers(address=start_addr, values=values)
        if result.isError():
            print(f"  FAIL: {group} @ {start_addr} - Error: {result}")
            all_passed = False
        else:
            print(f"  PASS: {group} @ {start_addr} values={values}")
            verify = client.read_holding_registers(address=start_addr, count=test_count)
            if not verify.isError():
                print(f"        Verified: {verify.registers}")
    except Exception as e:
        print(f"  FAIL: {group} @ {start_addr} - Error: {e}")
        all_passed = False

    return all_passed


def run_all_tests(host: str, port: int, config: ModbusRegisterConfig) -> bool:
    """Run all Modbus TCP tests."""
    unit_id = config.slave_id
    print(f"Device: {config.device_name}")
    print(f"Connecting to Modbus TCP server at {host}:{port} (unit_id={unit_id})...")

    client = ModbusTcpClient(host=host, port=port, timeout=5)
    client.slave = unit_id

    if not client.connect():
        print(f"ERROR: Failed to connect to {host}:{port}")
        return False

    print("Connected successfully!\n")

    # Print register summary
    print("=== Register Configuration Summary ===")
    print(f"  Coils: {len(config.coils)}")
    print(f"  Discrete Inputs: {len(config.discrete_inputs)}")
    print(f"  Holding Registers: {len(config.holding_registers)}")
    print(f"  Input Registers: {len(config.input_registers)}")

    try:
        results = []

        # Read tests
        results.append(("Read Holding Registers", test_read_holding_registers(client, config)))
        results.append(("Read Input Registers", test_read_input_registers(client, config)))
        results.append(("Read Coils", test_read_coils(client, config)))
        results.append(("Read Discrete Inputs", test_read_discrete_inputs(client, config)))

        # Write tests
        results.append(("Write Single Coil", test_write_single_coil(client, config)))
        results.append(("Write Single Register", test_write_single_register(client, config)))
        results.append(("Write Multiple Coils", test_write_multiple_coils(client, config)))
        results.append(("Write Multiple Registers", test_write_multiple_registers(client, config)))

        # Summary
        print("\n" + "=" * 50)
        print("TEST SUMMARY")
        print("=" * 50)

        all_passed = True
        for name, passed in results:
            status = "PASS" if passed else "FAIL"
            print(f"  {name}: {status}")
            if not passed:
                all_passed = False

        print("=" * 50)
        if all_passed:
            print("All tests PASSED!")
        else:
            print("Some tests FAILED!")

        return all_passed

    finally:
        client.close()
        print("\nConnection closed.")


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Test Modbus TCP server using register configuration from JSON"
    )
    parser.add_argument(
        "--config",
        default=DEFAULT_CONFIG,
        help=f"Path to Modbus register JSON config file (default: {DEFAULT_CONFIG})"
    )
    parser.add_argument(
        "--host",
        default=DEFAULT_HOST,
        help=f"Modbus TCP server IP address (default: {DEFAULT_HOST})"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_PORT,
        help=f"Modbus TCP server port (default: {DEFAULT_PORT})"
    )

    args = parser.parse_args()

    # Load configuration
    config_path = Path(args.config)
    if not config_path.exists():
        print(f"ERROR: Config file not found: {config_path}")
        sys.exit(1)

    print(f"Loading configuration from: {config_path}")
    config = ModbusRegisterConfig(str(config_path))

    success = run_all_tests(args.host, args.port, config)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()

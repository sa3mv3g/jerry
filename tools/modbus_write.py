#!/usr/bin/env python3
"""
Modbus Write Utility

A command-line utility to write coils and holding registers over Modbus TCP.
Supports writing single coils (FC05), multiple coils (FC15), single registers (FC06),
and multiple registers (FC16).

Usage:
    # Write single coil
    python modbus_write.py --host 169.254.4.100 coil 0 1
    python modbus_write.py --host 169.254.4.100 coil 0 on

    # Write multiple coils
    python modbus_write.py --host 169.254.4.100 coils 0 1 0 1 1 0 0 1

    # Write single holding register
    python modbus_write.py --host 169.254.4.100 register 0 5000

    # Write multiple holding registers
    python modbus_write.py --host 169.254.4.100 registers 0 1000 2000 3000
"""

from __future__ import annotations

import argparse
import sys
from datetime import datetime

try:
    from pymodbus.client import ModbusTcpClient
    from pymodbus.exceptions import ModbusException
except ImportError:
    print("Error: pymodbus is required. Install with: pip install pymodbus")
    sys.exit(1)


# Default configuration matching jerry_device register map
DEFAULT_HOST = "169.254.4.100"
DEFAULT_PORT = 502
DEFAULT_UNIT_ID = 1

# Coil addresses (from jerry_registers.json)
# Digital outputs: 0-15 (read_write)
# Digital inputs: 16-23 (read_only, mirror of discrete inputs)
# PWM enables: 24-27 (read_write)
COIL_DO_START = 0
COIL_DO_COUNT = 16
COIL_PWM_START = 24
COIL_PWM_COUNT = 4

# Holding register addresses (from jerry_registers.json)
# PWM duty cycles and frequencies: 0-11
# RTC registers: 210-215
HR_PWM_START = 0
HR_PWM_COUNT = 12
HR_RTC_START = 210
HR_RTC_COUNT = 6


def parse_bool_value(value: str) -> bool:
    """Parse a boolean value from string."""
    value_lower = value.lower()
    if value_lower in ("1", "true", "on", "yes", "high"):
        return True
    if value_lower in ("0", "false", "off", "no", "low"):
        return False
    raise ValueError(f"Invalid boolean value: {value}")


def parse_register_value(value: str) -> int:
    """Parse a register value from string (supports hex with 0x prefix)."""
    if value.lower().startswith("0x"):
        return int(value, 16)
    return int(value)


def write_single_coil(
    client: ModbusTcpClient, address: int, value: bool, verbose: bool
) -> int:
    """Write a single coil using FC05."""
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

    if verbose:
        print(f"[{timestamp}] Writing coil at address {address}: {value}")

    result = client.write_coil(address=address, value=value)

    if result.isError():
        print(f"[{timestamp}] Error: {result}")
        return 1

    state = "ON (1)" if value else "OFF (0)"
    print(f"[{timestamp}] Successfully wrote coil {address} = {state}")

    # Verify by reading back
    if verbose:
        verify = client.read_coils(address=address, count=1)
        if not verify.isError():
            actual = verify.bits[0]
            actual_state = "ON (1)" if actual else "OFF (0)"
            print(f"[{timestamp}] Verified: coil {address} = {actual_state}")
            if actual != value:
                print(f"[{timestamp}] WARNING: Value mismatch!")
                return 1

    return 0


def write_multiple_coils(
    client: ModbusTcpClient, address: int, values: list[bool], verbose: bool
) -> int:
    """Write multiple coils using FC15."""
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

    if verbose:
        values_str = ", ".join("1" if v else "0" for v in values)
        print(
            f"[{timestamp}] Writing {len(values)} coils starting at "
            f"address {address}: [{values_str}]"
        )

    result = client.write_coils(address=address, values=values)

    if result.isError():
        print(f"[{timestamp}] Error: {result}")
        return 1

    print(
        f"[{timestamp}] Successfully wrote {len(values)} coils "
        f"starting at address {address}"
    )

    # Show what was written
    for i, val in enumerate(values):
        state = "ON (1)" if val else "OFF (0)"
        print(f"  Coil {address + i}: {state}")

    # Verify by reading back
    if verbose:
        verify = client.read_coils(address=address, count=len(values))
        if not verify.isError():
            print(f"[{timestamp}] Verification:")
            mismatch = False
            for i, val in enumerate(values):
                actual = verify.bits[i]
                actual_state = "ON (1)" if actual else "OFF (0)"
                status = "OK" if actual == val else "MISMATCH"
                print(f"  Coil {address + i}: {actual_state} [{status}]")
                if actual != val:
                    mismatch = True
            if mismatch:
                print(f"[{timestamp}] WARNING: Some values did not match!")
                return 1

    return 0


def write_single_register(
    client: ModbusTcpClient, address: int, value: int, verbose: bool
) -> int:
    """Write a single holding register using FC06."""
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

    if verbose:
        print(
            f"[{timestamp}] Writing holding register at address {address}: "
            f"{value} (0x{value:04X})"
        )

    result = client.write_register(address=address, value=value)

    if result.isError():
        print(f"[{timestamp}] Error: {result}")
        return 1

    print(
        f"[{timestamp}] Successfully wrote register {address} = "
        f"{value} (0x{value:04X})"
    )

    # Verify by reading back
    if verbose:
        verify = client.read_holding_registers(address=address, count=1)
        if not verify.isError():
            actual = verify.registers[0]
            print(
                f"[{timestamp}] Verified: register {address} = "
                f"{actual} (0x{actual:04X})"
            )
            if actual != value:
                print(f"[{timestamp}] WARNING: Value mismatch!")
                return 1

    return 0


def write_multiple_registers(
    client: ModbusTcpClient, address: int, values: list[int], verbose: bool
) -> int:
    """Write multiple holding registers using FC16."""
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

    if verbose:
        values_str = ", ".join(f"{v} (0x{v:04X})" for v in values)
        print(
            f"[{timestamp}] Writing {len(values)} registers starting at "
            f"address {address}: [{values_str}]"
        )

    result = client.write_registers(address=address, values=values)

    if result.isError():
        print(f"[{timestamp}] Error: {result}")
        return 1

    print(
        f"[{timestamp}] Successfully wrote {len(values)} registers "
        f"starting at address {address}"
    )

    # Show what was written
    for i, val in enumerate(values):
        print(f"  Register {address + i}: {val} (0x{val:04X})")

    # Verify by reading back
    if verbose:
        verify = client.read_holding_registers(address=address, count=len(values))
        if not verify.isError():
            print(f"[{timestamp}] Verification:")
            mismatch = False
            for i, val in enumerate(values):
                actual = verify.registers[i]
                status = "OK" if actual == val else "MISMATCH"
                print(f"  Register {address + i}: {actual} (0x{actual:04X}) [{status}]")
                if actual != val:
                    mismatch = True
            if mismatch:
                print(f"[{timestamp}] WARNING: Some values did not match!")
                return 1

    return 0


def cmd_coil(args: argparse.Namespace, client: ModbusTcpClient) -> int:
    """Handle single coil write command."""
    try:
        value = parse_bool_value(args.value)
    except ValueError as e:
        print(f"Error: {e}")
        return 1

    return write_single_coil(client, args.address, value, args.verbose)


def cmd_coils(args: argparse.Namespace, client: ModbusTcpClient) -> int:
    """Handle multiple coils write command."""
    try:
        values = [parse_bool_value(v) for v in args.values]
    except ValueError as e:
        print(f"Error: {e}")
        return 1

    return write_multiple_coils(client, args.address, values, args.verbose)


def cmd_register(args: argparse.Namespace, client: ModbusTcpClient) -> int:
    """Handle single register write command."""
    try:
        value = parse_register_value(args.value)
        if not 0 <= value <= 65535:
            print(f"Error: Register value must be 0-65535, got {value}")
            return 1
    except ValueError as e:
        print(f"Error: Invalid register value: {e}")
        return 1

    return write_single_register(client, args.address, value, args.verbose)


def cmd_registers(args: argparse.Namespace, client: ModbusTcpClient) -> int:
    """Handle multiple registers write command."""
    try:
        values = [parse_register_value(v) for v in args.values]
        for i, val in enumerate(values):
            if not 0 <= val <= 65535:
                print(f"Error: Register value must be 0-65535, got {val} at index {i}")
                return 1
    except ValueError as e:
        print(f"Error: Invalid register value: {e}")
        return 1

    return write_multiple_registers(client, args.address, values, args.verbose)


def cmd_do(args: argparse.Namespace, client: ModbusTcpClient) -> int:
    """Handle digital output write command (convenience wrapper)."""
    if not 0 <= args.output < COIL_DO_COUNT:
        print(f"Error: Digital output must be 0-{COIL_DO_COUNT - 1}, got {args.output}")
        return 1

    try:
        value = parse_bool_value(args.value)
    except ValueError as e:
        print(f"Error: {e}")
        return 1

    print(f"Writing digital output DO{args.output}...")
    return write_single_coil(client, COIL_DO_START + args.output, value, args.verbose)


def cmd_pwm_enable(args: argparse.Namespace, client: ModbusTcpClient) -> int:
    """Handle PWM enable write command (convenience wrapper)."""
    if not 0 <= args.channel < COIL_PWM_COUNT:
        print(f"Error: PWM channel must be 0-{COIL_PWM_COUNT - 1}, got {args.channel}")
        return 1

    try:
        value = parse_bool_value(args.value)
    except ValueError as e:
        print(f"Error: {e}")
        return 1

    print(f"Writing PWM{args.channel} enable...")
    return write_single_coil(client, COIL_PWM_START + args.channel, value, args.verbose)


def cmd_pwm_duty(args: argparse.Namespace, client: ModbusTcpClient) -> int:
    """Handle PWM duty cycle write command (convenience wrapper)."""
    if not 0 <= args.channel < COIL_PWM_COUNT:
        print(f"Error: PWM channel must be 0-{COIL_PWM_COUNT - 1}, got {args.channel}")
        return 1

    # Duty cycle is 0-10000 (0.00% - 100.00%)
    duty = int(args.duty * 100)  # Convert percentage to 0-10000 scale
    if not 0 <= duty <= 10000:
        print(f"Error: Duty cycle must be 0-100%, got {args.duty}%")
        return 1

    # Each PWM channel has duty at offset 0, 3, 6, 9 from HR_PWM_START
    address = HR_PWM_START + (args.channel * 3)

    print(f"Writing PWM{args.channel} duty cycle = {args.duty}% ({duty}/10000)...")
    return write_single_register(client, address, duty, args.verbose)


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Write coils and holding registers over Modbus TCP",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Write single coil (digital output 0 ON)
  %(prog)s --host 169.254.4.100 coil 0 on

  # Write multiple coils (DO0-DO3)
  %(prog)s --host 169.254.4.100 coils 0 1 0 1 1

  # Write single holding register
  %(prog)s --host 169.254.4.100 register 0 5000

  # Write multiple holding registers
  %(prog)s --host 169.254.4.100 registers 0 1000 2000 3000

  # Convenience commands for jerry_device:
  %(prog)s --host 169.254.4.100 do 0 on       # Set digital output 0
  %(prog)s --host 169.254.4.100 pwm-enable 0 on  # Enable PWM channel 0
  %(prog)s --host 169.254.4.100 pwm-duty 0 50.5  # Set PWM0 duty to 50.5%%

Register Map (jerry_device):
  Coils:
    0-15   : Digital outputs (DO0-DO15) - read/write
    16-23  : Digital inputs (DI0-DI7) - read only
    24-27  : PWM enables (PWM0-PWM3) - read/write

  Holding Registers:
    0      : PWM0 duty cycle (0-10000 = 0.00-100.00%%)
    1-2    : PWM0 frequency (uint32, Hz)
    3      : PWM1 duty cycle
    4-5    : PWM1 frequency
    6      : PWM2 duty cycle
    7-8    : PWM2 frequency
    9      : PWM3 duty cycle
    10-11  : PWM3 frequency
    210-215: RTC (year, month, day, hour, minute, second)
        """,
    )

    # Global options
    parser.add_argument(
        "--host",
        "-H",
        default=DEFAULT_HOST,
        help=f"Modbus TCP host address (default: {DEFAULT_HOST})",
    )
    parser.add_argument(
        "--port",
        "-p",
        type=int,
        default=DEFAULT_PORT,
        help=f"Modbus TCP port (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--unit-id",
        "-u",
        type=int,
        default=DEFAULT_UNIT_ID,
        help=f"Modbus unit/slave ID (default: {DEFAULT_UNIT_ID})",
    )
    parser.add_argument(
        "--timeout",
        "-t",
        type=float,
        default=3.0,
        help="Connection timeout in seconds (default: 3.0)",
    )
    parser.add_argument(
        "--source-ip",
        "-s",
        default=None,
        help="Source IP address to bind to (for multi-interface systems)",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Verbose output with verification reads",
    )

    # Subcommands
    subparsers = parser.add_subparsers(dest="command", help="Command to execute")

    # coil: Write single coil
    coil_parser = subparsers.add_parser(
        "coil", help="Write single coil (FC05)"
    )
    coil_parser.add_argument(
        "address", type=int, help="Coil address (0-65535)"
    )
    coil_parser.add_argument(
        "value", help="Value to write (0/1, on/off, true/false)"
    )
    coil_parser.set_defaults(func=cmd_coil)

    # coils: Write multiple coils
    coils_parser = subparsers.add_parser(
        "coils", help="Write multiple coils (FC15)"
    )
    coils_parser.add_argument(
        "address", type=int, help="Starting coil address"
    )
    coils_parser.add_argument(
        "values", nargs="+", help="Values to write (0/1, on/off, true/false)"
    )
    coils_parser.set_defaults(func=cmd_coils)

    # register: Write single register
    register_parser = subparsers.add_parser(
        "register", help="Write single holding register (FC06)"
    )
    register_parser.add_argument(
        "address", type=int, help="Register address (0-65535)"
    )
    register_parser.add_argument(
        "value", help="Value to write (0-65535, or 0xHHHH for hex)"
    )
    register_parser.set_defaults(func=cmd_register)

    # registers: Write multiple registers
    registers_parser = subparsers.add_parser(
        "registers", help="Write multiple holding registers (FC16)"
    )
    registers_parser.add_argument(
        "address", type=int, help="Starting register address"
    )
    registers_parser.add_argument(
        "values", nargs="+", help="Values to write (0-65535 each, or 0xHHHH for hex)"
    )
    registers_parser.set_defaults(func=cmd_registers)

    # do: Convenience command for digital outputs
    do_parser = subparsers.add_parser(
        "do", help="Write digital output (DO0-DO15)"
    )
    do_parser.add_argument(
        "output", type=int, help="Digital output number (0-15)"
    )
    do_parser.add_argument(
        "value", help="Value to write (0/1, on/off, true/false)"
    )
    do_parser.set_defaults(func=cmd_do)

    # pwm-enable: Convenience command for PWM enable
    pwm_en_parser = subparsers.add_parser(
        "pwm-enable", help="Enable/disable PWM channel (PWM0-PWM3)"
    )
    pwm_en_parser.add_argument(
        "channel", type=int, help="PWM channel number (0-3)"
    )
    pwm_en_parser.add_argument(
        "value", help="Value to write (0/1, on/off, true/false)"
    )
    pwm_en_parser.set_defaults(func=cmd_pwm_enable)

    # pwm-duty: Convenience command for PWM duty cycle
    pwm_duty_parser = subparsers.add_parser(
        "pwm-duty", help="Set PWM duty cycle (PWM0-PWM3)"
    )
    pwm_duty_parser.add_argument(
        "channel", type=int, help="PWM channel number (0-3)"
    )
    pwm_duty_parser.add_argument(
        "duty", type=float, help="Duty cycle in percent (0.0-100.0)"
    )
    pwm_duty_parser.set_defaults(func=cmd_pwm_duty)

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return 1

    print(f"Connecting to {args.host}:{args.port} (Unit ID: {args.unit_id})...")
    if args.source_ip:
        print(f"Using source IP: {args.source_ip}")

    # Create client with source address if specified
    if args.source_ip:
        client = ModbusTcpClient(
            host=args.host,
            port=args.port,
            timeout=args.timeout,
            source_address=(args.source_ip, 0),
        )
    else:
        client = ModbusTcpClient(
            host=args.host,
            port=args.port,
            timeout=args.timeout,
        )

    try:
        if not client.connect():
            print(f"Error: Failed to connect to {args.host}:{args.port}")
            return 1

        print("Connected successfully!")

        # Set the unit ID on the client
        client.unit_id = args.unit_id

        return args.func(args, client)

    except ModbusException as e:
        print(f"Modbus error: {e}")
        return 1
    except Exception as e:
        print(f"Error: {e}")
        return 1
    finally:
        client.close()
        print("Connection closed.")


if __name__ == "__main__":
    sys.exit(main())

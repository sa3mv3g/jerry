#!/usr/bin/env python3
"""
Modbus Digital Input Monitor

A command-line utility to monitor GPIO digital inputs (DI0-DI7) over Modbus TCP.
Supports reading via both FC02 (Read Discrete Inputs) and FC01 (Read Coils).

Usage:
    python modbus_di_monitor.py --host 169.254.4.100 --port 502
    python modbus_di_monitor.py --host 169.254.4.100 --interval 0.5
    python modbus_di_monitor.py --host 169.254.4.100 --coils  # Read via coils instead
"""

from __future__ import annotations

import argparse
import sys
import time
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
DEFAULT_INTERVAL = 1.0

# Digital Input addresses
# Discrete Inputs (FC02): DI0-DI7 at addresses 0-7
DI_START_ADDRESS = 0
DI_COUNT = 8

# Coils (FC01): Digital inputs are mapped at addresses 16-23
# (after 16 digital outputs at addresses 0-15)
COIL_DI_START_ADDRESS = 16
COIL_DI_COUNT = 8


def format_di_state(bits: list[bool], count: int) -> str:
    """Format digital input states as a readable string."""
    states = []
    for i in range(count):
        state = "ON " if bits[i] else "OFF"
        states.append(f"DI{i}:{state}")
    return " | ".join(states)


def format_di_binary(bits: list[bool], count: int) -> str:
    """Format digital input states as binary representation."""
    binary = "".join("1" if bits[i] else "0" for i in range(count))
    return f"0b{binary} (0x{int(binary, 2):02X})"


def monitor_discrete_inputs(
    client: ModbusTcpClient, interval: float, verbose: bool
) -> None:
    """Monitor digital inputs using FC02 (Read Discrete Inputs)."""
    print(f"\n{'='*70}")
    print("Monitoring Digital Inputs via FC02 (Read Discrete Inputs)")
    print(f"Address range: {DI_START_ADDRESS} - {DI_START_ADDRESS + DI_COUNT - 1}")
    print(f"Polling interval: {interval}s")
    print(f"{'='*70}")
    print("Press Ctrl+C to stop\n")

    prev_bits: list[bool] | None = None

    try:
        while True:
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

            result = client.read_discrete_inputs(
                address=DI_START_ADDRESS, count=DI_COUNT
            )

            if result.isError():
                print(f"[{timestamp}] Error: {result}")
            else:
                bits = result.bits[:DI_COUNT]

                # Check for changes
                changed = prev_bits is not None and bits != prev_bits

                if verbose or changed or prev_bits is None:
                    status = format_di_state(bits, DI_COUNT)
                    binary = format_di_binary(bits, DI_COUNT)

                    if changed and prev_bits is not None:
                        print(f"[{timestamp}] *CHANGED* {status}")
                        print(f"           Binary: {binary}")
                        # Show which inputs changed
                        for i in range(DI_COUNT):
                            if bits[i] != prev_bits[i]:
                                old_state = "ON" if prev_bits[i] else "OFF"
                                new_state = "ON" if bits[i] else "OFF"
                                print(f"           DI{i}: {old_state} -> {new_state}")
                    else:
                        print(f"[{timestamp}] {status}")
                        if verbose:
                            print(f"           Binary: {binary}")

                prev_bits = bits

            time.sleep(interval)

    except KeyboardInterrupt:
        print("\n\nMonitoring stopped.")


def monitor_coils(
    client: ModbusTcpClient, interval: float, verbose: bool
) -> None:
    """Monitor digital inputs using FC01 (Read Coils)."""
    print(f"\n{'='*70}")
    print("Monitoring Digital Inputs via FC01 (Read Coils)")
    print(
        f"Address range: {COIL_DI_START_ADDRESS} - "
        f"{COIL_DI_START_ADDRESS + COIL_DI_COUNT - 1}"
    )
    print(f"Polling interval: {interval}s")
    print(f"{'='*70}")
    print("Press Ctrl+C to stop\n")

    prev_bits: list[bool] | None = None

    try:
        while True:
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

            result = client.read_coils(
                address=COIL_DI_START_ADDRESS, count=COIL_DI_COUNT
            )

            if result.isError():
                print(f"[{timestamp}] Error: {result}")
            else:
                bits = result.bits[:COIL_DI_COUNT]

                # Check for changes
                changed = prev_bits is not None and bits != prev_bits

                if verbose or changed or prev_bits is None:
                    status = format_di_state(bits, COIL_DI_COUNT)
                    binary = format_di_binary(bits, COIL_DI_COUNT)

                    if changed and prev_bits is not None:
                        print(f"[{timestamp}] *CHANGED* {status}")
                        print(f"           Binary: {binary}")
                        for i in range(COIL_DI_COUNT):
                            if bits[i] != prev_bits[i]:
                                old_state = "ON" if prev_bits[i] else "OFF"
                                new_state = "ON" if bits[i] else "OFF"
                                print(f"           DI{i}: {old_state} -> {new_state}")
                    else:
                        print(f"[{timestamp}] {status}")
                        if verbose:
                            print(f"           Binary: {binary}")

                prev_bits = bits

            time.sleep(interval)

    except KeyboardInterrupt:
        print("\n\nMonitoring stopped.")


def read_once(client: ModbusTcpClient, use_coils: bool) -> int:
    """Read digital inputs once and exit."""
    print(f"\n{'='*70}")

    if use_coils:
        print("Reading Digital Inputs via FC01 (Read Coils)")
        result = client.read_coils(
            address=COIL_DI_START_ADDRESS, count=COIL_DI_COUNT
        )
        count = COIL_DI_COUNT
    else:
        print("Reading Digital Inputs via FC02 (Read Discrete Inputs)")
        result = client.read_discrete_inputs(
            address=DI_START_ADDRESS, count=DI_COUNT
        )
        count = DI_COUNT

    print(f"{'='*70}\n")

    if result.isError():
        print(f"Error: {result}")
        return 1

    bits = result.bits[:count]
    print(format_di_state(bits, count))
    print(f"\nBinary: {format_di_binary(bits, count)}")

    # Detailed view
    print("\nDetailed State:")
    for i in range(count):
        state = "HIGH (ON)" if bits[i] else "LOW (OFF)"
        print(f"  DI{i}: {state}")

    return 0


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Monitor GPIO digital inputs (DI0-DI7) over Modbus TCP",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --host 169.254.4.100
  %(prog)s --host 169.254.4.100 --interval 0.5
  %(prog)s --host 169.254.4.100 --coils
  %(prog)s --host 169.254.4.100 --once
  %(prog)s --host 169.254.4.100 --verbose
  %(prog)s --host 169.254.4.100 --source-ip 169.254.4.50
        """,
    )

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
        "--interval",
        "-i",
        type=float,
        default=DEFAULT_INTERVAL,
        help=f"Polling interval in seconds (default: {DEFAULT_INTERVAL})",
    )
    parser.add_argument(
        "--coils",
        "-c",
        action="store_true",
        help="Read via FC01 (Read Coils) instead of FC02 (Read Discrete Inputs)",
    )
    parser.add_argument(
        "--once",
        "-1",
        action="store_true",
        help="Read once and exit (no continuous monitoring)",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Show all readings, not just changes",
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

    args = parser.parse_args()

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

        if args.once:
            return read_once(client, args.coils)

        if args.coils:
            monitor_coils(client, args.interval, args.verbose)
        else:
            monitor_discrete_inputs(client, args.interval, args.verbose)

        return 0

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

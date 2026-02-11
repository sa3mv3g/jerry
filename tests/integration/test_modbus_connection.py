#!/usr/bin/env python3
"""
Modbus TCP Connection Test with Source IP Binding

This script tests Modbus TCP connectivity while allowing explicit binding to a
specific network interface. This is useful when:
- Multiple network interfaces exist (e.g., VPN + physical adapter)
- The default routing sends traffic through the wrong interface
- You need to test connectivity through a specific adapter

Usage:
    # Basic test (uses default routing)
    python test_modbus_connection.py --host 169.254.4.100

    # Bind to specific source IP (bypasses VPN/routing issues)
    python test_modbus_connection.py --host 169.254.4.100 --source-ip 169.254.4.50

    # Test with custom port and unit ID
    python test_modbus_connection.py --host 169.254.4.100 --port 502 --unit-id 1 --source-ip 169.254.4.50

Copyright (c) 2026
"""

import argparse
import socket
import sys
import time
from typing import Optional

from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ModbusException


def test_tcp_connection(
    host: str,
    port: int,
    source_ip: Optional[str] = None,
    timeout: float = 5.0
) -> bool:
    """
    Test raw TCP connection to the target.

    Args:
        host: Target IP address
        port: Target port number
        source_ip: Optional source IP to bind to
        timeout: Connection timeout in seconds

    Returns:
        True if connection successful, False otherwise
    """
    print(f"\n{'='*60}")
    print("TCP Connection Test")
    print(f"{'='*60}")
    print(f"  Target:     {host}:{port}")
    print(f"  Source IP:  {source_ip or 'auto'}")
    print(f"  Timeout:    {timeout}s")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)

        if source_ip:
            # Bind to specific source IP (port 0 = auto-assign)
            sock.bind((source_ip, 0))
            print(f"  Bound to:   {source_ip}")

        start_time = time.perf_counter()
        sock.connect((host, port))
        elapsed = (time.perf_counter() - start_time) * 1000

        local_addr = sock.getsockname()
        print(f"\n  ✓ TCP Connected!")
        print(f"    Local endpoint:  {local_addr[0]}:{local_addr[1]}")
        print(f"    Connect time:    {elapsed:.2f} ms")

        sock.close()
        return True

    except socket.timeout:
        print(f"\n  ✗ Connection timed out after {timeout}s")
        return False
    except OSError as e:
        print(f"\n  ✗ Connection failed: {e}")
        return False
    finally:
        print(f"{'='*60}")


def test_modbus_connection(
    host: str,
    port: int,
    unit_id: int,
    source_ip: Optional[str] = None,
    timeout: float = 5.0
) -> bool:
    """
    Test Modbus TCP connection and perform basic read operation.

    Args:
        host: Target IP address
        port: Target port number
        unit_id: Modbus unit/slave ID
        source_ip: Optional source IP to bind to
        timeout: Connection timeout in seconds

    Returns:
        True if Modbus communication successful, False otherwise
    """
    print(f"\n{'='*60}")
    print("Modbus TCP Connection Test")
    print(f"{'='*60}")
    print(f"  Target:     {host}:{port}")
    print(f"  Unit ID:    {unit_id}")
    print(f"  Source IP:  {source_ip or 'auto'}")
    print(f"  Timeout:    {timeout}s")

    # Create client with source address if specified
    if source_ip:
        client = ModbusTcpClient(
            host=host,
            port=port,
            timeout=timeout,
            source_address=(source_ip, 0)
        )
    else:
        client = ModbusTcpClient(
            host=host,
            port=port,
            timeout=timeout
        )

    client.slave = unit_id

    try:
        start_time = time.perf_counter()
        if not client.connect():
            print(f"\n  ✗ Modbus connection failed")
            return False

        connect_time = (time.perf_counter() - start_time) * 1000
        print(f"\n  ✓ Modbus Connected!")
        print(f"    Connect time: {connect_time:.2f} ms")

        # Test read holding registers (FC03)
        print(f"\n  Testing Read Holding Registers (FC03)...")
        start_time = time.perf_counter()
        result = client.read_holding_registers(address=0, count=1)
        read_time = (time.perf_counter() - start_time) * 1000

        if result.isError():
            print(f"    ✗ Read failed: {result}")
            return False

        print(f"    ✓ Read successful!")
        print(f"      Register[0] = {result.registers[0]}")
        print(f"      Response time: {read_time:.2f} ms")

        # Test read coils (FC01)
        print(f"\n  Testing Read Coils (FC01)...")
        start_time = time.perf_counter()
        result = client.read_coils(address=0, count=8)
        read_time = (time.perf_counter() - start_time) * 1000

        if result.isError():
            print(f"    ✗ Read failed: {result}")
            return False

        print(f"    ✓ Read successful!")
        print(f"      Coils[0:8] = {result.bits[:8]}")
        print(f"      Response time: {read_time:.2f} ms")

        # Test read input registers (FC04)
        print(f"\n  Testing Read Input Registers (FC04)...")
        start_time = time.perf_counter()
        result = client.read_input_registers(address=0, count=4)
        read_time = (time.perf_counter() - start_time) * 1000

        if result.isError():
            print(f"    ✗ Read failed: {result}")
            return False

        print(f"    ✓ Read successful!")
        print(f"      Input Registers = {result.registers}")
        print(f"      Response time: {read_time:.2f} ms")

        return True

    except ModbusException as e:
        print(f"\n  ✗ Modbus error: {e}")
        return False
    except Exception as e:
        print(f"\n  ✗ Unexpected error: {e}")
        return False
    finally:
        client.close()
        print(f"{'='*60}")


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Modbus TCP Connection Test with Source IP Binding",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic test (uses default routing)
  python test_modbus_connection.py --host 169.254.4.100

  # Bind to specific source IP (bypasses VPN/routing issues)
  python test_modbus_connection.py --host 169.254.4.100 --source-ip 169.254.4.50

  # Test only TCP connection (no Modbus)
  python test_modbus_connection.py --host 169.254.4.100 --tcp-only

  # Full test with all options
  python test_modbus_connection.py --host 169.254.4.100 --port 502 --unit-id 1 \\
                                   --source-ip 169.254.4.50 --timeout 10
"""
    )
    parser.add_argument(
        "--host",
        required=True,
        help="Modbus TCP server IP address"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=502,
        help="Modbus TCP server port (default: 502)"
    )
    parser.add_argument(
        "--unit-id",
        type=int,
        default=1,
        help="Modbus unit/slave ID (default: 1)"
    )
    parser.add_argument(
        "--source-ip",
        default=None,
        help="Source IP address to bind to (for multi-interface systems)"
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=5.0,
        help="Connection timeout in seconds (default: 5.0)"
    )
    parser.add_argument(
        "--tcp-only",
        action="store_true",
        help="Test only TCP connection, skip Modbus test"
    )

    args = parser.parse_args()

    print("=" * 60)
    print("Modbus TCP Connection Test")
    print("=" * 60)
    print(f"Target: {args.host}:{args.port}")
    if args.source_ip:
        print(f"Source IP: {args.source_ip}")
    print("=" * 60)

    # Test TCP connection first
    tcp_ok = test_tcp_connection(
        args.host,
        args.port,
        args.source_ip,
        args.timeout
    )

    if not tcp_ok:
        print("\n✗ TCP connection failed - cannot proceed with Modbus test")
        sys.exit(1)

    if args.tcp_only:
        print("\n✓ TCP connection test passed (--tcp-only specified)")
        sys.exit(0)

    # Test Modbus connection
    modbus_ok = test_modbus_connection(
        args.host,
        args.port,
        args.unit_id,
        args.source_ip,
        args.timeout
    )

    if modbus_ok:
        print("\n" + "=" * 60)
        print("✓ All tests passed!")
        print("=" * 60)
        sys.exit(0)
    else:
        print("\n" + "=" * 60)
        print("✗ Modbus test failed")
        print("=" * 60)
        sys.exit(1)


if __name__ == "__main__":
    main()

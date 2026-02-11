#!/usr/bin/env python3
"""
Modbus TCP Performance Test

This script measures the performance of Modbus TCP communication:
- Request/response latency (round-trip time)
- Throughput (requests per second)
- Sustained load testing
- Burst testing

Usage:
    python test_modbus_performance.py --host 192.168.1.100 --port 502

    # With source IP binding (for VPN/multi-interface systems):
    python test_modbus_performance.py --host 169.254.4.100 --source-ip 169.254.4.50

Copyright (c) 2026
"""

import argparse
import statistics
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional

from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ModbusException

# Default configuration
DEFAULT_HOST = "192.168.1.100"
DEFAULT_PORT = 502
DEFAULT_UNIT_ID = 1


@dataclass
class PerformanceResult:
    """Performance test result."""
    test_name: str
    total_requests: int
    successful_requests: int
    failed_requests: int
    total_time_sec: float
    latencies_ms: list[float]

    @property
    def success_rate(self) -> float:
        """Calculate success rate percentage."""
        if self.total_requests == 0:
            return 0.0
        return (self.successful_requests / self.total_requests) * 100

    @property
    def requests_per_second(self) -> float:
        """Calculate requests per second."""
        if self.total_time_sec == 0:
            return 0.0
        return self.successful_requests / self.total_time_sec

    @property
    def min_latency_ms(self) -> float:
        """Minimum latency in milliseconds."""
        return min(self.latencies_ms) if self.latencies_ms else 0.0

    @property
    def max_latency_ms(self) -> float:
        """Maximum latency in milliseconds."""
        return max(self.latencies_ms) if self.latencies_ms else 0.0

    @property
    def avg_latency_ms(self) -> float:
        """Average latency in milliseconds."""
        return statistics.mean(self.latencies_ms) if self.latencies_ms else 0.0

    @property
    def median_latency_ms(self) -> float:
        """Median latency in milliseconds."""
        return statistics.median(self.latencies_ms) if self.latencies_ms else 0.0

    @property
    def stddev_latency_ms(self) -> float:
        """Standard deviation of latency in milliseconds."""
        if len(self.latencies_ms) < 2:
            return 0.0
        return statistics.stdev(self.latencies_ms)

    @property
    def p95_latency_ms(self) -> float:
        """95th percentile latency in milliseconds."""
        if not self.latencies_ms:
            return 0.0
        sorted_latencies = sorted(self.latencies_ms)
        idx = int(len(sorted_latencies) * 0.95)
        return sorted_latencies[min(idx, len(sorted_latencies) - 1)]

    @property
    def p99_latency_ms(self) -> float:
        """99th percentile latency in milliseconds."""
        if not self.latencies_ms:
            return 0.0
        sorted_latencies = sorted(self.latencies_ms)
        idx = int(len(sorted_latencies) * 0.99)
        return sorted_latencies[min(idx, len(sorted_latencies) - 1)]

    def print_report(self) -> None:
        """Print formatted performance report."""
        print(f"\n{'=' * 60}")
        print(f"Performance Test: {self.test_name}")
        print(f"{'=' * 60}")
        print(f"  Total Requests:      {self.total_requests}")
        print(f"  Successful:          {self.successful_requests}")
        print(f"  Failed:              {self.failed_requests}")
        print(f"  Success Rate:        {self.success_rate:.2f}%")
        print(f"  Total Time:          {self.total_time_sec:.3f} sec")
        print(f"  Throughput:          {self.requests_per_second:.2f} req/sec")
        print(f"\n  Latency Statistics (ms):")
        print(f"    Min:               {self.min_latency_ms:.3f}")
        print(f"    Max:               {self.max_latency_ms:.3f}")
        print(f"    Average:           {self.avg_latency_ms:.3f}")
        print(f"    Median:            {self.median_latency_ms:.3f}")
        print(f"    Std Dev:           {self.stddev_latency_ms:.3f}")
        print(f"    95th Percentile:   {self.p95_latency_ms:.3f}")
        print(f"    99th Percentile:   {self.p99_latency_ms:.3f}")
        print(f"{'=' * 60}")


def run_performance_test(
    client: ModbusTcpClient,
    test_name: str,
    operation: Callable[[], bool],
    num_requests: int,
    delay_between_requests_ms: float = 0
) -> PerformanceResult:
    """
    Run a performance test with the given operation.

    Args:
        client: Connected Modbus client
        test_name: Name of the test
        operation: Function that performs one Modbus operation, returns True on success
        num_requests: Number of requests to perform
        delay_between_requests_ms: Delay between requests in milliseconds

    Returns:
        PerformanceResult with test statistics
    """
    latencies = []
    successful = 0
    failed = 0

    print(f"\nRunning: {test_name} ({num_requests} requests)...")

    start_time = time.perf_counter()

    for i in range(num_requests):
        req_start = time.perf_counter()
        try:
            if operation():
                successful += 1
                req_end = time.perf_counter()
                latencies.append((req_end - req_start) * 1000)  # Convert to ms
            else:
                failed += 1
        except Exception as e:
            failed += 1
            if i == 0:
                print(f"  First request failed: {e}")

        if delay_between_requests_ms > 0:
            time.sleep(delay_between_requests_ms / 1000)

        # Progress indicator
        if (i + 1) % 100 == 0:
            print(f"  Progress: {i + 1}/{num_requests}")

    end_time = time.perf_counter()
    total_time = end_time - start_time

    return PerformanceResult(
        test_name=test_name,
        total_requests=num_requests,
        successful_requests=successful,
        failed_requests=failed,
        total_time_sec=total_time,
        latencies_ms=latencies
    )


def test_read_single_register(client: ModbusTcpClient, num_requests: int) -> PerformanceResult:
    """Test reading a single holding register."""
    def operation() -> bool:
        result = client.read_holding_registers(address=0, count=1)
        return not result.isError()

    return run_performance_test(
        client,
        "Read Single Holding Register (FC03)",
        operation,
        num_requests
    )


def test_read_multiple_registers(client: ModbusTcpClient, num_requests: int, count: int = 10) -> PerformanceResult:
    """Test reading multiple holding registers.

    Note: Uses PWM registers at addresses 0-11 which are contiguous.
    """
    # Limit count to valid contiguous range (PWM registers 0-11)
    safe_count = min(count, 12)

    def operation() -> bool:
        result = client.read_holding_registers(address=0, count=safe_count)
        return not result.isError()

    return run_performance_test(
        client,
        f"Read {safe_count} Holding Registers (FC03)",
        operation,
        num_requests
    )


def test_write_single_register(client: ModbusTcpClient, num_requests: int) -> PerformanceResult:
    """Test writing a single holding register (PWM duty cycle)."""
    value = 0

    def operation() -> bool:
        nonlocal value
        # PWM duty cycle range is 0-10000
        value = (value + 100) % 10001
        result = client.write_register(address=0, value=value)
        return not result.isError()

    return run_performance_test(
        client,
        "Write Single Holding Register (FC06)",
        operation,
        num_requests
    )


def test_write_multiple_registers(client: ModbusTcpClient, num_requests: int, count: int = 4) -> PerformanceResult:
    """Test writing multiple holding registers (PWM duty cycles).

    Note: Uses PWM duty cycle registers at addresses 0, 3, 6, 9.
    For contiguous writes, we use addresses 0-3 (pwm_0_duty_cycle and pwm_0_frequency).
    """
    # Use safe contiguous range
    safe_count = min(count, 3)
    values = [1000 + i * 100 for i in range(safe_count)]

    def operation() -> bool:
        result = client.write_registers(address=0, values=values)
        return not result.isError()

    return run_performance_test(
        client,
        f"Write {safe_count} Holding Registers (FC16)",
        operation,
        num_requests
    )


def test_read_coils(client: ModbusTcpClient, num_requests: int, count: int = 16) -> PerformanceResult:
    """Test reading coils (digital outputs 0-15)."""
    # Coils 0-27 are valid (16 digital outputs + 8 digital inputs + 4 PWM enables)
    safe_count = min(count, 28)

    def operation() -> bool:
        result = client.read_coils(address=0, count=safe_count)
        return not result.isError()

    return run_performance_test(
        client,
        f"Read {safe_count} Coils (FC01)",
        operation,
        num_requests
    )


def test_write_single_coil(client: ModbusTcpClient, num_requests: int) -> PerformanceResult:
    """Test writing a single coil (digital output 0)."""
    value = False

    def operation() -> bool:
        nonlocal value
        value = not value
        result = client.write_coil(address=0, value=value)
        return not result.isError()

    return run_performance_test(
        client,
        "Write Single Coil (FC05)",
        operation,
        num_requests
    )


def test_read_input_registers(client: ModbusTcpClient, num_requests: int, count: int = 4) -> PerformanceResult:
    """Test reading input registers (ADC values 0-3)."""
    # Input registers 0-3 are ADC values (contiguous)
    safe_count = min(count, 4)

    def operation() -> bool:
        result = client.read_input_registers(address=0, count=safe_count)
        return not result.isError()

    return run_performance_test(
        client,
        f"Read {safe_count} Input Registers (FC04)",
        operation,
        num_requests
    )


def test_read_discrete_inputs(client: ModbusTcpClient, num_requests: int, count: int = 8) -> PerformanceResult:
    """Test reading discrete inputs (digital inputs 0-7)."""
    # Discrete inputs 0-7 are valid
    safe_count = min(count, 8)

    def operation() -> bool:
        result = client.read_discrete_inputs(address=0, count=safe_count)
        return not result.isError()

    return run_performance_test(
        client,
        f"Read {safe_count} Discrete Inputs (FC02)",
        operation,
        num_requests
    )


def test_mixed_operations(client: ModbusTcpClient, num_requests: int) -> PerformanceResult:
    """Test mixed read/write operations using valid address ranges."""
    counter = 0

    def operation() -> bool:
        nonlocal counter
        counter += 1
        op_type = counter % 4

        if op_type == 0:
            # Read PWM registers (addresses 0-5)
            result = client.read_holding_registers(address=0, count=6)
        elif op_type == 1:
            # Write PWM duty cycle (address 0, range 0-10000)
            result = client.write_register(address=0, value=(counter * 100) % 10001)
        elif op_type == 2:
            # Read digital output coils (addresses 0-7)
            result = client.read_coils(address=0, count=8)
        else:
            # Read ADC input registers (addresses 0-3)
            result = client.read_input_registers(address=0, count=4)

        return not result.isError()

    return run_performance_test(
        client,
        "Mixed Operations (Read/Write)",
        operation,
        num_requests
    )


def test_sustained_load(client: ModbusTcpClient, duration_sec: int = 30) -> PerformanceResult:
    """Test sustained load for a specified duration using valid addresses."""
    latencies = []
    successful = 0
    failed = 0

    print(f"\nRunning: Sustained Load Test ({duration_sec} seconds)...")

    start_time = time.perf_counter()
    end_time = start_time + duration_sec
    last_progress = start_time

    while time.perf_counter() < end_time:
        req_start = time.perf_counter()
        try:
            # Read PWM registers (addresses 0-5, contiguous)
            result = client.read_holding_registers(address=0, count=6)
            if not result.isError():
                successful += 1
                req_end = time.perf_counter()
                latencies.append((req_end - req_start) * 1000)
            else:
                failed += 1
        except Exception:
            failed += 1

        # Progress indicator every 5 seconds
        current_time = time.perf_counter()
        if current_time - last_progress >= 5:
            elapsed = current_time - start_time
            print(f"  Progress: {elapsed:.0f}/{duration_sec} sec, {successful} successful")
            last_progress = current_time

    total_time = time.perf_counter() - start_time

    return PerformanceResult(
        test_name=f"Sustained Load ({duration_sec}s)",
        total_requests=successful + failed,
        successful_requests=successful,
        failed_requests=failed,
        total_time_sec=total_time,
        latencies_ms=latencies
    )


def test_burst(client: ModbusTcpClient, burst_size: int = 100, num_bursts: int = 10) -> PerformanceResult:
    """Test burst communication pattern using valid addresses."""
    latencies = []
    successful = 0
    failed = 0

    print(f"\nRunning: Burst Test ({num_bursts} bursts of {burst_size} requests)...")

    start_time = time.perf_counter()

    for burst in range(num_bursts):
        print(f"  Burst {burst + 1}/{num_bursts}...")

        for _ in range(burst_size):
            req_start = time.perf_counter()
            try:
                # Read PWM registers (addresses 0-5, contiguous)
                result = client.read_holding_registers(address=0, count=6)
                if not result.isError():
                    successful += 1
                    req_end = time.perf_counter()
                    latencies.append((req_end - req_start) * 1000)
                else:
                    failed += 1
            except Exception:
                failed += 1

        # Short pause between bursts
        time.sleep(0.1)

    total_time = time.perf_counter() - start_time

    return PerformanceResult(
        test_name=f"Burst ({num_bursts}x{burst_size})",
        total_requests=successful + failed,
        successful_requests=successful,
        failed_requests=failed,
        total_time_sec=total_time,
        latencies_ms=latencies
    )


def run_all_performance_tests(
    host: str,
    port: int,
    unit_id: int,
    quick: bool = False,
    source_ip: Optional[str] = None
) -> list[PerformanceResult]:
    """Run all performance tests.

    Args:
        host: Target Modbus server IP address
        port: Target Modbus server port
        unit_id: Modbus unit/slave ID
        quick: If True, run fewer iterations
        source_ip: Optional source IP to bind to (for multi-interface systems)
    """
    print(f"Connecting to Modbus TCP server at {host}:{port} (unit_id={unit_id})...")
    if source_ip:
        print(f"Using source IP: {source_ip}")

    # Create client with source address if specified
    if source_ip:
        client = ModbusTcpClient(
            host=host,
            port=port,
            timeout=5,
            source_address=(source_ip, 0)
        )
    else:
        client = ModbusTcpClient(host=host, port=port, timeout=5)

    client.slave = unit_id

    if not client.connect():
        print(f"ERROR: Failed to connect to {host}:{port}")
        if source_ip:
            print(f"  (using source IP: {source_ip})")
        sys.exit(1)

    print("Connected successfully!")

    # Determine test parameters based on quick mode
    if quick:
        num_requests = 100
        sustained_duration = 10
        burst_size = 50
        num_bursts = 5
    else:
        num_requests = 1000
        sustained_duration = 30
        burst_size = 100
        num_bursts = 10

    results = []

    try:
        # Basic read tests (using valid address ranges)
        results.append(test_read_single_register(client, num_requests))
        results.append(test_read_multiple_registers(client, num_requests, count=6))  # PWM regs 0-5
        results.append(test_read_multiple_registers(client, num_requests, count=12)) # PWM regs 0-11

        # Basic write tests (using PWM duty cycle registers)
        results.append(test_write_single_register(client, num_requests))
        results.append(test_write_multiple_registers(client, num_requests, count=3))  # Regs 0-2

        # Coil tests (digital outputs 0-15, inputs 16-23, PWM enables 24-27)
        results.append(test_read_coils(client, num_requests, count=16))  # Digital outputs
        results.append(test_write_single_coil(client, num_requests))

        # Input register tests (ADC values 0-3)
        results.append(test_read_input_registers(client, num_requests, count=4))
        results.append(test_read_discrete_inputs(client, num_requests, count=8))

        # Mixed operations
        results.append(test_mixed_operations(client, num_requests))

        # Sustained load test
        results.append(test_sustained_load(client, sustained_duration))

        # Burst test
        results.append(test_burst(client, burst_size, num_bursts))

    finally:
        client.close()
        print("\nConnection closed.")

    return results


def print_summary(results: list[PerformanceResult]) -> None:
    """Print summary of all test results."""
    print("\n" + "=" * 80)
    print("PERFORMANCE TEST SUMMARY")
    print("=" * 80)
    print(f"{'Test Name':<45} {'Req/s':>10} {'Avg(ms)':>10} {'P95(ms)':>10} {'Success':>10}")
    print("-" * 80)

    for result in results:
        print(f"{result.test_name:<45} "
              f"{result.requests_per_second:>10.1f} "
              f"{result.avg_latency_ms:>10.3f} "
              f"{result.p95_latency_ms:>10.3f} "
              f"{result.success_rate:>9.1f}%")

    print("=" * 80)

    # Calculate overall statistics
    total_requests = sum(r.total_requests for r in results)
    total_successful = sum(r.successful_requests for r in results)
    total_failed = sum(r.failed_requests for r in results)
    all_latencies = []
    for r in results:
        all_latencies.extend(r.latencies_ms)

    print(f"\nOverall Statistics:")
    print(f"  Total Requests:      {total_requests}")
    print(f"  Total Successful:    {total_successful}")
    print(f"  Total Failed:        {total_failed}")
    print(f"  Overall Success:     {(total_successful/total_requests)*100:.2f}%" if total_requests > 0 else "  N/A")

    if all_latencies:
        print(f"\n  Combined Latency Statistics (ms):")
        print(f"    Min:               {min(all_latencies):.3f}")
        print(f"    Max:               {max(all_latencies):.3f}")
        print(f"    Average:           {statistics.mean(all_latencies):.3f}")
        print(f"    Median:            {statistics.median(all_latencies):.3f}")


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Modbus TCP Performance Test"
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
    parser.add_argument(
        "--unit-id",
        type=int,
        default=DEFAULT_UNIT_ID,
        help=f"Modbus unit ID (default: {DEFAULT_UNIT_ID})"
    )
    parser.add_argument(
        "--quick",
        action="store_true",
        help="Run quick tests with fewer iterations"
    )
    parser.add_argument(
        "--detailed",
        action="store_true",
        help="Print detailed results for each test"
    )
    parser.add_argument(
        "--source-ip",
        default=None,
        help="Source IP address to bind to (for multi-interface/VPN systems)"
    )

    args = parser.parse_args()

    print("=" * 60)
    print("Modbus TCP Performance Test")
    print("=" * 60)
    print(f"Target: {args.host}:{args.port}")
    print(f"Unit ID: {args.unit_id}")
    if args.source_ip:
        print(f"Source IP: {args.source_ip}")
    print(f"Mode: {'Quick' if args.quick else 'Full'}")
    print("=" * 60)

    results = run_all_performance_tests(
        args.host,
        args.port,
        args.unit_id,
        args.quick,
        args.source_ip
    )

    if args.detailed:
        for result in results:
            result.print_report()

    print_summary(results)

    # Return non-zero if any test had failures
    total_failed = sum(r.failed_requests for r in results)
    sys.exit(1 if total_failed > 0 else 0)


if __name__ == "__main__":
    main()

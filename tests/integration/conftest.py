"""
Pytest Configuration for Modbus Integration Tests

This module provides fixtures and configuration for pymodbus integration tests.

Connection parameters can be supplied in three ways (highest priority first):
  1. pytest CLI options:  --modbus-host, --modbus-port, --modbus-unit-id
  2. Environment variables: MODBUS_HOST, MODBUS_PORT, MODBUS_UNIT_ID
  3. Defaults in test_config.py: 192.168.1.100, 502, 1

Example:
    uv run pytest tests/integration/ -m hardware \\
        --modbus-host 10.0.0.50 --modbus-port 502 --modbus-unit-id 3
"""

import pytest
from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ConnectionException

from test_config import MODBUS_HOST, MODBUS_PORT, MODBUS_UNIT_ID, TIMEOUT


@pytest.fixture(scope="session")
def modbus_host(request):
    """Return the Modbus host from CLI option, env var, or default."""
    return request.config.getoption("--modbus-host")


@pytest.fixture(scope="session")
def modbus_port(request):
    """Return the Modbus port from CLI option, env var, or default."""
    return request.config.getoption("--modbus-port")


@pytest.fixture(scope="session")
def modbus_unit_id(request):
    """Return the Modbus unit ID from CLI option, env var, or default."""
    return request.config.getoption("--modbus-unit-id")


@pytest.fixture(scope="module")
def modbus_client(modbus_host, modbus_port):  # pylint: disable=redefined-outer-name
    """Create a Modbus TCP client fixture shared across all tests in a module.

    Connection parameters are resolved from CLI options, environment variables,
    or test_config.py defaults (in that priority order).

    Yields:
        ModbusTcpClient: Connected Modbus client.

    Raises:
        pytest.skip: If the connection cannot be established.
    """
    client = ModbusTcpClient(
        host=modbus_host,
        port=modbus_port,
        timeout=TIMEOUT,
    )

    try:
        if not client.connect():
            pytest.skip(
                f"Cannot connect to Modbus device at {modbus_host}:{modbus_port}"
            )
    except ConnectionException as exc:
        pytest.skip(f"Connection failed: {exc}")

    yield client

    client.close()


@pytest.fixture(scope="function")
def modbus_client_per_test(modbus_host, modbus_port):  # pylint: disable=redefined-outer-name
    """Create a fresh Modbus TCP client for each individual test.

    Useful for connection-related tests that need a clean connection state.

    Yields:
        ModbusTcpClient: Modbus client (not yet connected).
    """
    client = ModbusTcpClient(
        host=modbus_host,
        port=modbus_port,
        timeout=TIMEOUT,
    )

    yield client

    if client.connected:
        client.close()


@pytest.fixture
def unit_id(modbus_unit_id):  # pylint: disable=redefined-outer-name
    """Return the configured Modbus unit ID."""
    return modbus_unit_id


def pytest_configure(config):
    """Configure pytest with custom markers."""
    config.addinivalue_line(
        "markers", "slow: marks tests as slow (deselect with '-m \"not slow\"')"
    )
    config.addinivalue_line("markers", "stress: marks tests as stress tests")
    config.addinivalue_line("markers", "hardware: marks tests that require hardware")


def pytest_collection_modifyitems(config, items):
    """Add skip markers for hardware tests when --no-hardware is specified."""
    if config.getoption("--no-hardware", default=False):
        skip_hardware = pytest.mark.skip(reason="--no-hardware option specified")
        for item in items:
            if "hardware" in item.keywords:
                item.add_marker(skip_hardware)


def pytest_addoption(parser):
    """Add custom command line options for Modbus connection parameters."""
    parser.addoption(
        "--no-hardware",
        action="store_true",
        default=False,
        help="Skip tests that require hardware connection",
    )
    parser.addoption(
        "--modbus-host",
        action="store",
        default=MODBUS_HOST,
        help=f"Modbus device IP address (default: {MODBUS_HOST}, env: MODBUS_HOST)",
    )
    parser.addoption(
        "--modbus-port",
        action="store",
        default=MODBUS_PORT,
        type=int,
        help=f"Modbus device TCP port (default: {MODBUS_PORT}, env: MODBUS_PORT)",
    )
    parser.addoption(
        "--modbus-unit-id",
        action="store",
        default=MODBUS_UNIT_ID,
        type=int,
        help=f"Modbus slave unit ID (default: {MODBUS_UNIT_ID}, env: MODBUS_UNIT_ID)",
    )
    parser.addoption(
        "--idle-timeout-s",
        action="store",
        type=float,
        default=60.0,
        help="Device Modbus receive idle timeout in seconds (default: %(default)s)",
    )

@pytest.fixture(scope="session")
def idle_timeout_s(request):
    """Return the idle timeout in seconds."""
    return request.config.getoption("--idle-timeout-s")

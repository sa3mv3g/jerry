"""
Pytest Configuration for Modbus Integration Tests

This module provides fixtures and configuration for pymodbus integration tests.
"""

import pytest
from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ModbusException, ConnectionException

from test_config import MODBUS_HOST, MODBUS_PORT, MODBUS_UNIT_ID, TIMEOUT


@pytest.fixture(scope="module")
def modbus_client():
    """
    Create a Modbus TCP client fixture.

    This fixture creates a connection to the Modbus slave device
    and yields the client for use in tests. The connection is
    closed after all tests in the module complete.

    Yields:
        ModbusTcpClient: Connected Modbus client

    Raises:
        pytest.skip: If connection cannot be established
    """
    client = ModbusTcpClient(
        host=MODBUS_HOST,
        port=MODBUS_PORT,
        timeout=TIMEOUT,
    )

    try:
        if not client.connect():
            pytest.skip(f"Cannot connect to Modbus device at {MODBUS_HOST}:{MODBUS_PORT}")
    except ConnectionException as e:
        pytest.skip(f"Connection failed: {e}")

    yield client

    client.close()


@pytest.fixture(scope="function")
def modbus_client_per_test():
    """
    Create a Modbus TCP client fixture per test.

    This fixture creates a fresh connection for each test,
    useful for connection-related tests.

    Yields:
        ModbusTcpClient: Connected Modbus client
    """
    client = ModbusTcpClient(
        host=MODBUS_HOST,
        port=MODBUS_PORT,
        timeout=TIMEOUT,
    )

    yield client

    if client.connected:
        client.close()


@pytest.fixture
def unit_id():
    """Return the configured Modbus unit ID."""
    return MODBUS_UNIT_ID


def pytest_configure(config):
    """Configure pytest with custom markers."""
    config.addinivalue_line(
        "markers", "slow: marks tests as slow (deselect with '-m \"not slow\"')"
    )
    config.addinivalue_line(
        "markers", "stress: marks tests as stress tests"
    )
    config.addinivalue_line(
        "markers", "hardware: marks tests that require hardware"
    )


def pytest_collection_modifyitems(config, items):
    """Add skip markers for hardware tests when --no-hardware is specified."""
    if config.getoption("--no-hardware", default=False):
        skip_hardware = pytest.mark.skip(reason="--no-hardware option specified")
        for item in items:
            if "hardware" in item.keywords:
                item.add_marker(skip_hardware)


def pytest_addoption(parser):
    """Add custom command line options."""
    parser.addoption(
        "--no-hardware",
        action="store_true",
        default=False,
        help="Skip tests that require hardware connection"
    )
    parser.addoption(
        "--modbus-host",
        action="store",
        default=MODBUS_HOST,
        help="Modbus device IP address"
    )
    parser.addoption(
        "--modbus-port",
        action="store",
        default=MODBUS_PORT,
        type=int,
        help="Modbus device port"
    )

"""
Pytest configuration for FOTA integration tests.

Connection parameters can be supplied in three ways (highest priority first):
  1. pytest CLI options:  --fota-host, --fota-port, --fota-firmware, --fota-ca-key, --fota-ca-cert
  2. Environment variables: FOTA_HOST, FOTA_PORT, FOTA_FIRMWARE, FOTA_CA_KEY, FOTA_CA_CERT
  3. Defaults defined below

Example:
    uv run pytest tests/integration/fota/ -m hardware \\
        --fota-host 192.168.1.100 \\
        --fota-firmware build/stm-Debug/application/jerry_app.bin

All tests in this directory are marked @pytest.mark.hardware and are skipped
unless the device is reachable on the FOTA port.
"""

from __future__ import annotations

import os
import socket
import struct
import sys
import tempfile
import time
from pathlib import Path

import pytest

# ---------------------------------------------------------------------------
# Make tools/ importable (for sign_firmware)
# ---------------------------------------------------------------------------
REPO_ROOT = Path(__file__).resolve().parents[3]
TOOLS_DIR = REPO_ROOT / "tools"
sys.path.insert(0, str(TOOLS_DIR))

# ---------------------------------------------------------------------------
# Defaults (overridable via env or CLI)
# ---------------------------------------------------------------------------
DEFAULT_FOTA_HOST = os.environ.get("FOTA_HOST", "192.168.1.100")
DEFAULT_FOTA_PORT = int(os.environ.get("FOTA_PORT", "8080"))
DEFAULT_FIRMWARE = os.environ.get(
    "FOTA_FIRMWARE",
    str(REPO_ROOT / "build" / "stm-Debug" / "application" / "jerry_app.bin"),
)
DEFAULT_CA_KEY = os.environ.get("FOTA_CA_KEY", str(REPO_ROOT / "keys" / "fota_ca.key"))
DEFAULT_CA_CERT = os.environ.get("FOTA_CA_CERT", str(REPO_ROOT / "keys" / "fota_ca.crt"))

# How long to wait for the device to reboot after a successful FOTA commit
REBOOT_WAIT_SECONDS = float(os.environ.get("FOTA_REBOOT_WAIT", "15.0"))

# How long to wait for the device to come back online after rollback
ROLLBACK_WAIT_SECONDS = float(os.environ.get("FOTA_ROLLBACK_WAIT", "20.0"))

# HTTP request timeout
HTTP_TIMEOUT_SECONDS = float(os.environ.get("FOTA_HTTP_TIMEOUT", "120.0"))


# ---------------------------------------------------------------------------
# pytest hooks
# ---------------------------------------------------------------------------


def pytest_configure(config):
    config.addinivalue_line(
        "markers", "hardware: marks tests that require a live Jerry device"
    )
    config.addinivalue_line(
        "markers", "fota: marks FOTA integration tests"
    )


def pytest_addoption(parser):
    parser.addoption(
        "--fota-host",
        action="store",
        default=DEFAULT_FOTA_HOST,
        help=f"Jerry device IP address (default: {DEFAULT_FOTA_HOST}, env: FOTA_HOST)",
    )
    parser.addoption(
        "--fota-port",
        action="store",
        default=DEFAULT_FOTA_PORT,
        type=int,
        help=f"FOTA HTTP port (default: {DEFAULT_FOTA_PORT}, env: FOTA_PORT)",
    )
    parser.addoption(
        "--fota-firmware",
        action="store",
        default=DEFAULT_FIRMWARE,
        help=f"Path to jerry_app.bin (default: {DEFAULT_FIRMWARE}, env: FOTA_FIRMWARE)",
    )
    parser.addoption(
        "--fota-ca-key",
        action="store",
        default=DEFAULT_CA_KEY,
        help=f"CA private key PEM (default: {DEFAULT_CA_KEY}, env: FOTA_CA_KEY)",
    )
    parser.addoption(
        "--fota-ca-cert",
        action="store",
        default=DEFAULT_CA_CERT,
        help=f"CA certificate PEM (default: {DEFAULT_CA_CERT}, env: FOTA_CA_CERT)",
    )
    parser.addoption(
        "--no-hardware",
        action="store_true",
        default=False,
        help="Skip all hardware tests",
    )


def pytest_collection_modifyitems(config, items):
    if config.getoption("--no-hardware", default=False):
        skip = pytest.mark.skip(reason="--no-hardware specified")
        for item in items:
            if "hardware" in item.keywords:
                item.add_marker(skip)


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------


@pytest.fixture(scope="session")
def fota_host(request):
    return request.config.getoption("--fota-host")


@pytest.fixture(scope="session")
def fota_port(request):
    return request.config.getoption("--fota-port")


@pytest.fixture(scope="session")
def fota_firmware_path(request):
    path = Path(request.config.getoption("--fota-firmware"))
    if not path.exists():
        pytest.skip(f"Firmware binary not found: {path} — run 'uv run python tools/build.py build' first")
    return path


@pytest.fixture(scope="session")
def fota_ca_key_path(request):
    path = Path(request.config.getoption("--fota-ca-key"))
    if not path.exists():
        pytest.skip(f"CA key not found: {path} — see docs/FOTA_README.md §13 Key Management")
    return path


@pytest.fixture(scope="session")
def fota_ca_cert_path(request):
    path = Path(request.config.getoption("--fota-ca-cert"))
    if not path.exists():
        pytest.skip(f"CA cert not found: {path}")
    return path


@pytest.fixture(scope="session")
def device_reachable(fota_host, fota_port):
    """Skip the entire session if the device is not reachable on the FOTA port."""
    try:
        with socket.create_connection((fota_host, fota_port), timeout=5.0):
            pass
    except (OSError, ConnectionRefusedError) as exc:
        pytest.skip(f"Device not reachable at {fota_host}:{fota_port} — {exc}")
    return True


@pytest.fixture(scope="session")
def signed_firmware(fota_firmware_path, fota_ca_key_path, fota_ca_cert_path, tmp_path_factory):
    """Sign the firmware once per session and return the signed package path."""
    from sign_firmware import sign_firmware as _sign

    tmp = tmp_path_factory.mktemp("fota")
    out_path = tmp / "jerry_app_signed.bin"
    _sign(fota_firmware_path, fota_ca_key_path, fota_ca_cert_path, out_path)
    return out_path


def wait_for_port(host: str, port: int, timeout: float, interval: float = 1.0) -> bool:
    """Poll until the TCP port is open or timeout expires. Returns True if open."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((host, port), timeout=2.0):
                return True
        except OSError:
            time.sleep(interval)
    return False

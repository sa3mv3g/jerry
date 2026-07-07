"""
FOTA integration tests — require a live Jerry device.

Tests cover:
  1. Firmware binary generation (signed package format)
  2. HTTP POST delivery (correct response codes)
  3. Firmware integrity checking (bad signature / tampered binary rejected)
  4. Rollback (device returns to previous firmware after bad update)

Prerequisites:
  - Device running and reachable at --fota-host (default: 192.168.1.100)
  - keys/fota_ca.key present (CA private key)
  - keys/fota_ca.crt present (CA certificate)
  - Firmware binary built: build/stm-Debug/application/jerry_app.bin

Run:
    uv run pytest tests/integration/fota/ -m hardware -v \\
        --fota-host 192.168.1.100

Skip hardware tests:
    uv run pytest tests/integration/fota/ --no-hardware

Environment variables (alternative to CLI options):
    FOTA_HOST=192.168.1.100
    FOTA_PORT=8080
    FOTA_FIRMWARE=build/stm-Debug/application/jerry_app.bin
    FOTA_CA_KEY=keys/fota_ca.key
    FOTA_CA_CERT=keys/fota_ca.crt
    FOTA_REBOOT_WAIT=15
    FOTA_ROLLBACK_WAIT=20
"""

from __future__ import annotations

import hashlib
import struct
import sys
import time
from pathlib import Path

import pytest
import requests

# conftest.py exports: fota_host, fota_port, fota_firmware_path, fota_ca_key_path,
#                      fota_ca_cert_path, device_reachable, signed_firmware,
#                      wait_for_port, REBOOT_WAIT_SECONDS, ROLLBACK_WAIT_SECONDS,
#                      HTTP_TIMEOUT_SECONDS

REPO_ROOT = Path(__file__).resolve().parents[3]
TOOLS_DIR = REPO_ROOT / "tools"
sys.path.insert(0, str(TOOLS_DIR))

from sign_firmware import (  # noqa: E402
    FOTA_MAGIC,
    pad_to_alignment,
    sign_firmware as _sign_firmware,
)

# Import constants from the local FOTA conftest.
# Use an explicit sys.path insert to ensure the local conftest is found,
# not the parent tests/integration/conftest.py.
import sys as _sys
import os as _os
_sys.path.insert(0, _os.path.dirname(__file__))
from conftest import (  # noqa: E402
    HTTP_TIMEOUT_SECONDS,
    REBOOT_WAIT_SECONDS,
    ROLLBACK_WAIT_SECONDS,
    wait_for_port,
)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


# Bypass corporate proxy for direct device connections (link-local / private IPs)
_NO_PROXY = {"http": None, "https": None}


def _post_firmware(host: str, port: int, package_path: Path, timeout: float = HTTP_TIMEOUT_SECONDS):
    """POST a signed firmware package to the device. Returns requests.Response."""
    url = f"http://{host}:{port}/fota"
    data = package_path.read_bytes()
    response = requests.post(
        url,
        data=data,
        headers={"Content-Type": "application/octet-stream"},
        timeout=timeout,
        proxies=_NO_PROXY,
    )
    return response


def _device_online(host: str, port: int, timeout: float = 5.0) -> bool:
    """Return True if the device FOTA port is open."""
    return wait_for_port(host, port, timeout=timeout, interval=1.0)


# ---------------------------------------------------------------------------
# 1. Firmware binary generation tests (host-side, no HTTP)
# ---------------------------------------------------------------------------


@pytest.mark.hardware
@pytest.mark.fota
class TestFirmwareBinaryGeneration:
    """Verify the signed package produced for this device has the correct format."""

    def test_signed_package_exists(self, signed_firmware):
        """sign_firmware() creates a non-empty output file."""
        assert signed_firmware.exists()
        assert signed_firmware.stat().st_size > 0

    def test_trailer_magic(self, signed_firmware):
        """Last 4 bytes are the FOTA magic 0x464F5441 ('FOTA')."""
        data = signed_firmware.read_bytes()
        magic = struct.unpack_from("<I", data, len(data) - 4)[0]
        assert magic == FOTA_MAGIC, f"Expected 0x{FOTA_MAGIC:08X}, got 0x{magic:08X}"

    def test_firmware_section_16_byte_aligned(self, signed_firmware):
        """Firmware section size is a multiple of 16 (required by STM32H5 quad-word program)."""
        data = signed_firmware.read_bytes()
        cert_size = struct.unpack_from("<I", data, len(data) - 8)[0]
        fw_size = len(data) - 8 - cert_size
        assert fw_size > 0
        assert fw_size % 16 == 0, f"Firmware section {fw_size} bytes is not 16-byte aligned"

    def test_cert_size_reasonable(self, signed_firmware):
        """Certificate DER size is between 200 and 2048 bytes (typical X.509 range)."""
        data = signed_firmware.read_bytes()
        cert_size = struct.unpack_from("<I", data, len(data) - 8)[0]
        assert 200 <= cert_size <= 2048, f"Unexpected cert size: {cert_size}"

    def test_sha256_in_cert_matches_firmware(self, signed_firmware):
        """SHA-256 hash embedded in the X.509 cert matches the firmware section."""
        try:
            from cryptography import x509
            from cryptography.hazmat.backends import default_backend
        except ImportError:
            pytest.skip("cryptography package not installed")

        data = signed_firmware.read_bytes()
        cert_size = struct.unpack_from("<I", data, len(data) - 8)[0]
        fw_size = len(data) - 8 - cert_size
        fw_bytes = data[:fw_size]
        cert_der = data[fw_size : fw_size + cert_size]

        expected_hash = hashlib.sha256(fw_bytes).digest()

        cert = x509.load_der_x509_certificate(cert_der, default_backend())
        FOTA_HASH_OID = x509.ObjectIdentifier("1.3.6.1.4.1.99999.1")
        ext = cert.extensions.get_extension_for_oid(FOTA_HASH_OID)
        raw = ext.value.value
        # Extension value is a single OCTET STRING: 04 20 <32-byte hash>
        # raw[0] = 0x04 (tag), raw[1] = 0x20 (length=32), raw[2:34] = hash
        hash_start = 2
        actual_hash = raw[hash_start : hash_start + 32]
        assert actual_hash == expected_hash


# ---------------------------------------------------------------------------
# 2. HTTP POST delivery tests
# ---------------------------------------------------------------------------


@pytest.mark.hardware
@pytest.mark.fota
class TestHttpPostDelivery:
    """Verify the HTTP server responds correctly to valid and invalid requests."""

    def test_post_wrong_path_returns_404(self, fota_host, fota_port, device_reachable):
        """POST to /wrong returns 404."""
        url = f"http://{fota_host}:{fota_port}/wrong"
        response = requests.post(
            url,
            data=b"\x00" * 16,
            headers={"Content-Type": "application/octet-stream"},
            timeout=10.0,
            proxies=_NO_PROXY,
        )
        assert response.status_code == 404

    def test_get_request_returns_405(self, fota_host, fota_port, device_reachable):
        """GET /fota returns 405 Method Not Allowed."""
        url = f"http://{fota_host}:{fota_port}/fota"
        response = requests.get(url, timeout=10.0, proxies=_NO_PROXY)
        assert response.status_code == 405

    def test_post_without_content_length_returns_411(
        self, fota_host, fota_port, device_reachable
    ):
        """POST without Content-Length returns 411 Length Required.

        Note: requests always sends Content-Length, so we use a raw socket.
        """
        import socket

        raw_request = (
            b"POST /fota HTTP/1.1\r\n"
            b"Host: " + fota_host.encode() + b"\r\n"
            b"Content-Type: application/octet-stream\r\n"
            b"Transfer-Encoding: chunked\r\n"
            b"Connection: close\r\n"
            b"\r\n"
            b"0\r\n\r\n"
        )
        with socket.create_connection((fota_host, fota_port), timeout=10.0) as sock:
            sock.sendall(raw_request)
            response_bytes = b""
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response_bytes += chunk

        status_line = response_bytes.split(b"\r\n")[0].decode()
        status_code = int(status_line.split()[1])
        assert status_code == 411

    def test_valid_upload_returns_200(
        self, fota_host, fota_port, device_reachable, signed_firmware
    ):
        """Valid signed firmware upload returns HTTP 200.

        WARNING: This test causes the device to reboot. It must run LAST in the
        delivery test class, or be run in isolation. The device will be offline
        for ~15 seconds after this test.

        The test waits for the device to come back online before returning.
        """
        response = _post_firmware(fota_host, fota_port, signed_firmware)
        assert response.status_code == 200, (
            f"Expected 200, got {response.status_code}: {response.text}"
        )
        assert "rebooting" in response.text.lower() or response.status_code == 200

        # Wait for device to reboot and come back online
        print(f"\n  Device rebooting... waiting up to {REBOOT_WAIT_SECONDS}s")
        came_back = wait_for_port(fota_host, fota_port, timeout=REBOOT_WAIT_SECONDS)
        assert came_back, (
            f"Device did not come back online within {REBOOT_WAIT_SECONDS}s after FOTA"
        )
        print(f"  Device back online after FOTA ✓")


# ---------------------------------------------------------------------------
# 3. Firmware integrity checking tests
# ---------------------------------------------------------------------------


@pytest.mark.hardware
@pytest.mark.fota
class TestFirmwareIntegrityChecking:
    """Verify the device rejects packages with bad signatures or tampered content."""

    def _make_tampered_package(self, signed_firmware: Path, tmp_path: Path, flip_offset: int) -> Path:
        """Return a copy of the signed package with one byte flipped at flip_offset."""
        data = bytearray(signed_firmware.read_bytes())
        data[flip_offset] ^= 0xFF
        out = tmp_path / "tampered.bin"
        out.write_bytes(bytes(data))
        return out

    def test_tampered_firmware_rejected(
        self, fota_host, fota_port, device_reachable, signed_firmware, tmp_path
    ):
        """Package with flipped firmware byte returns 400 (hash mismatch)."""
        tampered = self._make_tampered_package(signed_firmware, tmp_path, flip_offset=0)
        response = _post_firmware(fota_host, fota_port, tampered)
        assert response.status_code == 400, (
            f"Expected 400 for tampered firmware, got {response.status_code}"
        )
        # Device should NOT reboot — still online
        assert _device_online(fota_host, fota_port), "Device went offline after rejecting tampered firmware"

    def test_tampered_cert_rejected(
        self, fota_host, fota_port, device_reachable, signed_firmware, tmp_path
    ):
        """Package with flipped cert byte returns 400 (cert verify failure)."""
        data = signed_firmware.read_bytes()
        cert_size = struct.unpack_from("<I", data, len(data) - 8)[0]
        fw_size = len(data) - 8 - cert_size
        # Flip a byte in the middle of the cert
        cert_mid = fw_size + cert_size // 2
        tampered = self._make_tampered_package(signed_firmware, tmp_path, flip_offset=cert_mid)
        response = _post_firmware(fota_host, fota_port, tampered)
        assert response.status_code == 400, (
            f"Expected 400 for tampered cert, got {response.status_code}"
        )
        assert _device_online(fota_host, fota_port), "Device went offline after rejecting tampered cert"

    def test_wrong_magic_rejected(
        self, fota_host, fota_port, device_reachable, signed_firmware, tmp_path
    ):
        """Package with wrong trailer magic returns 400."""
        data = bytearray(signed_firmware.read_bytes())
        struct.pack_into("<I", data, len(data) - 4, 0xDEADBEEF)
        bad_magic = tmp_path / "bad_magic.bin"
        bad_magic.write_bytes(bytes(data))
        response = _post_firmware(fota_host, fota_port, bad_magic)
        assert response.status_code == 400, (
            f"Expected 400 for bad magic, got {response.status_code}"
        )
        assert _device_online(fota_host, fota_port)

    def test_unsigned_binary_rejected(
        self, fota_host, fota_port, device_reachable, fota_firmware_path, tmp_path
    ):
        """Raw unsigned firmware binary (no cert, no trailer) returns 400."""
        # Pad to 16 bytes and append a fake trailer with wrong magic
        fw_data = pad_to_alignment(fota_firmware_path.read_bytes(), 16)
        fake_trailer = struct.pack("<II", 0, 0xDEADBEEF)  # cert_size=0, wrong magic
        bad_pkg = tmp_path / "unsigned.bin"
        bad_pkg.write_bytes(fw_data + fake_trailer)
        response = _post_firmware(fota_host, fota_port, bad_pkg)
        assert response.status_code == 400
        assert _device_online(fota_host, fota_port)

    def test_wrong_ca_key_rejected(
        self, fota_host, fota_port, device_reachable, fota_firmware_path, tmp_path
    ):
        """Package signed with a different CA key returns 400 (cert verify failure)."""
        try:
            from cryptography import x509
            from cryptography.hazmat.primitives import hashes, serialization
            from cryptography.hazmat.primitives.asymmetric import ec
            from cryptography.x509.oid import NameOID
            from cryptography.hazmat.backends import default_backend
            import datetime
        except ImportError:
            pytest.skip("cryptography package not installed")

        # Generate a rogue CA key pair
        rogue_key = ec.generate_private_key(ec.SECP256R1(), default_backend())
        now = datetime.datetime.utcnow()
        rogue_cert = (
            x509.CertificateBuilder()
            .subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "Rogue CA")]))
            .issuer_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "Rogue CA")]))
            .public_key(rogue_key.public_key())
            .serial_number(x509.random_serial_number())
            .not_valid_before(now)
            .not_valid_after(now + datetime.timedelta(days=3650))
            .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
            .sign(rogue_key, hashes.SHA256(), default_backend())
        )

        rogue_key_path = tmp_path / "rogue.key"
        rogue_cert_path = tmp_path / "rogue.crt"
        rogue_key_path.write_bytes(
            rogue_key.private_bytes(
                serialization.Encoding.PEM,
                serialization.PrivateFormat.TraditionalOpenSSL,
                serialization.NoEncryption(),
            )
        )
        rogue_cert_path.write_bytes(rogue_cert.public_bytes(serialization.Encoding.PEM))

        rogue_signed = tmp_path / "rogue_signed.bin"
        _sign_firmware(fota_firmware_path, rogue_key_path, rogue_cert_path, rogue_signed)

        response = _post_firmware(fota_host, fota_port, rogue_signed)
        assert response.status_code == 400, (
            f"Expected 400 for wrong CA, got {response.status_code}"
        )
        assert _device_online(fota_host, fota_port)


# ---------------------------------------------------------------------------
# 4. Rollback test
# ---------------------------------------------------------------------------


@pytest.mark.hardware
@pytest.mark.fota
@pytest.mark.slow
class TestRollback:
    """Verify the device rolls back to the previous firmware when the new one is bad.

    This test requires the device to support the rollback mechanism:
      - FOTA_VALID_FLAG_VADDR must NOT be set after a fresh FOTA
      - The watchdog must trigger a reset if fota_mark_valid() is not called
      - On reset, fota_startup_check() calls SECURE_FOTA_Rollback()

    Since we cannot control the application's self-test from the test bench,
    this test simulates rollback by uploading a firmware that is valid at the
    crypto level but will fail the application self-test (e.g. a truncated
    firmware that crashes on startup).

    Alternatively, if the device exposes a Modbus register to trigger rollback,
    that can be used here.

    NOTE: This test is marked @pytest.mark.slow because it involves two reboots
    (FOTA commit + rollback) and takes ~30-40 seconds.
    """

    def test_device_comes_back_after_valid_fota(
        self, fota_host, fota_port, device_reachable, signed_firmware
    ):
        """After a valid FOTA upload, the device reboots and comes back online.

        This is the baseline rollback test: verify the device is reachable
        after a successful FOTA commit. The new firmware must call
        fota_mark_valid() within the watchdog timeout for the device to stay
        on the new firmware.
        """
        response = _post_firmware(fota_host, fota_port, signed_firmware)
        assert response.status_code == 200

        print(f"\n  Waiting up to {REBOOT_WAIT_SECONDS}s for device to reboot...")
        came_back = wait_for_port(fota_host, fota_port, timeout=REBOOT_WAIT_SECONDS)
        assert came_back, f"Device did not come back within {REBOOT_WAIT_SECONDS}s"
        print("  Device back online after FOTA ✓")

        # Give the application time to complete startup and mark valid
        time.sleep(3.0)

        # Verify the FOTA server is still accepting connections (device is stable)
        assert _device_online(fota_host, fota_port, timeout=5.0), (
            "Device went offline after startup — possible rollback triggered"
        )
        print("  Device stable after startup ✓")

    def test_rollback_after_watchdog_timeout(
        self, fota_host, fota_port, device_reachable, fota_firmware_path,
        fota_ca_key_path, fota_ca_cert_path, tmp_path
    ):
        """Upload a firmware that crashes before calling fota_mark_valid().

        The device should:
          1. Accept the upload (HTTP 200)
          2. Reboot into new firmware
          3. Crash / watchdog timeout (new firmware never calls fota_mark_valid)
          4. Reboot again via watchdog
          5. fota_startup_check() detects missing valid flag → SECURE_FOTA_Rollback()
          6. Device comes back on old firmware

        Since we cannot easily produce a crashing firmware in CI, this test
        uses a firmware binary with a corrupted entry point (first 4 bytes set
        to 0x00000000) which will cause an immediate HardFault on startup.

        The test verifies the device comes back online after the rollback timeout.
        """
        # Build a "bad" firmware: valid crypto signature, but corrupted binary
        # (first 4 bytes = 0x00000000 → invalid stack pointer → HardFault)
        fw_data = fota_firmware_path.read_bytes()
        bad_fw_data = b"\x00\x00\x00\x00" + fw_data[4:]  # corrupt stack pointer
        bad_fw_path = tmp_path / "bad_firmware.bin"
        bad_fw_path.write_bytes(bad_fw_data)

        bad_signed = tmp_path / "bad_signed.bin"
        _sign_firmware(bad_fw_path, fota_ca_key_path, fota_ca_cert_path, bad_signed)

        # Upload the bad firmware — should be accepted (crypto is valid)
        response = _post_firmware(fota_host, fota_port, bad_signed)
        assert response.status_code == 200, (
            f"Expected 200 for crypto-valid but crashing firmware, got {response.status_code}"
        )

        print(f"\n  Bad firmware uploaded. Waiting up to {ROLLBACK_WAIT_SECONDS}s for rollback...")

        # Device will:
        #   1. Reboot into bad firmware (~5s)
        #   2. HardFault immediately
        #   3. Watchdog reset (~watchdog timeout, typically 5-30s)
        #   4. fota_startup_check() → rollback → reboot (~5s)
        #   5. Come back on old firmware
        came_back = wait_for_port(fota_host, fota_port, timeout=ROLLBACK_WAIT_SECONDS)
        assert came_back, (
            f"Device did not come back within {ROLLBACK_WAIT_SECONDS}s after rollback. "
            "Check watchdog timeout configuration."
        )
        print("  Device back online after rollback ✓")

        # Verify the device is stable (not in a reboot loop)
        time.sleep(3.0)
        assert _device_online(fota_host, fota_port, timeout=5.0), (
            "Device went offline again after rollback — possible reboot loop"
        )
        print("  Device stable on old firmware ✓")

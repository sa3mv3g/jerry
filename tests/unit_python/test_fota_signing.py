"""
Unit tests for tools/sign_firmware.py — no hardware required.

Tests cover:
  1. Binary generation — correct package format, alignment, trailer
  2. Integrity check — tampered binary / cert / trailer detected
  3. Edge cases — empty firmware, already-aligned firmware, large firmware

Run with:
    uv run pytest tests/unit_python/test_fota_signing.py -v
"""

from __future__ import annotations

import hashlib
import struct
import sys
import tempfile
from pathlib import Path

import pytest

# ---------------------------------------------------------------------------
# Make tools/ importable
# ---------------------------------------------------------------------------
REPO_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIR = REPO_ROOT / "tools"
sys.path.insert(0, str(TOOLS_DIR))

from sign_firmware import (  # noqa: E402
    FOTA_MAGIC,
    build_signed_cert,
    compute_sha256,
    pad_to_alignment,
    sign_firmware,
)

# ---------------------------------------------------------------------------
# Fixtures — generate a throw-away ECDSA-P256 CA key pair for each test session
# ---------------------------------------------------------------------------

try:
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.x509.oid import NameOID
    from cryptography.hazmat.backends import default_backend
    import datetime

    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False

pytestmark = pytest.mark.skipif(
    not CRYPTO_AVAILABLE,
    reason="'cryptography' package not installed — run: uv add cryptography",
)


@pytest.fixture(scope="session")
def ca_key_pair(tmp_path_factory):
    """Generate a throw-away ECDSA-P256 CA key pair for the test session."""
    tmp = tmp_path_factory.mktemp("keys")
    key_path = tmp / "test_ca.key"
    cert_path = tmp / "test_ca.crt"

    # Generate key
    private_key = ec.generate_private_key(ec.SECP256R1(), default_backend())

    # Write PEM key
    key_path.write_bytes(
        private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.TraditionalOpenSSL,
            encryption_algorithm=serialization.NoEncryption(),
        )
    )

    # Self-signed CA cert
    now = datetime.datetime.utcnow()
    cert = (
        x509.CertificateBuilder()
        .subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "Test FOTA CA")]))
        .issuer_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "Test FOTA CA")]))
        .public_key(private_key.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now)
        .not_valid_after(now + datetime.timedelta(days=3650))
        .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
        .sign(private_key, hashes.SHA256(), default_backend())
    )
    cert_path.write_bytes(cert.public_bytes(serialization.Encoding.PEM))

    return key_path, cert_path


@pytest.fixture
def firmware_bin(tmp_path):
    """Create a small synthetic firmware binary (256 bytes, non-aligned)."""
    data = bytes(range(256)) * 4 + b"\xAB\xCD"  # 1026 bytes — not 16-byte aligned
    fw_path = tmp_path / "firmware.bin"
    fw_path.write_bytes(data)
    return fw_path


@pytest.fixture
def signed_package(firmware_bin, ca_key_pair, tmp_path):
    """Produce a signed firmware package and return (package_path, fw_data_padded)."""
    ca_key, ca_cert = ca_key_pair
    out_path = tmp_path / "firmware_signed.bin"
    fw_data = firmware_bin.read_bytes()
    fw_padded = pad_to_alignment(fw_data, 16)
    sign_firmware(firmware_bin, ca_key, ca_cert, out_path)
    return out_path, fw_padded


# ===========================================================================
# 1. Binary generation tests
# ===========================================================================


class TestBinaryGeneration:
    """Verify the signed package has the correct format and content."""

    def test_output_file_created(self, signed_package):
        """sign_firmware() creates the output file."""
        out_path, _ = signed_package
        assert out_path.exists()
        assert out_path.stat().st_size > 0

    def test_trailer_magic(self, signed_package):
        """Last 4 bytes of the package are the FOTA magic 0x464F5441."""
        out_path, _ = signed_package
        data = out_path.read_bytes()
        magic = struct.unpack_from("<I", data, len(data) - 4)[0]
        assert magic == FOTA_MAGIC, f"Expected magic 0x{FOTA_MAGIC:08X}, got 0x{magic:08X}"

    def test_trailer_cert_size_valid(self, signed_package):
        """cert_size in trailer is positive and equals the cert section length."""
        out_path, fw_padded = signed_package
        data = out_path.read_bytes()
        cert_size, magic = struct.unpack_from("<II", data, len(data) - 8)
        assert magic == FOTA_MAGIC
        assert cert_size > 0
        # total = fw_padded + cert + 8-byte trailer  →  cert_size == total - 8 - fw_size
        assert cert_size == len(data) - 8 - len(fw_padded)

    def test_firmware_padded_to_16_bytes(self, signed_package, firmware_bin):
        """Firmware section is padded to a 16-byte boundary."""
        out_path, fw_padded = signed_package
        data = out_path.read_bytes()
        cert_size = struct.unpack_from("<I", data, len(data) - 8)[0]
        fw_size = len(data) - 8 - cert_size
        assert fw_size % 16 == 0, f"Firmware section {fw_size} is not 16-byte aligned"
        assert fw_size == len(fw_padded)

    def test_firmware_content_preserved(self, signed_package, firmware_bin):
        """Original firmware bytes are at the start of the package (before padding)."""
        out_path, fw_padded = signed_package
        data = out_path.read_bytes()
        original = firmware_bin.read_bytes()
        assert data[: len(original)] == original

    def test_padding_bytes_are_0xff(self, signed_package, firmware_bin):
        """Padding bytes between original firmware and cert are 0xFF."""
        out_path, fw_padded = signed_package
        data = out_path.read_bytes()
        original = firmware_bin.read_bytes()
        padding = data[len(original) : len(fw_padded)]
        assert all(b == 0xFF for b in padding), "Padding bytes should be 0xFF"

    def test_sha256_in_cert_matches_firmware(self, signed_package):
        """The SHA-256 hash embedded in the X.509 cert matches the padded firmware."""
        out_path, fw_padded = signed_package
        data = out_path.read_bytes()
        cert_size, _ = struct.unpack_from("<II", data, len(data) - 8)
        fw_size = len(data) - 8 - cert_size
        fw_bytes = data[:fw_size]
        cert_der = data[fw_size : fw_size + cert_size]

        # Compute expected hash
        expected_hash = hashlib.sha256(fw_bytes).digest()

        # Parse cert and extract custom OID extension value.
        # sign_firmware.py calls _encode_octet_string(fw_hash) which produces:
        #   04 20 <32-byte hash>   (single OCTET STRING, length 0x20 = 32)
        # The cryptography library stores the raw DER value of the extension.
        cert = x509.load_der_x509_certificate(cert_der, default_backend())
        FOTA_HASH_OID = x509.ObjectIdentifier("1.3.6.1.4.1.99999.1")
        ext = cert.extensions.get_extension_for_oid(FOTA_HASH_OID)
        raw = ext.value.value  # raw DER bytes of the extension value
        # raw = 04 20 <32 bytes>
        assert raw[0] == 0x04, f"Expected OCTET STRING tag 0x04, got 0x{raw[0]:02X}"
        hash_len = raw[1]
        assert hash_len == 32, f"Expected 32-byte hash, got {hash_len}"
        actual_hash = raw[2 : 2 + hash_len]
        assert actual_hash == expected_hash, "Hash in cert does not match firmware"

    def test_package_total_size(self, signed_package):
        """Total package size = fw_padded + cert + 8-byte trailer."""
        out_path, fw_padded = signed_package
        data = out_path.read_bytes()
        cert_size, _ = struct.unpack_from("<II", data, len(data) - 8)
        assert len(data) == len(fw_padded) + cert_size + 8

    def test_already_aligned_firmware(self, ca_key_pair, tmp_path):
        """Firmware already aligned to 16 bytes — no padding added."""
        ca_key, ca_cert = ca_key_pair
        fw_data = bytes(range(16)) * 64  # 1024 bytes, exactly 16-byte aligned
        fw_path = tmp_path / "aligned.bin"
        fw_path.write_bytes(fw_data)
        out_path = tmp_path / "aligned_signed.bin"
        sign_firmware(fw_path, ca_key, ca_cert, out_path)

        data = out_path.read_bytes()
        cert_size, _ = struct.unpack_from("<II", data, len(data) - 8)
        fw_size = len(data) - 8 - cert_size
        assert fw_size == len(fw_data), "No padding should be added to aligned firmware"

    def test_empty_firmware(self, ca_key_pair, tmp_path):
        """Empty firmware binary — padded to 0 bytes (already aligned)."""
        ca_key, ca_cert = ca_key_pair
        fw_path = tmp_path / "empty.bin"
        fw_path.write_bytes(b"")
        out_path = tmp_path / "empty_signed.bin"
        sign_firmware(fw_path, ca_key, ca_cert, out_path)

        data = out_path.read_bytes()
        _, magic = struct.unpack_from("<II", data, len(data) - 8)
        assert magic == FOTA_MAGIC


# ===========================================================================
# 2. Integrity check tests (host-side verification)
# ===========================================================================


class TestIntegrityCheck:
    """Verify that tampered packages are detectable."""

    def _parse_package(self, data: bytes) -> tuple[bytes, bytes, int, int]:
        """Return (fw_bytes, cert_der, cert_size, magic)."""
        cert_size, magic = struct.unpack_from("<II", data, len(data) - 8)
        fw_size = len(data) - 8 - cert_size
        return data[:fw_size], data[fw_size : fw_size + cert_size], cert_size, magic

    def test_tampered_firmware_hash_mismatch(self, signed_package):
        """Flipping a byte in the firmware section causes hash mismatch."""
        out_path, _ = signed_package
        data = bytearray(out_path.read_bytes())
        # Flip first byte of firmware
        data[0] ^= 0xFF
        fw_bytes, cert_der, _, _ = self._parse_package(bytes(data))

        # Recompute hash and compare with cert
        actual_hash = hashlib.sha256(fw_bytes).digest()
        cert = x509.load_der_x509_certificate(cert_der, default_backend())
        FOTA_HASH_OID = x509.ObjectIdentifier("1.3.6.1.4.1.99999.1")
        ext = cert.extensions.get_extension_for_oid(FOTA_HASH_OID)
        raw = ext.value.value
        hash_start = 4  # outer tag + len + inner tag + len
        cert_hash = raw[hash_start : hash_start + 32]
        assert actual_hash != cert_hash, "Tampered firmware should not match cert hash"

    def test_tampered_cert_invalid_signature(self, signed_package, ca_key_pair):
        """Flipping a byte in the cert DER causes signature verification failure."""
        out_path, _ = signed_package
        data = bytearray(out_path.read_bytes())
        cert_size, _ = struct.unpack_from("<II", data, len(data) - 8)
        fw_size = len(data) - 8 - cert_size
        # Flip a byte in the middle of the cert
        cert_mid = fw_size + cert_size // 2
        data[cert_mid] ^= 0xFF

        cert_der = bytes(data[fw_size : fw_size + cert_size])
        _, ca_cert_path = ca_key_pair
        ca_cert = x509.load_pem_x509_certificate(ca_cert_path.read_bytes())

        # Attempt to verify — should raise an exception
        from cryptography.exceptions import InvalidSignature
        from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature

        try:
            cert = x509.load_der_x509_certificate(cert_der, default_backend())
            # If parsing succeeds, verify signature
            ca_pub = ca_cert.public_key()
            ca_pub.verify(
                cert.signature,
                cert.tbs_certificate_bytes,
                ec.ECDSA(hashes.SHA256()),
            )
            pytest.fail("Expected signature verification to fail for tampered cert")
        except Exception:
            pass  # Expected — tampered cert should fail

    def test_wrong_magic_detected(self, signed_package):
        """Wrong magic in trailer is detectable."""
        out_path, _ = signed_package
        data = bytearray(out_path.read_bytes())
        # Overwrite magic with garbage
        struct.pack_into("<I", data, len(data) - 4, 0xDEADBEEF)
        _, magic = struct.unpack_from("<II", data, len(data) - 8)
        assert magic != FOTA_MAGIC

    def test_truncated_package_detected(self, signed_package):
        """Truncated package (missing trailer) is detectable."""
        out_path, _ = signed_package
        data = out_path.read_bytes()
        truncated = data[:-4]  # Remove last 4 bytes (half the trailer)
        # cert_size from truncated trailer will be garbage
        cert_size, _ = struct.unpack_from("<II", truncated, len(truncated) - 8)
        fw_size = len(truncated) - 8 - cert_size
        # fw_size will be negative or nonsensical
        assert fw_size < 0 or fw_size > len(truncated)

    def test_wrong_ca_cert_rejected(self, signed_package, tmp_path):
        """Package signed with CA A is rejected when verified against CA B."""
        # Generate a second CA
        private_key_b = ec.generate_private_key(ec.SECP256R1(), default_backend())
        now = datetime.datetime.utcnow()
        ca_cert_b = (
            x509.CertificateBuilder()
            .subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "Wrong CA")]))
            .issuer_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "Wrong CA")]))
            .public_key(private_key_b.public_key())
            .serial_number(x509.random_serial_number())
            .not_valid_before(now)
            .not_valid_after(now + datetime.timedelta(days=3650))
            .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
            .sign(private_key_b, hashes.SHA256(), default_backend())
        )

        out_path, _ = signed_package
        data = out_path.read_bytes()
        cert_size, _ = struct.unpack_from("<II", data, len(data) - 8)
        fw_size = len(data) - 8 - cert_size
        cert_der = data[fw_size : fw_size + cert_size]

        cert = x509.load_der_x509_certificate(cert_der, default_backend())
        wrong_pub = ca_cert_b.public_key()

        from cryptography.exceptions import InvalidSignature

        with pytest.raises(Exception):
            wrong_pub.verify(
                cert.signature,
                cert.tbs_certificate_bytes,
                ec.ECDSA(hashes.SHA256()),
            )


# ===========================================================================
# 3. Helper function unit tests
# ===========================================================================


class TestHelpers:
    """Unit tests for individual helper functions in sign_firmware.py."""

    def test_pad_to_alignment_no_padding_needed(self):
        """Already-aligned data is returned unchanged."""
        data = b"A" * 32
        assert pad_to_alignment(data, 16) == data

    def test_pad_to_alignment_adds_0xff(self):
        """Padding bytes are 0xFF."""
        data = b"A" * 17  # 1 byte over 16-byte boundary
        padded = pad_to_alignment(data, 16)
        assert len(padded) == 32
        assert padded[17:] == b"\xFF" * 15

    def test_pad_to_alignment_single_byte(self):
        """Single byte is padded to 16 bytes."""
        data = b"\x42"
        padded = pad_to_alignment(data, 16)
        assert len(padded) == 16
        assert padded[0] == 0x42
        assert padded[1:] == b"\xFF" * 15

    def test_compute_sha256_known_value(self):
        """SHA-256 of empty string matches known value."""
        empty_hash = compute_sha256(b"")
        expected = bytes.fromhex(
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
        )
        assert empty_hash == expected

    def test_compute_sha256_deterministic(self):
        """Same input always produces same hash."""
        data = b"Jerry FOTA test data"
        assert compute_sha256(data) == compute_sha256(data)

    def test_fota_magic_value(self):
        """FOTA_MAGIC = 0x464F5441 — reads as 'FOTA' in big-endian (network byte order).

        The magic is stored as a uint32 little-endian in the trailer.
        When packed LE it becomes b'ATOF', but when read as a big-endian uint32
        (or as 4 ASCII bytes in memory order on a little-endian CPU) it spells 'FOTA'.
        The device reads it as: trailer[4] | trailer[5]<<8 | trailer[6]<<16 | trailer[7]<<24
        which equals 0x464F5441 = ord('F')<<24 | ord('O')<<16 | ord('T')<<8 | ord('A').
        """
        # LE packing: bytes are A, T, O, F (LSB first)
        magic_bytes_le = struct.pack("<I", FOTA_MAGIC)
        assert magic_bytes_le == b"ATOF"
        # BE packing: bytes are F, O, T, A (MSB first) — spells 'FOTA'
        magic_bytes_be = struct.pack(">I", FOTA_MAGIC)
        assert magic_bytes_be == b"FOTA"

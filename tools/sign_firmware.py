#!/usr/bin/env python3
"""
sign_firmware.py — Append X.509 certificate + 8-byte trailer to a firmware binary.

Produces a signed firmware package for FOTA upload:

  [0 .. fw_size-1]          Raw firmware binary (padded to 16-byte boundary)
  [fw_size .. total-9]      X.509 certificate DER
                              Custom extension OID 1.3.6.1.4.1.99999.1
                              contains SHA-256(firmware) as OCTET STRING
  [total-8 .. total-5]      cert_size (uint32_t LE)
  [total-4 .. total-1]      magic = 0x464F5441 ("FOTA")

Usage:
  # One-time: generate CA key pair
  openssl ecparam -name prime256v1 -genkey -noout -out keys/fota_ca.key
  openssl req -new -x509 -key keys/fota_ca.key -out keys/fota_ca.crt \\
      -subj "/CN=Jerry FOTA CA" -days 3650

  # Per-release: sign firmware
  python3 tools/sign_firmware.py \\
      --input build/stm-Debug/application/jerry_app.bin \\
      --ca-key keys/fota_ca.key \\
      --ca-cert keys/fota_ca.crt \\
      --output jerry_app_signed.bin

  # Flash
  curl -X POST http://<device-ip>:8080/fota \\
       -H "Content-Type: application/octet-stream" \\
       --data-binary @jerry_app_signed.bin

Requirements:
  pip install cryptography  (or: uv add cryptography)
"""

from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

try:
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.x509.oid import NameOID
    from cryptography.hazmat.backends import default_backend
    import datetime
except ImportError:
    print("ERROR: 'cryptography' package not found.")
    print("Install with: pip install cryptography")
    sys.exit(1)


# Custom OID for firmware SHA-256 hash extension
# OID 1.3.6.1.4.1.99999.1 (private enterprise, Jerry FOTA)
FOTA_HASH_OID = x509.ObjectIdentifier("1.3.6.1.4.1.99999.1")

# FOTA package magic
FOTA_MAGIC = 0x464F5441  # "FOTA"


def pad_to_alignment(data: bytes, alignment: int = 16) -> bytes:
    """Pad data to the given alignment boundary with 0xFF bytes."""
    remainder = len(data) % alignment
    if remainder != 0:
        data += b"\xFF" * (alignment - remainder)
    return data


def compute_sha256(data: bytes) -> bytes:
    """Compute SHA-256 hash of data."""
    return hashlib.sha256(data).digest()


def _encode_octet_string(data: bytes) -> bytes:
    """Encode bytes as a DER OCTET STRING (tag 0x04 + length + data)."""
    length = len(data)
    if length < 128:
        return bytes([0x04, length]) + data
    elif length < 256:
        return bytes([0x04, 0x81, length]) + data
    else:
        return bytes([0x04, 0x82, (length >> 8) & 0xFF, length & 0xFF]) + data


def build_signed_cert(
    fw_hash: bytes,
    ca_key_path: Path,
    ca_cert_path: Path,
) -> bytes:
    """
    Build an X.509 certificate (DER) containing the firmware SHA-256 hash
    in a custom extension, signed by the CA private key.
    """
    ca_key_pem = ca_key_path.read_bytes()
    ca_key = serialization.load_pem_private_key(ca_key_pem, password=None)

    ca_cert_pem = ca_cert_path.read_bytes()
    ca_cert = x509.load_pem_x509_certificate(ca_cert_pem)

    now = datetime.datetime.utcnow()
    builder = (
        x509.CertificateBuilder()
        .subject_name(x509.Name([
            x509.NameAttribute(NameOID.COMMON_NAME, "Jerry Firmware"),
        ]))
        .issuer_name(ca_cert.subject)
        .public_key(ca_cert.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now)
        .not_valid_after(now + datetime.timedelta(days=3650))
        .add_extension(
            x509.UnrecognizedExtension(
                oid=FOTA_HASH_OID,
                value=_encode_octet_string(fw_hash),
            ),
            critical=False,
        )
    )

    cert = builder.sign(ca_key, hashes.SHA256(), default_backend())
    return cert.public_bytes(serialization.Encoding.DER)


def sign_firmware(
    input_path: Path,
    ca_key_path: Path,
    ca_cert_path: Path,
    output_path: Path,
) -> None:
    """Sign a firmware binary and write the signed package."""
    fw_data = input_path.read_bytes()
    fw_data = pad_to_alignment(fw_data, 16)
    fw_size = len(fw_data)

    print(f"Firmware:    {input_path}")
    print(f"  Size:      {fw_size} bytes ({fw_size / 1024:.1f} KB)")

    fw_hash = compute_sha256(fw_data)
    print(f"  SHA-256:   {fw_hash.hex()}")

    cert_der = build_signed_cert(fw_hash, ca_key_path, ca_cert_path)
    cert_size = len(cert_der)
    print(f"Certificate: {cert_size} bytes (DER)")

    # 8-byte trailer: [cert_size (uint32 LE)] [magic (uint32 LE)]
    trailer = struct.pack("<II", cert_size, FOTA_MAGIC)

    package = fw_data + cert_der + trailer
    total_size = len(package)

    output_path.write_bytes(package)
    print(f"Output:      {output_path}")
    print(f"  Total:     {total_size} bytes ({total_size / 1024:.1f} KB)")
    print(f"  Layout:    [{fw_size}B firmware] + [{cert_size}B cert] + [8B trailer]")
    print()
    print("Flash with:")
    print(f"  curl -X POST http://<device-ip>:8080/fota \\")
    print(f"       -H 'Content-Type: application/octet-stream' \\")
    print(f"       --data-binary @{output_path}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Sign a Jerry firmware binary for FOTA upload",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--input", "-i", type=Path, required=True,
                        help="Raw firmware binary (jerry_app.bin)")
    parser.add_argument("--ca-key", type=Path, default=Path("keys/fota_ca.key"),
                        help="CA private key PEM (default: keys/fota_ca.key)")
    parser.add_argument("--ca-cert", type=Path, default=Path("keys/fota_ca.crt"),
                        help="CA certificate PEM (default: keys/fota_ca.crt)")
    parser.add_argument("--output", "-o", type=Path, default=None,
                        help="Output path (default: <input_stem>_signed.bin)")

    args = parser.parse_args()

    if not args.input.exists():
        print(f"ERROR: Input file not found: {args.input}")
        return 1
    if not args.ca_key.exists():
        print(f"ERROR: CA key not found: {args.ca_key}")
        print("Generate with:")
        print("  openssl ecparam -name prime256v1 -genkey -noout -out keys/fota_ca.key")
        print("  openssl req -new -x509 -key keys/fota_ca.key -out keys/fota_ca.crt \\")
        print('      -subj "/CN=Jerry FOTA CA" -days 3650')
        return 1
    if not args.ca_cert.exists():
        print(f"ERROR: CA cert not found: {args.ca_cert}")
        return 1

    if args.output is None:
        args.output = args.input.parent / (args.input.stem + "_signed.bin")

    sign_firmware(args.input, args.ca_key, args.ca_cert, args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())

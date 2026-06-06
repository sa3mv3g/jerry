# Copyright (c) 2026
# All rights reserved.
"""
Firmware Flash Robot Framework Keyword Library.

Wraps tools/flash_nucleo.py to provide Robot Framework keywords for
flashing the Jerry firmware to the STM32H563ZI Nucleo board before
running HIL test suites.
"""

import hashlib
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional

from robot.api import logger
from robot.api.deco import keyword, library

# Project root is 3 levels up from this file (tests/robot/libraries/)
_PROJECT_ROOT = Path(__file__).resolve().parents[3]
_FLASH_SCRIPT = _PROJECT_ROOT / "tools" / "flash_nucleo.py"
_DEFAULT_SECURE_ELF = (
    _PROJECT_ROOT
    / "build"
    / "application"
    / "bsp"
    / "stm"
    / "stm32h563"
    / "jerry_secure_app.elf"
)
_DEFAULT_NONSECURE_ELF = _PROJECT_ROOT / "build" / "application" / "jerry_app.elf"
_HASH_CACHE_FILE = _PROJECT_ROOT / "build" / ".robot_flash_hash"


@library(scope="GLOBAL", version="1.0.0", doc_format="reST")
class FlashLibrary:
    """Robot Framework keyword library for flashing Jerry firmware.

    Wraps ``tools/flash_nucleo.py`` to flash both the secure and non-secure
    ELF images to the STM32H563ZI Nucleo board via STM32_Programmer_CLI.

    The library is ``GLOBAL`` scope so the flash state persists across suites.
    """

    ROBOT_LIBRARY_SCOPE = "GLOBAL"

    def __init__(self) -> None:
        self._last_flash_hash: Optional[str] = None
        self._load_hash_cache()

    def _load_hash_cache(self) -> None:
        if _HASH_CACHE_FILE.exists():
            self._last_flash_hash = _HASH_CACHE_FILE.read_text().strip()

    def _save_hash_cache(self, hash_value: str) -> None:
        _HASH_CACHE_FILE.parent.mkdir(parents=True, exist_ok=True)
        _HASH_CACHE_FILE.write_text(hash_value)
        self._last_flash_hash = hash_value

    @staticmethod
    def _compute_elf_hash(
        secure_elf: Path, nonsecure_elf: Path
    ) -> str:
        """Compute a combined SHA256 hash of both ELF files."""
        hasher = hashlib.sha256()
        for elf in (secure_elf, nonsecure_elf):
            if elf.exists():
                hasher.update(elf.read_bytes())
        return hasher.hexdigest()

    # =========================================================================
    # Flash keywords
    # =========================================================================

    @keyword("Flash Firmware")
    def flash_firmware(
        self,
        secure_elf: str = "",
        nonsecure_elf: str = "",
        skip_option_bytes: bool = False,
    ) -> None:
        """Flash the Jerry firmware to the STM32H563ZI Nucleo board.

        Calls ``tools/flash_nucleo.py`` via subprocess. Both the secure and
        non-secure ELF images are flashed.

        Args:
            secure_elf: Path to the secure application ELF. Defaults to
                ``build/application/bsp/stm/stm32h563/jerry_secure_app.elf``.
            nonsecure_elf: Path to the non-secure application ELF. Defaults to
                ``build/application/jerry_app.elf``.
            skip_option_bytes: If True, skip TrustZone option byte programming
                (use when board is already configured).

        Raises:
            RuntimeError: If flashing fails.
        """
        secure_path = Path(secure_elf) if secure_elf else _DEFAULT_SECURE_ELF
        nonsecure_path = Path(nonsecure_elf) if nonsecure_elf else _DEFAULT_NONSECURE_ELF

        if not secure_path.exists():
            raise RuntimeError(f"Secure ELF not found: {secure_path}")
        if not nonsecure_path.exists():
            raise RuntimeError(f"Non-secure ELF not found: {nonsecure_path}")

        cmd = [
            sys.executable,
            str(_FLASH_SCRIPT),
            "--secure-app", str(secure_path),
            "--nonsecure-app", str(nonsecure_path),
        ]
        if skip_option_bytes:
            cmd.append("--skip-option-bytes")

        logger.info(f"FlashLibrary: flashing firmware: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)

        if result.returncode != 0:
            raise RuntimeError(
                f"Firmware flash failed (exit {result.returncode}):\n"
                f"stdout: {result.stdout}\nstderr: {result.stderr}"
            )

        new_hash = self._compute_elf_hash(secure_path, nonsecure_path)
        self._save_hash_cache(new_hash)
        logger.info("FlashLibrary: firmware flashed successfully")

    @keyword("Flash Firmware If Needed")
    def flash_firmware_if_needed(
        self,
        secure_elf: str = "",
        nonsecure_elf: str = "",
    ) -> bool:
        """Flash firmware only if the ELF files have changed since last flash.

        Uses a SHA256 hash of both ELF files stored in
        ``build/.robot_flash_hash`` to detect changes.

        Args:
            secure_elf: Path to secure ELF (optional, uses default if empty).
            nonsecure_elf: Path to non-secure ELF (optional, uses default).

        Returns:
            True if firmware was flashed, False if already up to date.
        """
        secure_path = Path(secure_elf) if secure_elf else _DEFAULT_SECURE_ELF
        nonsecure_path = Path(nonsecure_elf) if nonsecure_elf else _DEFAULT_NONSECURE_ELF

        current_hash = self._compute_elf_hash(secure_path, nonsecure_path)
        if current_hash == self._last_flash_hash:
            logger.info("FlashLibrary: firmware unchanged, skipping flash")
            return False

        self.flash_firmware(secure_elf, nonsecure_elf, skip_option_bytes=True)
        return True

    @keyword("Wait For DUT Ready")
    def wait_for_dut_ready(
        self,
        host: str = "192.168.1.100",
        port: int = 502,
        timeout: float = 30.0,
        poll_interval: float = 1.0,
    ) -> None:
        """Poll Modbus TCP until the DUT responds (firmware boot complete).

        Args:
            host: DUT IP address.
            port: Modbus TCP port (default 502).
            timeout: Maximum wait time in seconds (default 30).
            poll_interval: Polling interval in seconds (default 1.0).

        Raises:
            RuntimeError: If DUT does not respond within timeout.
        """
        from pymodbus.client import ModbusTcpClient
        from pymodbus.exceptions import ConnectionException

        deadline = time.monotonic() + float(timeout)
        logger.info(
            f"FlashLibrary: waiting for DUT at {host}:{port} "
            f"(timeout={timeout}s)"
        )
        while time.monotonic() < deadline:
            client = ModbusTcpClient(
                host=host, port=int(port), timeout=2.0
            )
            try:
                if client.connect():
                    # Try a simple read to confirm firmware is running
                    result = client.read_holding_registers(
                        address=300, count=1, slave=1
                    )
                    client.close()
                    if not result.isError():
                        logger.info("FlashLibrary: DUT is ready")
                        return
            except (ConnectionException, Exception):  # noqa: BLE001
                pass
            finally:
                if client.connected:
                    client.close()
            time.sleep(float(poll_interval))

        raise RuntimeError(
            f"DUT at {host}:{port} did not respond within {timeout}s"
        )

    @keyword("Get Firmware Version")
    def get_firmware_version(
        self,
        host: str = "192.168.1.100",
        port: int = 502,
    ) -> dict:
        """Read firmware version registers from the DUT via Modbus TCP.

        Reads holding registers 300 (major), 301 (minor), 302 (patch).

        Args:
            host: DUT IP address.
            port: Modbus TCP port.

        Returns:
            Dictionary with keys ``major``, ``minor``, ``patch``, ``version_str``.
        """
        from pymodbus.client import ModbusTcpClient

        client = ModbusTcpClient(host=host, port=int(port), timeout=3.0)
        if not client.connect():
            raise RuntimeError(f"Cannot connect to {host}:{port}")
        try:
            result = client.read_holding_registers(
                address=300, count=3, slave=1
            )
            if result.isError():
                raise RuntimeError(f"Cannot read version registers: {result}")
            major, minor, patch = result.registers
        finally:
            client.close()

        version = {
            "major": major,
            "minor": minor,
            "patch": patch,
            "version_str": f"{major}.{minor}.{patch}",
        }
        logger.info(f"FlashLibrary: firmware version = {version['version_str']}")
        return version

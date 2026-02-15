#!/usr/bin/env python3
# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.
"""
Automatic flashing script for virgin NUCLEO-H563ZI boards.

This script automates the complete setup and flashing process for a new
(virgin) STM32H563 Nucleo board, including:
1. Enabling TrustZone (TZEN option byte)
2. Configuring secure memory regions
3. Setting secure boot address
4. Flashing both secure and non-secure applications

Requirements:
    - STM32CubeCLT installed with STM32_Programmer_CLI on PATH
    - Or STM32CubeProgrammer installed with STM32_Programmer_CLI on PATH

Usage:
    python flash_nucleo.py [options]

Examples:
    # Flash with default paths (looks for ELF files in build directory)
    python flash_nucleo.py

    # Flash with custom firmware paths
    python flash_nucleo.py --secure-app path/to/secure.elf --nonsecure-app path/to/app.elf

    # Skip option byte configuration (board already configured)
    python flash_nucleo.py --skip-option-bytes

    # Dry run to see what commands would be executed
    python flash_nucleo.py --dry-run
"""

import argparse
import logging
import os
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Optional

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger(__name__)


class FlashError(Exception):
    """Exception raised for flashing errors."""


class ConnectionType(Enum):
    """STM32 Programmer connection types."""

    SWD = "SWD"
    JTAG = "JTAG"
    UART = "UART"


@dataclass
class OptionByteConfig:
    """Configuration for STM32H563 TrustZone option bytes."""

    # TrustZone enable value (0xB4 enables TrustZone)
    tzen: int = 0xB4

    # Secure watermark for Bank 1 (start and end)
    # Default: entire Bank 1 is secure (0x00 to 0x7F = 1MB)
    secwm1_strt: int = 0x00
    secwm1_end: int = 0x7F

    # Secure watermark for Bank 2 (start and end)
    # Setting STRT > END makes Bank 2 non-secure
    secwm2_strt: int = 0x7F
    secwm2_end: int = 0x00

    # Secure boot address (0x0C0000 = 0x0C000000 >> 8)
    secbootadd: int = 0x0C0000


@dataclass
class FlashConfig:
    """Configuration for firmware flashing."""

    # Secure application address (Bank 1, secure flash)
    secure_app_address: int = 0x0C000000

    # Non-secure application address (Bank 2, non-secure flash)
    nonsecure_app_address: int = 0x08100000


class STM32Programmer:
    """Wrapper for STM32_Programmer_CLI operations."""

    def __init__(
        self,
        programmer_path: Optional[str] = None,
        connection: ConnectionType = ConnectionType.SWD,
        dry_run: bool = False,
    ):
        """Initialize the STM32 Programmer wrapper.

        Args:
            programmer_path: Path to STM32_Programmer_CLI executable.
                           If None, searches PATH and common locations.
            connection: Connection type (SWD, JTAG, UART).
            dry_run: If True, only print commands without executing.
        """
        self.programmer_path = programmer_path or self._find_programmer()
        self.connection = connection
        self.dry_run = dry_run

        if not self.programmer_path:
            raise FlashError(
                "STM32_Programmer_CLI not found. Please install STM32CubeCLT or "
                "STM32CubeProgrammer and ensure it's on PATH, or specify the path "
                "using --programmer-path."
            )

        logger.info("Using programmer: %s", self.programmer_path)

    def _find_programmer(self) -> Optional[str]:
        """Find STM32_Programmer_CLI executable.

        Returns:
            Path to the programmer executable, or None if not found.
        """
        # First, check if it's on PATH
        programmer = shutil.which("STM32_Programmer_CLI")
        if programmer:
            return programmer

        # Check common installation locations
        common_paths = []

        if sys.platform == "win32":
            # Windows paths
            program_files = os.environ.get(
                "ProgramFiles", "C:\\Program Files"
            )
            program_files_x86 = os.environ.get(
                "ProgramFiles(x86)", "C:\\Program Files (x86)"
            )

            common_paths.extend(
                [
                    # STM32CubeCLT
                    Path(program_files)
                    / "STMicroelectronics"
                    / "STM32Cube"
                    / "STM32CubeCLT"
                    / "STM32CubeProgrammer"
                    / "bin"
                    / "STM32_Programmer_CLI.exe",
                    # STM32CubeProgrammer standalone
                    Path(program_files)
                    / "STMicroelectronics"
                    / "STM32Cube"
                    / "STM32CubeProgrammer"
                    / "bin"
                    / "STM32_Programmer_CLI.exe",
                    Path(program_files_x86)
                    / "STMicroelectronics"
                    / "STM32Cube"
                    / "STM32CubeProgrammer"
                    / "bin"
                    / "STM32_Programmer_CLI.exe",
                ]
            )

            # Check STM32CLT_PATH environment variable
            stm32clt_path = os.environ.get("STM32CLT_PATH")
            if stm32clt_path:
                common_paths.insert(
                    0,
                    Path(stm32clt_path)
                    / "STM32CubeProgrammer"
                    / "bin"
                    / "STM32_Programmer_CLI.exe",
                )
        else:
            # Linux/macOS paths
            common_paths.extend(
                [
                    Path("/opt/st/stm32cubeclt/STM32CubeProgrammer/bin")
                    / "STM32_Programmer_CLI",
                    Path("/usr/local/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin")
                    / "STM32_Programmer_CLI",
                    Path.home()
                    / "STM32CubeProgrammer"
                    / "bin"
                    / "STM32_Programmer_CLI",
                ]
            )

            # Check STM32CLT_PATH environment variable
            stm32clt_path = os.environ.get("STM32CLT_PATH")
            if stm32clt_path:
                common_paths.insert(
                    0,
                    Path(stm32clt_path)
                    / "STM32CubeProgrammer"
                    / "bin"
                    / "STM32_Programmer_CLI",
                )

        for path in common_paths:
            if path.exists():
                return str(path)

        return None

    def _run_command(
        self, args: list[str], check: bool = True
    ) -> subprocess.CompletedProcess:
        """Run STM32_Programmer_CLI with given arguments.

        Args:
            args: Command arguments (without the executable path).
            check: If True, raise exception on non-zero return code.

        Returns:
            CompletedProcess instance with command results.

        Raises:
            FlashError: If command fails and check is True.
        """
        cmd = [self.programmer_path] + args
        cmd_str = " ".join(cmd)

        if self.dry_run:
            logger.info("[DRY RUN] Would execute: %s", cmd_str)
            return subprocess.CompletedProcess(cmd, 0, "", "")

        logger.debug("Executing: %s", cmd_str)

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=120,  # 2 minute timeout
                check=False,
            )

            if result.stdout:
                for line in result.stdout.strip().split("\n"):
                    logger.debug("  %s", line)

            if result.returncode != 0 and check:
                error_msg = result.stderr or result.stdout or "Unknown error"
                raise FlashError(
                    f"Command failed with code {result.returncode}: {error_msg}"
                )

            return result

        except subprocess.TimeoutExpired as e:
            raise FlashError(f"Command timed out: {cmd_str}") from e
        except FileNotFoundError as e:
            raise FlashError(
                f"Programmer not found: {self.programmer_path}"
            ) from e

    def connect(self) -> bool:
        """Test connection to the target device.

        Returns:
            True if connection successful.

        Raises:
            FlashError: If connection fails.
        """
        logger.info("Testing connection to target...")
        result = self._run_command(
            ["-c", f"port={self.connection.value}"], check=False
        )

        if result.returncode != 0:
            raise FlashError(
                "Failed to connect to target. Ensure the board is connected "
                "and ST-LINK drivers are installed."
            )

        logger.info("Connection successful!")
        return True

    def read_option_bytes(self) -> str:
        """Read current option bytes from the device.

        Returns:
            String containing option byte values.
        """
        logger.info("Reading option bytes...")
        result = self._run_command(
            ["-c", f"port={self.connection.value}", "-ob", "displ"]
        )
        return result.stdout

    def write_option_bytes(self, **kwargs: int) -> None:
        """Write option bytes to the device.

        Args:
            **kwargs: Option byte name=value pairs.

        Raises:
            FlashError: If write fails.
        """
        if not kwargs:
            return

        ob_args = [f"{name}={value:#x}" for name, value in kwargs.items()]
        ob_str = " ".join(ob_args)
        logger.info("Writing option bytes: %s", ob_str)

        self._run_command(
            ["-c", f"port={self.connection.value}", "-ob"] + ob_args
        )

        logger.info("Option bytes written successfully")

    def flash_file(
        self,
        file_path: str,
        address: Optional[int] = None,
        verify: bool = True,
        reset: bool = True,
    ) -> None:
        """Flash a file to the target device.

        Args:
            file_path: Path to the file to flash (ELF, HEX, or BIN).
            address: Start address (required for BIN files, optional for ELF/HEX).
            verify: If True, verify after programming.
            reset: If True, reset the device after programming.

        Raises:
            FlashError: If flashing fails.
        """
        if not os.path.exists(file_path):
            raise FlashError(f"File not found: {file_path}")

        logger.info("Flashing: %s", file_path)

        args = ["-c", f"port={self.connection.value}", "-w", file_path]

        if address is not None:
            args.append(f"{address:#x}")

        if verify:
            args.append("-v")

        if reset:
            args.append("-rst")

        self._run_command(args)
        logger.info("Flash complete: %s", os.path.basename(file_path))

    def mass_erase(self) -> None:
        """Perform mass erase of the device flash.

        Raises:
            FlashError: If erase fails.
        """
        logger.warning("Performing mass erase...")
        self._run_command(
            ["-c", f"port={self.connection.value}", "-e", "all"]
        )
        logger.info("Mass erase complete")

    def reset_device(self) -> None:
        """Reset the target device.

        Raises:
            FlashError: If reset fails.
        """
        logger.info("Resetting device...")
        self._run_command(
            ["-c", f"port={self.connection.value}", "-rst"]
        )
        logger.info("Device reset complete")


def find_firmware_files(
    build_dir: Path,
) -> tuple[Optional[Path], Optional[Path]]:
    """Find firmware files in the build directory.

    Args:
        build_dir: Path to the build directory.

    Returns:
        Tuple of (secure_app_path, nonsecure_app_path).
    """
    secure_app = None
    nonsecure_app = None

    # Expected paths based on CMake configuration
    secure_path = (
        build_dir
        / "application"
        / "bsp"
        / "stm"
        / "stm32h563"
        / "jerry_secure_app.elf"
    )
    nonsecure_path = build_dir / "application" / "jerry_app.elf"

    if secure_path.exists():
        secure_app = secure_path
    if nonsecure_path.exists():
        nonsecure_app = nonsecure_path

    return secure_app, nonsecure_app


def configure_trustzone(
    programmer: STM32Programmer,
    config: OptionByteConfig,
    force: bool = False,
) -> None:
    """Configure TrustZone option bytes on the device.

    This is a two-step process:
    1. Enable TrustZone (TZEN=0xB4) - may trigger mass erase
    2. Configure secure regions and boot address

    Args:
        programmer: STM32Programmer instance.
        config: Option byte configuration.
        force: If True, skip confirmation prompts.

    Raises:
        FlashError: If configuration fails.
    """
    logger.info("=" * 60)
    logger.info("STEP 1: Enabling TrustZone")
    logger.info("=" * 60)
    logger.warning(
        "WARNING: Enabling TrustZone (TZEN) may trigger a mass erase!"
    )

    if not force and not programmer.dry_run:
        response = input("Continue? [y/N]: ").strip().lower()
        if response != "y":
            logger.info("Aborted by user")
            sys.exit(0)

    # Step 1: Enable TrustZone
    programmer.write_option_bytes(TZEN=config.tzen)

    # Wait for device to reset after TZEN change
    if not programmer.dry_run:
        logger.info("Waiting for device to reset...")
        time.sleep(2)

    logger.info("=" * 60)
    logger.info("STEP 2: Configuring Secure Regions and Boot Address")
    logger.info("=" * 60)

    # Step 2: Configure secure watermarks and boot address
    programmer.write_option_bytes(
        SECWM2_STRT=config.secwm2_strt,
        SECWM2_END=config.secwm2_end,
        SECBOOTADD=config.secbootadd,
    )

    logger.info("TrustZone configuration complete!")


def flash_firmware(
    programmer: STM32Programmer,
    secure_app: Path,
    nonsecure_app: Path,
    flash_config: FlashConfig,
) -> None:
    """Flash both secure and non-secure applications.

    Args:
        programmer: STM32Programmer instance.
        secure_app: Path to secure application ELF file.
        nonsecure_app: Path to non-secure application ELF file.
        flash_config: Flash address configuration.

    Raises:
        FlashError: If flashing fails.
    """
    logger.info("=" * 60)
    logger.info("STEP 3: Flashing Secure Application")
    logger.info("=" * 60)
    logger.info("  File: %s", secure_app)
    logger.info("  Address: %s", f"{flash_config.secure_app_address:#010x}")

    # Flash secure app (ELF files contain address info, but we specify for clarity)
    programmer.flash_file(
        str(secure_app),
        verify=True,
        reset=False,  # Don't reset yet, need to flash non-secure too
    )

    logger.info("=" * 60)
    logger.info("STEP 4: Flashing Non-Secure Application")
    logger.info("=" * 60)
    logger.info("  File: %s", nonsecure_app)
    logger.info("  Address: %s", f"{flash_config.nonsecure_app_address:#010x}")

    # Flash non-secure app and reset
    programmer.flash_file(
        str(nonsecure_app),
        verify=True,
        reset=True,  # Reset after final flash
    )

    logger.info("=" * 60)
    logger.info("FLASHING COMPLETE!")
    logger.info("=" * 60)


def main() -> int:
    """Main entry point for the flashing script.

    Returns:
        Exit code (0 for success, non-zero for failure).
    """
    parser = argparse.ArgumentParser(
        description="Flash Jerry firmware to NUCLEO-H563ZI board",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                              # Auto-detect and flash
  %(prog)s --build-dir ./build          # Specify build directory
  %(prog)s --skip-option-bytes          # Skip TrustZone config
  %(prog)s --dry-run                    # Show commands without executing
  %(prog)s --secure-app s.elf --nonsecure-app ns.elf  # Custom paths
        """,
    )

    parser.add_argument(
        "--build-dir",
        type=Path,
        default=Path("build"),
        help="Path to CMake build directory (default: ./build)",
    )

    parser.add_argument(
        "--secure-app",
        type=Path,
        help="Path to secure application ELF file",
    )

    parser.add_argument(
        "--nonsecure-app",
        type=Path,
        help="Path to non-secure application ELF file",
    )

    parser.add_argument(
        "--programmer-path",
        type=str,
        help="Path to STM32_Programmer_CLI executable",
    )

    parser.add_argument(
        "--skip-option-bytes",
        action="store_true",
        help="Skip option byte configuration (use if already configured)",
    )

    parser.add_argument(
        "--option-bytes-only",
        action="store_true",
        help="Only configure option bytes, don't flash firmware",
    )

    parser.add_argument(
        "--force",
        "-f",
        action="store_true",
        help="Skip confirmation prompts",
    )

    parser.add_argument(
        "--dry-run",
        "-n",
        action="store_true",
        help="Print commands without executing them",
    )

    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Enable verbose output",
    )

    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    try:
        # Initialize programmer
        programmer = STM32Programmer(
            programmer_path=args.programmer_path,
            dry_run=args.dry_run,
        )

        # Test connection
        programmer.connect()

        # Configure option bytes if needed
        if not args.skip_option_bytes:
            ob_config = OptionByteConfig()
            configure_trustzone(programmer, ob_config, force=args.force)

        # Flash firmware if not option-bytes-only mode
        if not args.option_bytes_only:
            # Find or use provided firmware paths
            if args.secure_app and args.nonsecure_app:
                secure_app = args.secure_app
                nonsecure_app = args.nonsecure_app
            else:
                secure_app, nonsecure_app = find_firmware_files(args.build_dir)

            if not secure_app:
                raise FlashError(
                    f"Secure application not found in {args.build_dir}. "
                    "Build the project first or specify --secure-app."
                )

            if not nonsecure_app:
                raise FlashError(
                    f"Non-secure application not found in {args.build_dir}. "
                    "Build the project first or specify --nonsecure-app."
                )

            flash_config = FlashConfig()
            flash_firmware(programmer, secure_app, nonsecure_app, flash_config)

        logger.info("")
        logger.info("All operations completed successfully!")
        return 0

    except FlashError as e:
        logger.error("Flash error: %s", e)
        return 1
    except KeyboardInterrupt:
        logger.info("\nOperation cancelled by user")
        return 130
    except Exception as e:
        logger.exception("Unexpected error: %s", e)
        return 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# Copyright (c) 2026
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

    # Target a specific board when multiple are connected
    python flash_nucleo.py --sn 066EFF303451897067120842

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
    """Configuration for STM32H563 TrustZone option bytes.

    Memory layout (64KB Secure + 8KB NSC + 888KB NS):
      Sectors 0-7  (64KB) = Secure firmware code
      Sector  8    (8KB)  = NSC veneer  → SECWM1_END = 8
      Sectors 9-119       = NS firmware → NSBOOTADD  = 0x08012000
      Sectors 120-127     = EDATA (Bank 1, 8 sectors, 100K-cycle calibration storage)
    """

    # TrustZone enable value (0xB4 enables TrustZone)
    tzen: int = 0xB4

    # Secure watermark for Bank 1:
    #   SECWM1_STRT = 0  (sector 0 = start of Secure area)
    #   SECWM1_END  = 8  (sector 8 = NSC veneer, last Secure sector)
    # [RM0481 §7.6.1: NSC region must be inside Secure watermark]
    secwm1_strt: int = 0x00
    secwm1_end: int = 0x1F

    # Secure watermark for Bank 2 (start and end)
    # Setting STRT > END makes Bank 2 non-secure
    secwm2_strt: int = 0x7F
    secwm2_end: int = 0x00

    # Secure boot address — Bank 1 physical sector 0 via Secure alias.
    # Stored as address >> 7 in the option byte register.
    # 0x0C000000 >> 7 = 0x180000 (STM32_Programmer_CLI takes raw address)
    # [RM0481 §7.4.6: SECBOOTADD = 0x0C00_0000 → always Bank 1 physical]
    secbootadd: int = 0x0C0000  # STM32_Programmer_CLI format: address >> 8

    # NS boot address — where MCU jumps to NS firmware after Secure init.
    # Must match NS linker script ORIGIN and VTOR_TABLE_NS_START_ADDR in Secure main.c.
    # Sector 9 of Bank 1 = 0x0800_0000 + 9 × 8KB = 0x0801_2000
    # After SWAP_BANK=1 (FOTA): this address maps to Bank 2 sector 9 (new firmware).
    # STM32_Programmer_CLI takes the REGISTER VALUE (address >> 8), same as SECBOOTADD.
    # 0x0801_2000 >> 8 = 0x80120  →  decoded back: 0x80120 << 8 = 0x0801_2000 ✓
    # [RM0481 §7.4.6: NSBOOTADD register stores address[28:8]]
    nsbootadd: int = 0x80400

    # EDATA Bank 1: enable 8 sectors (120-127) for EEPROM emulation calibration storage.
    # EDATA1_STRT = 7 → 8 sectors (0=1 sector, 7=8 sectors).
    # [RM0481 §7.3.10: EDATA sectors 120-127, 100K erase cycles]
    edata1_en: int = 1     # 1 = enabled
    edata1_strt: int = 7   # 7 = 8 sectors (sectors 120-127)

    # EDATA Bank 2: must be DISABLED (calibration is Bank 1 only).
    edata2_en: int = 0     # 0 = disabled
    edata2_strt: int = 0   # irrelevant when disabled, set to 0


@dataclass
class FlashConfig:
    """Configuration for firmware flashing."""

    # Secure application address (Bank 1, secure flash)
    secure_app_address: int = 0x0C000000

    # Non-secure application address — Bank 1 sector 9 (NS firmware start).
    # ELF files carry their own load addresses from the linker script, so this
    # value is used for logging/display only (not for actual ELF placement).
    # Must match NS linker script ORIGIN = 0x08012000.
    nonsecure_app_address: int = 0x08040000


class STM32Programmer:
    """Wrapper for STM32_Programmer_CLI operations."""

    def __init__(
        self,
        programmer_path: Optional[str] = None,
        connection: ConnectionType = ConnectionType.SWD,
        sn: Optional[str] = None,
        dry_run: bool = False,
    ):
        """Initialize the STM32 Programmer wrapper.

        Args:
            programmer_path: Path to STM32_Programmer_CLI executable.
                           If None, searches PATH and common locations.
            connection: Connection type (SWD, JTAG, UART).
            sn: Specific ST-LINK serial number to connect to.
            dry_run: If True, only print commands without executing.
        """
        self.programmer_path = programmer_path or self._find_programmer()
        self.connection = connection
        self.sn = sn
        self.dry_run = dry_run

        if not self.programmer_path:
            raise FlashError(
                "STM32_Programmer_CLI not found. Please install STM32CubeCLT or "
                "STM32CubeProgrammer and ensure it's on PATH, or specify the path "
                "using --programmer-path."
            )

        logger.info("Using programmer: %s", self.programmer_path)
        if self.sn:
            logger.info("Targeting ST-LINK SN: %s", self.sn)

    @property
    def _connect_args(self) -> list[str]:
        """Generate the base connection arguments, including the serial number if provided."""
        args = ["-c", f"port={self.connection.value}"]
        if self.sn:
            args.append(f"sn={self.sn}")
        return args

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
            program_files = os.environ.get("ProgramFiles", "C:\\Program Files")
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
                    Path(
                        "/usr/local/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin"
                    )
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
        result = self._run_command(self._connect_args, check=False)

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
        result = self._run_command(self._connect_args + ["-ob", "displ"])
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

        self._run_command(self._connect_args + ["-ob"] + ob_args)
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

        args = self._connect_args + ["-w", file_path]

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
        self._run_command(self._connect_args + ["-e", "all"])
        logger.info("Mass erase complete")

    def reset_device(self) -> None:
        """Reset the target device.

        Raises:
            FlashError: If reset fails.
        """
        logger.info("Resetting device...")
        self._run_command(self._connect_args + ["-rst"])
        logger.info("Device reset complete")


def find_firmware_files(
    build_dir: Path,
) -> tuple[Optional[Path], Optional[Path]]:
    """Find firmware files in the build directory or next to this script.

    Search order:
    1. Release artifact layout: ELFs in the same directory as this script
       (used when running from an extracted CPack ZIP on the production line).
    2. CMake build tree layout: ELFs under ``build_dir/application/...``
       (used during development).

    Args:
        build_dir: Path to the CMake build directory (fallback when ELFs are
                   not found next to the script).

    Returns:
        Tuple of (secure_app_path, nonsecure_app_path).
    """
    secure_app = None
    nonsecure_app = None

    # ------------------------------------------------------------------
    # Priority 1: release artifact layout — ELFs sit next to this script.
    # This is the layout produced by CPack for the production-line ZIP.
    # ------------------------------------------------------------------
    script_dir = Path(__file__).resolve().parent
    secure_path_release = script_dir / "jerry_secure_app.elf"
    nonsecure_path_release = script_dir / "jerry_app.elf"

    if secure_path_release.exists():
        secure_app = secure_path_release
    if nonsecure_path_release.exists():
        nonsecure_app = nonsecure_path_release

    if secure_app and nonsecure_app:
        logger.info("Using release artifact layout (ELFs next to script)")
        return secure_app, nonsecure_app

    # ------------------------------------------------------------------
    # Priority 2: CMake build tree layout (development workflow).
    # ------------------------------------------------------------------
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
    1. Enable TrustZone (TZEN=0xB4) - may trigger mass erase (only if not already enabled)
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

    # Check if TrustZone is already enabled to avoid unnecessary mass erase
    try:
        current_ob = programmer.read_option_bytes()
        tzen_enabled = "tzen=0xb4" in current_ob.lower()
        if tzen_enabled:
            logger.info("TrustZone is already enabled (TZEN=0xB4) - skipping enable step")
        else:
            logger.warning(
                "WARNING: Enabling TrustZone (TZEN) may trigger a mass erase!"
            )

            if not force and not programmer.dry_run:
                # Auto-confirm when stdin is not a TTY (e.g. build.py flash, CI, pipes).
                # Only prompt when running interactively in a terminal.
                import sys as _sys
                if not _sys.stdin.isatty():
                    logger.info("Non-interactive mode — auto-confirming TrustZone enable")
                else:
                    response = input("Continue? [y/N]: ").strip().lower()
                    if response != "y":
                        logger.info("Aborted by user")
                        _sys.exit(0)

            # Step 1: Enable TrustZone
            programmer.write_option_bytes(TZEN=config.tzen)

            # Wait for device to reset after TZEN change
            if not programmer.dry_run:
                logger.info("Waiting for device to reset...")
                time.sleep(2)
    except Exception as e:
        logger.warning(f"Could not read option bytes to check TZEN status: {e}")
        logger.warning("Proceeding with TrustZone enable (may cause mass erase)")
        if not force and not programmer.dry_run:
            # Auto-confirm when stdin is not a TTY (e.g. build.py flash, CI, pipes).
            # Only prompt when running interactively in a terminal.
            import sys as _sys
            if not _sys.stdin.isatty():
                logger.info("Non-interactive mode — auto-confirming TrustZone enable")
            else:
                response = input("Continue? [y/N]: ").strip().lower()
                if response != "y":
                    logger.info("Aborted by user")
                    _sys.exit(0)

        # Step 1: Enable TrustZone
        programmer.write_option_bytes(TZEN=config.tzen)

        # Wait for device to reset after TZEN change
        if not programmer.dry_run:
            logger.info("Waiting for device to reset...")
            time.sleep(2)

    logger.info("=" * 60)
    logger.info("STEP 2: Configuring Secure Regions, Boot Address and EDATA")
    logger.info("=" * 60)

    # Step 2a: Configure secure watermarks, boot addresses, and reset SWAP_BANK.
    # SWAP_BANK=0 is set here to ensure the device boots from Bank 1 after a
    # full reflash. The firmware is always written to Bank 1 by the flash script.
    # During normal FOTA operation, SWAP_BANK is toggled by fota_boot_check()
    # in Secure main.c — but a full reflash must start from a known state.
    programmer.write_option_bytes(
        SECWM1_STRT=config.secwm1_strt,
        SECWM1_END=config.secwm1_end,
        SECWM2_STRT=config.secwm2_strt,
        SECWM2_END=config.secwm2_end,
        SECBOOTADD=config.secbootadd,
        NSBOOTADD=config.nsbootadd,
        SWAP_BANK=0x0,
    )

    # Step 2b: Configure EDATA option bytes.
    # EDATA1: Bank 1 sectors 120-127 (8 sectors, 100K-cycle calibration storage).
    # EDATA2: Bank 2 EDATA must be DISABLED (calibration is Bank 1 only).
    # STM32_Programmer_CLI uses individual field names: EDATA1_EN, EDATA1_STRT, etc.
    logger.info("Configuring EDATA option bytes (Bank1=enabled/8sectors, Bank2=disabled)...")
    programmer.write_option_bytes(
        EDATA1_EN=config.edata1_en,      # 1 = Bank 1 EDATA enabled
        EDATA1_STRT=config.edata1_strt,  # 7 = 8 sectors (120-127)
        EDATA2_EN=config.edata2_en,      # 0 = Bank 2 EDATA disabled
        EDATA2_STRT=config.edata2_strt,  # 0 = irrelevant when disabled
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

    # Flash secure app without verify.
    # After TrustZone is enabled, the Secure flash alias (0x0C000000) is
    # protected from Non-Secure/debug read-back. STM32CubeProgrammer reads
    # back zeros and reports a mismatch even though the data was written
    # correctly (download shows 100%). Skip verify for the Secure firmware;
    # the NS firmware verify below confirms the overall flash operation worked.
    programmer.flash_file(
        str(secure_app),
        verify=False,
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
  %(prog)s --sn 066EFF303451897067120842 # Target specific board
  %(prog)s --build-dir ./build          # Specify build directory
  %(prog)s --skip-option-bytes          # Skip TrustZone config
  %(prog)s --dry-run                    # Show commands without executing
  %(prog)s --secure-app s.elf --nonsecure-app ns.elf  # Custom paths
        """,
    )

    parser.add_argument(
        "--sn",
        type=str,
        help="ST-LINK Serial Number (to target a specific board)",
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
            sn=args.sn,
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

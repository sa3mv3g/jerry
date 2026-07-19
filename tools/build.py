#!/usr/bin/env python3
# Copyright (c) 2026
# All rights reserved.
# ruff: noqa: S324  # MD5 is used for file integrity, not cryptographic security
"""
Jerry Firmware Build Script
============================
A cross-platform build orchestrator that wraps CMake + Ninja.
This is the single source of truth for build configuration.

Usage:
    uv run python tools/build.py [COMMAND] [OPTIONS]

Commands:
    configure   Run CMake configure only
    build       Configure (if needed) + build jerry_app  [default]
    clean       Remove build directory for the selected vendor/profile
    clean-all   Remove all build directories
    flash       Flash firmware to device
    lint        Run all static analysis tools
    format      Run clang-format on C/C++ and Python sources
    package     Build + create CPack ZIP package

Examples:
    uv run python tools/build.py
    uv run python tools/build.py --vendor stm --profile release
    uv run python tools/build.py --profile debug --enable-i2c-scan
    uv run python tools/build.py clean --profile release
    uv run python tools/build.py flash --profile release
    uv run python tools/build.py lint
"""

import argparse
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

# =============================================================================
# Constants
# =============================================================================

# Map user-friendly lowercase profile names to CMake build type conventions.
# This is the authoritative mapping — the single source of truth for profile names.
PROFILE_MAP: dict[str, str] = {
    "debug": "Debug",
    "release": "Release",
    "relwithdebinfo": "RelWithDebInfo",
    "minsizerel": "MinSizeRel",
}

# Supported vendors. Adding a new vendor requires:
#   1. Adding it here
#   2. Adding an elseif branch in application/bsp/toolchain.cmake
SUPPORTED_VENDORS: list[str] = ["stm"]

# Default CMake option values. These are the project defaults and are passed
# explicitly so the build is fully reproducible without relying on CMake defaults.
CMAKE_DEFAULTS: dict[str, str] = {
    "CPPCHECK_USE_ADDONS": "OFF",
    "ENABLE_I2C_DEVICE_SCAN": "OFF",
    "MODBUS_ENABLE_RTU": "ON",
    "MODBUS_ENABLE_ASCII": "ON",
    "MODBUS_ENABLE_TCP": "ON",
}

# Root of the project (parent of tools/)
PROJECT_ROOT: Path = Path(__file__).resolve().parent.parent

# Toolchain file path (relative to project root)
TOOLCHAIN_FILE: str = "application/bsp/toolchain.cmake"


# =============================================================================
# Helpers
# =============================================================================


def build_dir(vendor: str, profile: str) -> Path:
    """Return the build directory path for a given vendor and profile."""
    cmake_profile = PROFILE_MAP[profile]
    return PROJECT_ROOT / "build" / f"{vendor}-{cmake_profile}"


def run(cmd: list[str], cwd: Path | None = None) -> int:
    """Run a command, streaming output. Returns exit code."""
    cwd = cwd or PROJECT_ROOT
    print(f"\n[build.py] Running: {' '.join(str(c) for c in cmd)}")
    print(f"[build.py] Working dir: {cwd}\n")
    result = subprocess.run(cmd, cwd=cwd, check=False)
    return result.returncode


def cmake_configure(
    vendor: str,
    profile: str,
    extra_cmake_args: list[str],
    force: bool = False,
) -> int:
    """
    Run CMake configure step.

    Skips configure if CMakeCache.txt already exists and force=False.
    """
    bdir = build_dir(vendor, profile)
    cmake_cache = bdir / "CMakeCache.txt"

    if cmake_cache.exists() and not force:
        print(
            f"[build.py] CMakeCache.txt exists at {bdir}, skipping configure."
        )
        print("[build.py] Use --reconfigure to force re-configure.")
        return 0

    cmake_profile = PROFILE_MAP[profile]

    cmd: list[str] = [
        "cmake",
        "-S",
        str(PROJECT_ROOT),
        "-B",
        str(bdir),
        "-G",
        "Ninja",
        f"-DCMAKE_TOOLCHAIN_FILE={TOOLCHAIN_FILE}",
        f"-DVENDOR={vendor}",
        f"-DCMAKE_BUILD_TYPE={cmake_profile}",
    ]

    # Append all default options explicitly for reproducibility
    for key, value in CMAKE_DEFAULTS.items():
        cmd.append(f"-D{key}={value}")

    # Append any extra user-provided -D flags
    cmd.extend(extra_cmake_args)

    return run(cmd)


def cmake_build(vendor: str, profile: str, target: str = "jerry_app") -> int:
    """Run CMake build step."""
    bdir = build_dir(vendor, profile)
    cmd: list[str] = [
        "cmake",
        "--build",
        str(bdir),
        "--target",
        target,
    ]
    return run(cmd)


# =============================================================================
# Command Implementations
# =============================================================================


def _version_args_provided(args: argparse.Namespace) -> bool:
    """Return True if any --version-* argument was explicitly provided.

    When version args are provided, CMake must be re-configured so the new
    APP_VERSION_* cache values are written and picked up by CPack and gen_version.py.
    """
    return any(
        getattr(args, attr, None) is not None
        for attr in ("version_major", "version_minor", "version_patch")
    )


def cmd_configure(args: argparse.Namespace) -> int:
    """Run CMake configure step only, without building."""
    extra = _collect_extra_cmake_args(args)
    return cmake_configure(
        args.vendor,
        args.profile,
        extra,
        force=args.reconfigure or _version_args_provided(args),
    )


def cmd_build(args: argparse.Namespace) -> int:
    """
    Build the firmware.

    For Release profile, the following additional steps run automatically:
      PRE-BUILD:
        1. clang-format  — enforces code style before compiling
      POST-BUILD:
        2. cppcheck (MISRA) — static analysis with MISRA C:2012 addon
        3. lizard           — cyclomatic complexity and code metrics
    """
    is_release = args.profile == "release"

    extra = _collect_extra_cmake_args(args)

    # For Release builds, MISRA checking is mandatory.
    # Force CPPCHECK_USE_ADDONS=ON at configure time so the cppcheck CMake
    # target includes the MISRA addon.
    if is_release and "-DCPPCHECK_USE_ADDONS=ON" not in extra:
        extra.append("-DCPPCHECK_USE_ADDONS=ON")

    rc = cmake_configure(
        args.vendor,
        args.profile,
        extra,
        force=args.reconfigure or _version_args_provided(args),
    )
    if rc != 0:
        return rc

    # -------------------------------------------------------------------------
    # PRE-BUILD step (Release only): enforce clang-format
    # -------------------------------------------------------------------------
    if is_release:
        print("\n[build.py] === PRE-BUILD: Running clang-format (Release) ===")
        rc = cmake_build(args.vendor, args.profile, target="format")
        if rc != 0:
            print(
                "[build.py] ERROR: clang-format failed. "
                "Fix formatting issues before Release build."
            )
            return rc

    # -------------------------------------------------------------------------
    # Main firmware build
    # -------------------------------------------------------------------------
    rc = cmake_build(args.vendor, args.profile)
    if rc != 0:
        return rc

    # -------------------------------------------------------------------------
    # POST-BUILD steps (Release only): MISRA cppcheck + Lizard
    # -------------------------------------------------------------------------
    if is_release:
        print(
            "\n[build.py] === POST-BUILD: Running MISRA cppcheck (Release) ==="
        )
        rc_cppcheck = cmake_build(args.vendor, args.profile, target="cppcheck")
        if rc_cppcheck != 0:
            print(
                "[build.py] WARNING: cppcheck/MISRA reported issues. "
                "See report for details."
            )
            # Non-fatal by default: analysis continues to lizard.
            # To make MISRA failures abort the build, change to: return rc_cppcheck

        print(
            "\n[build.py] === POST-BUILD: Running Lizard complexity analysis (Release) ==="
        )
        rc_lizard = cmake_build(args.vendor, args.profile, target="lizard")
        if rc_lizard != 0:
            print(
                "[build.py] WARNING: Lizard reported complexity issues. "
                "See report for details."
            )
            # Non-fatal by default.
            # To make Lizard failures abort the build, change to: return rc_lizard

    return 0


def cmd_clean(args: argparse.Namespace) -> int:
    """Remove the build directory for the selected vendor and profile."""
    bdir = build_dir(args.vendor, args.profile)
    if bdir.exists():
        print(f"[build.py] Removing {bdir}")
        shutil.rmtree(bdir)
    else:
        print(f"[build.py] Nothing to clean: {bdir} does not exist.")
    return 0


def cmd_clean_all(_args: argparse.Namespace) -> int:
    """Remove all build directories under build/."""
    build_root = PROJECT_ROOT / "build"
    if build_root.exists():
        print(f"[build.py] Removing {build_root}")
        shutil.rmtree(build_root)
    else:
        print("[build.py] Nothing to clean: build/ does not exist.")
    return 0


def cmd_flash(args: argparse.Namespace) -> int:
    """Flash firmware to the target device using tools/flash_nucleo.py."""
    bdir = build_dir(args.vendor, args.profile)
    flash_script = PROJECT_ROOT / "tools" / "flash_nucleo.py"
    cmd: list[str] = [
        sys.executable,
        str(flash_script),
        "--build-dir",
        str(bdir),
        "-f"
    ]
    if args.serial_number:
        cmd.extend(["--sn", args.serial_number])
    return run(cmd)



def cmd_lint(args: argparse.Namespace) -> int:
    """Run all static analysis tools (cppcheck, lizard, pylint, ruff)."""
    extra = _collect_extra_cmake_args(args)
    rc = cmake_configure(
        args.vendor, args.profile, extra, force=args.reconfigure
    )
    if rc != 0:
        return rc
    return cmake_build(args.vendor, args.profile, target="lint")


def cmd_format(args: argparse.Namespace) -> int:
    """Run clang-format on all C/C++ and Python sources."""
    extra = _collect_extra_cmake_args(args)
    rc = cmake_configure(
        args.vendor, args.profile, extra, force=args.reconfigure
    )
    if rc != 0:
        return rc
    return cmake_build(args.vendor, args.profile, target="format")


def _generate_md5(zip_path: Path) -> Path:
    """Compute MD5 of zip_path and write a <name>.md5 sidecar file.

    The sidecar file contains a single line in the standard md5sum format:
        <hex_digest>  <filename>

    Args:
        zip_path: Path to the ZIP file to checksum.

    Returns:
        Path to the generated .md5 sidecar file.
    """
    md5 = hashlib.md5()  # noqa: S324
    with open(zip_path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            md5.update(chunk)
    digest = md5.hexdigest()
    md5_path = zip_path.with_suffix(".md5")
    md5_path.write_text(f"{digest}  {zip_path.name}\n", encoding="utf-8")
    return md5_path


def cmd_package(args: argparse.Namespace) -> int:
    """Build the firmware, create a CPack ZIP package, and generate an MD5 sidecar file."""
    extra = _collect_extra_cmake_args(args)
    # If any version arg was explicitly provided, force reconfigure so the new
    # APP_VERSION_* values are written to the CMake cache and picked up by CPack.
    rc = cmake_configure(
        args.vendor,
        args.profile,
        extra,
        force=args.reconfigure or _version_args_provided(args),
    )
    if rc != 0:
        return rc
    rc = cmake_build(args.vendor, args.profile)
    if rc != 0:
        return rc
    bdir = build_dir(args.vendor, args.profile)
    rc = run(["cpack"], cwd=bdir)
    if rc != 0:
        return rc

    # Find the generated ZIP and produce an MD5 sidecar file next to it.
    zip_files = sorted(bdir.glob("*.zip"))
    if not zip_files:
        print("[build.py] WARNING: No ZIP file found in build dir after cpack.")
        return 0

    for zip_path in zip_files:
        md5_path = _generate_md5(zip_path)
        print(f"[build.py] MD5: {md5_path.name}")
        print(f"[build.py]      {md5_path.read_text(encoding='utf-8').strip()}")

    return 0


# =============================================================================
# Argument Parsing
# =============================================================================


def _collect_extra_cmake_args(args: argparse.Namespace) -> list[str]:
    """
    Translate parsed optional feature flags into CMake -D arguments,
    overriding the defaults in CMAKE_DEFAULTS.
    """
    extra: list[str] = []

    if args.enable_i2c_scan:
        extra.append("-DENABLE_I2C_DEVICE_SCAN=ON")
    if args.cppcheck_addons:
        extra.append("-DCPPCHECK_USE_ADDONS=ON")
    if args.disable_modbus_rtu:
        extra.append("-DMODBUS_ENABLE_RTU=OFF")
    if args.disable_modbus_ascii:
        extra.append("-DMODBUS_ENABLE_ASCII=OFF")
    if args.disable_modbus_tcp:
        extra.append("-DMODBUS_ENABLE_TCP=OFF")

    # Application version overrides (passed to CMake cache vars APP_VERSION_*)
    if getattr(args, "version_major", None) is not None:
        extra.append(f"-DAPP_VERSION_MAJOR={args.version_major}")
    if getattr(args, "version_minor", None) is not None:
        extra.append(f"-DAPP_VERSION_MINOR={args.version_minor}")
    if getattr(args, "version_patch", None) is not None:
        extra.append(f"-DAPP_VERSION_PATCH={args.version_patch}")

    # Pass through any raw -D flags from --cmake-arg
    extra.extend(args.cmake_arg or [])

    return extra


def build_parser() -> argparse.ArgumentParser:
    """Build and return the top-level argument parser with all sub-commands."""
    parser = argparse.ArgumentParser(
        prog="build.py",
        description="Jerry Firmware Build Script",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )

    # -------------------------------------------------------------------------
    # Sub-commands
    # -------------------------------------------------------------------------
    subparsers = parser.add_subparsers(dest="command", metavar="COMMAND")

    # configure
    p_configure = subparsers.add_parser(
        "configure", help="Run CMake configure only"
    )
    _add_common_args(p_configure)

    # build (default)
    p_build = subparsers.add_parser(
        "build", help="Configure (if needed) + build jerry_app [default]"
    )
    _add_common_args(p_build)

    # clean
    p_clean = subparsers.add_parser(
        "clean", help="Remove build directory for selected vendor/profile"
    )
    _add_vendor_profile_args(p_clean)

    # clean-all
    subparsers.add_parser("clean-all", help="Remove all build directories")

    # flash
    p_flash = subparsers.add_parser("flash", help="Flash firmware to device")
    _add_vendor_profile_args(p_flash)
    p_flash.add_argument(
        "--sn",
        default=None,
        dest="serial_number",
        metavar="SN",
        help="Target device serial number",
    )

    # lint
    p_lint = subparsers.add_parser("lint", help="Run all static analysis tools")
    _add_common_args(p_lint)

    # format
    p_format = subparsers.add_parser(
        "format", help="Run clang-format on C/C++ and Python sources"
    )
    _add_common_args(p_format)

    # package
    p_package = subparsers.add_parser(
        "package", help="Build + create CPack ZIP package"
    )
    _add_common_args(p_package)

    return parser


def _add_vendor_profile_args(parser: argparse.ArgumentParser) -> None:
    """Add --vendor and --profile arguments to a subparser."""
    parser.add_argument(
        "--vendor",
        default="stm",
        choices=SUPPORTED_VENDORS,
        help="Microcontroller vendor (default: stm)",
    )
    parser.add_argument(
        "--profile",
        default="debug",
        choices=list(PROFILE_MAP.keys()),
        help="Build profile (default: debug)",
    )


def _add_common_args(parser: argparse.ArgumentParser) -> None:
    """Add all common arguments (vendor, profile, feature flags) to a subparser."""
    _add_vendor_profile_args(parser)

    parser.add_argument(
        "--reconfigure",
        action="store_true",
        help="Force re-run of CMake configure even if CMakeCache.txt exists",
    )

    # Optional feature flags (override CMAKE_DEFAULTS)
    feat = parser.add_argument_group("Optional Features")
    feat.add_argument(
        "--enable-i2c-scan",
        action="store_true",
        dest="enable_i2c_scan",
        help="Enable I2C bus scanning at startup (ENABLE_I2C_DEVICE_SCAN=ON)",
    )
    feat.add_argument(
        "--cppcheck-addons",
        action="store_true",
        dest="cppcheck_addons",
        help="Enable cppcheck MISRA and naming addons (CPPCHECK_USE_ADDONS=ON)",
    )
    feat.add_argument(
        "--disable-modbus-rtu",
        action="store_true",
        dest="disable_modbus_rtu",
        help="Disable Modbus RTU protocol (MODBUS_ENABLE_RTU=OFF)",
    )
    feat.add_argument(
        "--disable-modbus-ascii",
        action="store_true",
        dest="disable_modbus_ascii",
        help="Disable Modbus ASCII protocol (MODBUS_ENABLE_ASCII=OFF)",
    )
    feat.add_argument(
        "--disable-modbus-tcp",
        action="store_true",
        dest="disable_modbus_tcp",
        help="Disable Modbus TCP protocol (MODBUS_ENABLE_TCP=OFF)",
    )

    # Application version (override CMake cache vars APP_VERSION_*)
    ver = parser.add_argument_group("Application Version")
    ver.add_argument(
        "--version-major",
        default=None,
        dest="version_major",
        metavar="N",
        help="Override application version major (APP_VERSION_MAJOR, default: 1)",
    )
    ver.add_argument(
        "--version-minor",
        default=None,
        dest="version_minor",
        metavar="N",
        help="Override application version minor (APP_VERSION_MINOR, default: 0)",
    )
    ver.add_argument(
        "--version-patch",
        default=None,
        dest="version_patch",
        metavar="N",
        help="Override application version patch (APP_VERSION_PATCH, default: 0)",
    )

    # Escape hatch for arbitrary CMake -D flags
    parser.add_argument(
        "--cmake-arg",
        action="append",
        metavar="-DKEY=VALUE",
        help="Pass additional CMake -D argument (can be repeated)",
    )


# =============================================================================
# Entry Point
# =============================================================================

COMMAND_MAP = {
    "configure": cmd_configure,
    "build": cmd_build,
    "clean": cmd_clean,
    "clean-all": cmd_clean_all,
    "flash": cmd_flash,
    "lint": cmd_lint,
    "format": cmd_format,
    "package": cmd_package,
}


def main() -> int:
    """Parse arguments and dispatch to the appropriate command handler."""
    parser = build_parser()
    args = parser.parse_args()

    # Default command is 'build' when none is specified
    if args.command is None:
        args.command = "build"
        # Apply defaults for vendor/profile/flags that build needs
        if not hasattr(args, "vendor"):
            args.vendor = "stm"
        if not hasattr(args, "profile"):
            args.profile = "debug"
        if not hasattr(args, "reconfigure"):
            args.reconfigure = False
        if not hasattr(args, "enable_i2c_scan"):
            args.enable_i2c_scan = False
        if not hasattr(args, "cppcheck_addons"):
            args.cppcheck_addons = False
        if not hasattr(args, "disable_modbus_rtu"):
            args.disable_modbus_rtu = False
        if not hasattr(args, "disable_modbus_ascii"):
            args.disable_modbus_ascii = False
        if not hasattr(args, "disable_modbus_tcp"):
            args.disable_modbus_tcp = False
        if not hasattr(args, "cmake_arg"):
            args.cmake_arg = []

    handler = COMMAND_MAP.get(args.command)
    if handler is None:
        parser.print_help()
        return 1

    cmake_profile = PROFILE_MAP.get(getattr(args, "profile", "debug"), "Debug")
    vendor = getattr(args, "vendor", "stm")
    print(f"[build.py] Vendor: {vendor}  |  Profile: {cmake_profile}")
    print(f"[build.py] Build dir: build/{vendor}-{cmake_profile}")

    return handler(args)


if __name__ == "__main__":
    sys.exit(main())

# Copyright (c) 2026
# All rights reserved.

"""
Script to run Lizard cyclomatic complexity analysis.

Invokes lizard directly from Python to avoid CMake glob-expanding the
--exclude patterns into actual filesystem paths before they reach lizard.
The output format (HTML for non-Release, XML for Release) is selected via
the --release flag.

Usage (via CMake target):
    uv run python tools/run_lizard.py
    uv run python tools/run_lizard.py --release
"""

import argparse
import os
import subprocess
import sys


# =============================================================================
# Configuration
# =============================================================================

# Paths to analyse (relative to project root)
ANALYSE_PATHS: list[str] = [
    "application/src",
    "application/inc",
    "application/dependencies/modbus",
    "application/dependencies/adc_filter",
    "application/dependencies/lwip/port",
    "application/dependencies/lcd_i2c",
]

# Glob patterns passed to lizard --exclude.
# These are NOT expanded by the shell/CMake — lizard handles them internally.
EXCLUDE_PATTERNS: list[str] = [
    "./application/bsp/*",
    "./application/dependencies/FreeRTOS-Kernel/*",
    "./application/dependencies/CMSIS_6/*",
    "./application/dependencies/CMSIS-DSP/*",
    "./application/dependencies/lwip/stm32-mw-lwip/*",
    "./tools/*",
    "./tests/*",
]

# Lizard thresholds
CCN_THRESHOLD: int = 15
LENGTH_THRESHOLD: int = 100
ARGUMENTS_THRESHOLD: int = 5

# Report filename (without extension — extension is added based on build type)
REPORT_BASENAME: str = "lizrad_report"


# =============================================================================
# Helpers
# =============================================================================


def _project_root() -> str:
    """Return the absolute path to the project root directory."""
    return os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def _build_dir(root: str) -> str:
    """
    Return the build output directory.

    Reads CMAKE_BINARY_DIR from the environment if set (injected by CMake),
    otherwise falls back to <project_root>/build.
    """
    cmake_binary_dir = os.environ.get("CMAKE_BINARY_DIR", "")
    if cmake_binary_dir:
        return cmake_binary_dir
    return os.path.join(root, "build")


# =============================================================================
# Main
# =============================================================================


def main() -> int:
    """Parse arguments and run lizard with the appropriate output format."""
    parser = argparse.ArgumentParser(
        description="Run Lizard cyclomatic complexity analysis."
    )
    parser.add_argument(
        "--release",
        action="store_true",
        help="Use XML output format (for Release builds); default is HTML.",
    )
    args = parser.parse_args()

    root = _project_root()
    build_output_dir = _build_dir(root)

    # Ensure output directory exists
    os.makedirs(build_output_dir, exist_ok=True)

    # Select output format based on build type
    if args.release:
        report_file = os.path.join(build_output_dir, f"{REPORT_BASENAME}.xml")
        output_format_flag = "--xml"
    else:
        report_file = os.path.join(build_output_dir, f"{REPORT_BASENAME}.html")
        output_format_flag = "--html"

    # Build the lizard command
    cmd: list[str] = [
        sys.executable,
        "-m",
        "lizard",
        f"--CCN={CCN_THRESHOLD}",
        f"--length={LENGTH_THRESHOLD}",
        f"--arguments={ARGUMENTS_THRESHOLD}",
        "--modified",
        "--languages=cpp",
    ]

    # Add exclude patterns — each as a separate --exclude argument.
    # Passing them here (not via CMake list) prevents CMake glob expansion.
    for pattern in EXCLUDE_PATTERNS:
        cmd.extend(["--exclude", pattern])

    # Output format and file
    cmd.append(output_format_flag)
    cmd.extend(["--output_file", report_file])

    # Paths to analyse (relative to project root)
    for path in ANALYSE_PATHS:
        abs_path = os.path.join(root, path)
        if os.path.exists(abs_path):
            cmd.append(abs_path)

    print("[run_lizard.py] Running Lizard analysis...")
    print(f"[run_lizard.py] Report: {report_file}")

    result = subprocess.run(cmd, cwd=root, check=False)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())

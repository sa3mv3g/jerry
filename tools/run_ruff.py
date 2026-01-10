"""
Script to run Ruff.
"""

import os
import subprocess
import sys


def main():
    """
    Main function to run ruff.
    """
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    config_file = os.path.join(root_dir, "config", "ruff.toml")
    target = os.path.join(root_dir, "tools")

    # Check
    print(f"Running Ruff Check on {target}...")
    cmd_check = [
        sys.executable,
        "-m",
        "ruff",
        "check",
        f"--config={config_file}",
        target,
    ]
    subprocess.call(cmd_check)

    # Format
    print(f"Running Ruff Format on {target}...")
    cmd_fmt = [
        sys.executable,
        "-m",
        "ruff",
        "format",
        f"--config={config_file}",
        target,
    ]
    subprocess.call(cmd_fmt)


if __name__ == "__main__":
    main()

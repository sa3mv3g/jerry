# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.

"""
Script to run Lizard complexity analysis.
"""

import os
import subprocess
import sys


def main():
    """
    Main function to run lizard.
    """
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    config_file = os.path.join(root_dir, "config", "lizard_config.txt")

    args = []
    if os.path.exists(config_file):
        with open(config_file, encoding="utf-8") as f:
            for raw_line in f:
                line = raw_line.strip()
                if line and not line.startswith("#"):
                    # Split arguments just in case multiple args per line
                    args.extend(line.split())

    # Command: run module lizard
    cmd = [sys.executable, "-m", "lizard", *args]

    # Target directory
    cmd.append(os.path.join(root_dir, "application"))

    print("Running Lizard on application...")
    try:
        subprocess.check_call(cmd)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)


if __name__ == "__main__":
    main()

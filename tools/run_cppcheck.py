# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.

"""
Script to run CppCheck static analysis on the application code.
"""

import os
import subprocess
import sys


def main():
    """
    Main function to run cppcheck.
    """
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    opts_file = os.path.join(root_dir, "config", "cppcheck_opts.txt")

    args = []
    if os.path.exists(opts_file):
        with open(opts_file, encoding="utf-8") as f:
            for raw_line in f:
                line = raw_line.strip()
                if line and not line.startswith("#"):
                    args.append(line)

    # Base command
    cmd = ["cppcheck", *args]

    # Add include directories (optional, but good practice if not in opts)
    cmd.append(f"-I{os.path.join(root_dir, 'application', 'inc')}")

    # Add source directories
    cmd.append(os.path.join(root_dir, "application", "src"))

    print("Running CppCheck on application/src...")
    try:
        subprocess.check_call(cmd)
    except FileNotFoundError:
        print(
            "Error: 'cppcheck' executable not found. "
            "Please install cppcheck and ensure it is in your PATH."
        )
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)


if __name__ == "__main__":
    main()

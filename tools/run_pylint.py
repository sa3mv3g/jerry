"""
Script to run Pylint.
"""

import os
import subprocess
import sys


def main():
    """
    Main function to run pylint.
    """
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    rc_file = os.path.join(root_dir, "config", "pylintrc")

    # Find all python files in tools/
    # Or just pass the directory if it's a package
    target = os.path.join(root_dir, "tools")

    cmd = [sys.executable, "-m", "pylint", f"--rcfile={rc_file}", target]

    print(f"Running Pylint on {target}...")
    try:
        subprocess.check_call(cmd)
    except subprocess.CalledProcessError as e:
        # Pylint returns non-zero on issues
        sys.exit(e.returncode)


if __name__ == "__main__":
    main()

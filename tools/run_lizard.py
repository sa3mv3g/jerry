import os
import subprocess
import sys


def main():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    config_file = os.path.join(root_dir, "config", "lizard_config.txt")

    args = []
    if os.path.exists(config_file):
        with open(config_file, "r") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    # Split arguments just in case multiple args per line
                    args.extend(line.split())

    # Command: run module lizard
    cmd = [sys.executable, "-m", "lizard"] + args

    # Target directory
    cmd.append(os.path.join(root_dir, "application"))

    print(f"Running Lizard on application...")
    try:
        subprocess.check_call(cmd)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)


if __name__ == "__main__":
    main()

import os
import subprocess
import glob
import sys


def main():
    # Determine project root (assuming script is in tools/)
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    config_file = os.path.join(root_dir, "config", ".clang-format")

    # Files to ignore/exclude if any

    # Search patterns
    patterns = [
        os.path.join(root_dir, "application", "src", "**", "*.[ch]"),
        os.path.join(root_dir, "application", "inc", "**", "*.[ch]"),
        os.path.join(root_dir, "application", "src", "**", "*.s"),
    ]

    files = []
    for pattern in patterns:
        files.extend(glob.glob(pattern, recursive=True))

    if not files:
        print("No files found to format.")
        return

    # Batch processing to avoid command line length limits on Windows
    # cmd.exe has 8191 char limit
    MAX_CMD_LEN = 7000

    batch = []
    current_len = 0
    base_cmd = ["clang-format", "-i", f"-style=file:{config_file}"]
    base_len = sum(len(arg) + 1 for arg in base_cmd)  # +1 for space

    for f in files:
        # Normalize path
        f = os.path.normpath(f)
        f_len = len(f) + 1

        if current_len + f_len + base_len > MAX_CMD_LEN:
            # Flush batch
            print(f"Formatting batch of {len(batch)} files...")
            subprocess.check_call(base_cmd + batch)
            batch = []
            current_len = 0

        batch.append(f)
        current_len += f_len

    if batch:
        print(f"Formatting batch of {len(batch)} files...")
        subprocess.check_call(base_cmd + batch)


if __name__ == "__main__":
    main()

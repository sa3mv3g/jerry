# Copyright (c) 2026
# All rights reserved.

"""
Script to format C/C++ source code using clang-format.
"""

import glob
import os
import subprocess

# Batch processing to avoid command line length limits on Windows
# cmd.exe has 8191 char limit
MAX_CMD_LEN = 7000


def main():
    """
    Main function to run clang-format on project files.
    """
    # Determine project root (assuming script is in tools/)
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    config_file = os.path.join(root_dir, "config", ".clang-format")

    # Secure BSP FOTA files — listed explicitly to avoid formatting
    # CubeMX-generated headers (partition_stm32h563xx.h, stm32h5xx_hal_conf.h, etc.)
    # which use preprocessor macro patterns that clang-format breaks.
    secure_bsp = os.path.join(
        root_dir, "application", "bsp", "stm", "stm32h563"
    )
    exclusive_files_list: list[str] = [
        os.path.join(root_dir, "application/bsp/stm/bsp.c"),
        os.path.join(root_dir, "application/bsp/bsp.h"),
        # FOTA crypto (Secure firmware)
        os.path.join(secure_bsp, "Secure", "Core", "Inc", "fota_crypto.h"),
        os.path.join(secure_bsp, "Secure", "Core", "Inc", "user_settings.h"),
        os.path.join(secure_bsp, "Secure", "Core", "Src", "fota_crypto.c"),
        os.path.join(secure_bsp, "Secure", "Core", "Src", "secure_nsc.c"),
        # NSC shared header
        os.path.join(secure_bsp, "Secure_nsclib", "secure_nsc.h"),
    ]

    # Files to ignore/exclude if any
    # Search patterns
    patterns = [
        os.path.join(root_dir, "application", "src", "**", "*.[ch]"),
        os.path.join(root_dir, "application", "inc", "**", "*.[ch]"),
        os.path.join(
            root_dir, "application", "bsp", "stm", "bsp_i2c", "**", "*.[ch]"
        ),
        os.path.join(
            root_dir, "application", "src", "generated", "**", "*.[ch]"
        ),
        os.path.join(
            root_dir, "application", "bsp", "stm", "bsp_SinePwm", "**", "*.[ch]"
        ),
        os.path.join(
            root_dir,
            "application",
            "dependencies",
            "modbus",
            "src",
            "**",
            "*.[ch]",
        ),
        os.path.join(
            root_dir,
            "application",
            "dependencies",
            "modbus",
            "inc",
            "**",
            "*.[ch]",
        ),
        os.path.join(
            root_dir,
            "application",
            "dependencies",
            "lwip",
            "port",
            "stm32h5",
            "**",
            "*.[ch]",
        ),
        os.path.join(
            root_dir,
            "application",
            "dependencies",
            "lwip",
            "port",
            "stm32h5",
            "arch",
            "**",
            "*.[ch]",
        ),
        os.path.join(
            root_dir,
            "application",
            "dependencies",
            "adc_filter",
            "src",
            "**",
            "*.[ch]",
        ),
        os.path.join(
            root_dir,
            "application",
            "dependencies",
            "adc_filter",
            "inc",
            "**",
            "*.[ch]",
        ),
        os.path.join(
            root_dir,
            "application",
            "dependencies",
            "lcd_i2c",
            "src",
            "**",
            "*.[ch]",
        ),
        os.path.join(
            root_dir,
            "application",
            "dependencies",
            "lcd_i2c",
            "inc",
            "**",
            "*.[ch]",
        ),
    ]

    files = []
    files.extend(exclusive_files_list)
    for pattern in patterns:
        files.extend(glob.glob(pattern, recursive=True))

    if not files:
        print("No files found to format.")
        return

    batch = []
    current_len = 0
    base_cmd = ["clang-format", "-i", f"-style=file:{config_file}"]
    base_len = sum(len(arg) + 1 for arg in base_cmd)  # +1 for space

    for filepath in files:
        # Normalize path
        norm_filepath = os.path.normpath(filepath)
        f_len = len(norm_filepath) + 1

        if current_len + f_len + base_len > MAX_CMD_LEN:
            # Flush batch
            print(f"Formatting batch of {len(batch)} files...")
            subprocess.check_call(base_cmd + batch)
            batch = []
            current_len = 0

        batch.append(norm_filepath)
        current_len += f_len

    if batch:
        print(f"Formatting batch of {len(batch)} files...")
        subprocess.check_call(base_cmd + batch)


if __name__ == "__main__":
    main()

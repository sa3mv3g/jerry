"""
MX_Device_gen.py

Generates MX_Device.h and RTE_Components.h from STM32CubeMX .ioc file and
generated source files.
This script parses the .ioc file to identify used peripherals and
configurations, and extracts pin mappings and other details from the generated
C files (main.c, stm32xxx_hal_msp.c).
"""

# pylint: disable=invalid-name

import sys
import argparse
import datetime
import re
import logging
from pathlib import Path
from typing import Dict, List, Optional, Any, TextIO

# Constants
PERIPHERALS = [
    "USART", "UART", "LPUART", "SPI", "I2C", "ETH", "SDMMC", "CAN", "USB",
    "SDIO", "FDCAN"
]

CONTEXT_MAP = {
    "CortexM33S": "Secure",
    "CortexM33NS": "NonSecure",
    "CortexM4": "CM4",
    "CortexM7": "CM7",
}

RTE_MAPPING = {
    "ETH": "RTE_Drivers_ETH_MAC0",
    "I2C": "RTE_Drivers_I2C{}",
    "SPI": "RTE_Drivers_SPI{}",
    "USART": "RTE_CMSIS_Driver_USART{}",
    "UART": "RTE_CMSIS_Driver_USART{}",
    "SDMMC": "RTE_Drivers_MCI{}",
    "SDIO": "RTE_Drivers_MCI{}",
    "USB": {
        "Device": "RTE_Drivers_USBD{}",
        "Host": "RTE_Drivers_USBH{}",
        "OTG": "RTE_Drivers_USBH{}"
    }
}


def setup_logging(log_file: str = "mx_device_gen.log"):
    """
    Sets up logging to file and console.

    Args:
        log_file (str): Path to the log file.
    """
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        handlers=[
            logging.FileHandler(log_file),
            logging.StreamHandler(sys.stdout)
        ]
    )


class IOCFile:
    """
    Parses and provides access to .ioc file content.

    Attributes:
        path (Path): Path to the .ioc file.
        lines (List[str]): Lines of the file.
        config (Dict[str, str]): Key-value pairs parsed from the file.
    """

    def __init__(self, path: Path):
        """
        Initialize the IOCFile parser.

        Args:
            path (Path): Path to the .ioc file.

        Raises:
            SystemExit: If the file is not found.
        """
        self.path = path
        try:
            with open(path, 'r', encoding='utf-8', errors='replace') as f:
                self.lines = [line.strip() for line in f.readlines()]
        except FileNotFoundError:
            logging.error("File not found: %s", path)
            sys.exit(1)

        self.config: Dict[str, str] = {}
        for line in self.lines:
            if '=' in line:
                parts = line.split('=', 1)
                self.config[parts[0].strip()] = parts[1].strip()

    def get_lines(self) -> List[str]:
        """
        Return all lines in the file.

        Returns:
            List[str]: List of strings representing lines in the file.
        """
        return self.lines

    def get_value(self, key: str) -> Optional[str]:
        """
        Get the value for a specific key from the parsed configuration.

        Args:
            key (str): The configuration key to look up.

        Returns:
            Optional[str]: The value associated with the key, or None if not found.
        """
        return self.config.get(key)


def get_digit_at_end(text: str) -> str:
    """
    Returns the sequence of digits at the end of the string.

    Args:
        text (str): The input string.

    Returns:
        str: The sequence of digits found at the end, or an empty string.
    """
    match = re.search(r'\d+$', text)
    return match.group(0) if match else ""


def replace_special_chars(text: str, ch: str) -> str:
    """
    Replaces special characters in text with the specified character.

    Non-alphanumeric characters are replaced. Consecutive replacement characters
    are collapsed into a single character.

    Args:
        text (str): The input text.
        ch (str): The character to use for replacement.

    Returns:
        str: The sanitized text.
    """
    # Replace non-alphanumeric characters with ch
    out_text = re.sub(r'[^a-zA-Z0-9]', ch, text)
    # Replace multiple occurrences of ch with a single ch
    out_text = re.sub(f'{re.escape(ch)}+', ch, out_text)
    return out_text.strip(ch)


class MXDeviceGenerator:
    """
    Main class for generating MX_Device.h.

    Orchestrates the parsing of IOC and C files to generate the header file
    containing macro definitions for the board configuration.

    Attributes:
        ioc_path (Path): Path to the .ioc file.
        work_dir (Path): Working directory containing the .ioc file.
        ioc (IOCFile): Parsed IOC file object.
    """

    def __init__(self, ioc_path: Path):
        """
        Initialize the generator.

        Args:
            ioc_path (Path): Path to the .ioc file.
        """
        self.ioc_path = ioc_path.resolve()
        self.work_dir = self.ioc_path.parent
        self.ioc = IOCFile(self.ioc_path)

    def get_contexts(self) -> List[str]:
        """
        Get the list of contexts from the IOC file.

        Returns:
            List[str]: A list of context names (e.g., 'CortexM33S').
        """
        contexts = []
        for line in self.ioc.get_lines():
            if line.startswith("Mcu.Context"):
                parts = line.split('=')
                if parts[0][-1].isdigit():  # Mcu.Context0, Mcu.Context1...
                    contexts.append(parts[1])
        return contexts

    def get_main_location(self) -> str:
        """
        Get the main location path from the IOC file.

        Returns:
            str: The relative path to the main source directory.
        """
        for line in self.ioc.get_lines():
            if line.startswith("ProjectManager.MainLocation="):
                return str(Path(line.split('=')[1]))
        return ""

    def get_device_family(self) -> str:
        """
        Get the device family from the IOC file.

        Returns:
            str: The device family name (e.g., 'STM32H5').
        """
        for line in self.ioc.get_lines():
            if line.startswith("Mcu.Family=STM32"):
                return line.split('=')[1]
        return ""

    def get_peripherals(self, context: Optional[str] = None) -> List[str]:
        """
        Get the list of peripherals used in the specified context.

        Args:
            context (Optional[str]): The context name to filter by
                                     (e.g., 'CortexM33S'). If None, all
                                     peripherals are checked.

        Returns:
            List[str]: A list of peripheral instance names (e.g., 'USART1').
        """
        peripherals = []
        context_ips_line = ""

        if context:
            for line in self.ioc.get_lines():
                if line.startswith(f"{context}.IPs"):
                    context_ips_line = line
                    break

        if context and not context_ips_line:
            return peripherals

        for line in self.ioc.get_lines():
            if not line.startswith("Mcu.IP"):
                continue

            try:
                peripheral = line.split('=')[1]
            except IndexError:
                continue

            if context and peripheral not in context_ips_line:
                continue

            is_supported = any(peripheral.startswith(p) for p in PERIPHERALS)
            if is_supported and peripheral not in peripherals:
                peripherals.append(peripheral)

        return peripherals

    def get_virtual_mode(self, peripheral: str) -> str:
        """
        Get the virtual mode for a peripheral.

        Args:
            peripheral (str): The peripheral instance name.

        Returns:
            str: The virtual mode string, or empty string if not found.
        """
        find_phrase = f"{peripheral}.VirtualMode"
        for line in self.ioc.get_lines():
            if line.startswith(find_phrase):
                return line.split("=")[1]
        return ""

    def get_i2c_info(self, main_path: Path, peripheral: str) -> Dict[str, str]:
        """
        Get I2C configuration info from main.c.

        Scans the `main.c` file for I2C initialization code to extract
        filter settings.

        Args:
            main_path (Path): Path to the main.c file.
            peripheral (str): The peripheral instance name.

        Returns:
            Dict[str, str]: A dictionary containing I2C settings
                            (e.g., 'ANF_ENABLE', 'DNF').
        """
        info = {}
        if not peripheral.startswith("I2C"):
            return info

        if not main_path.exists():
            logging.warning("%s not found.", main_path)
            return info

        try:
            with open(main_path, 'r', encoding='utf-8', errors='replace') as f:
                lines = f.readlines()

            section_found = False
            for line in lines:
                if not section_found:
                    if (line.strip().startswith(f"static void MX_{peripheral}_Init")
                            and ";" not in line):
                        section_found = True
                    continue

                if line.strip().startswith("}"):
                    break

                if "HAL_I2CEx_ConfigAnalogFilter" in line:
                    info["ANF_ENABLE"] = (
                        "1" if "I2C_ANALOGFILTER_ENABLE" in line else "0"
                    )

                if "HAL_I2CEx_ConfigDigitalFilter" in line:
                    match = re.search(
                        r'ConfigDigitalFilter\s*\([^,]+,\s*([^)]+)\)',
                        line
                    )
                    if match:
                        info["DNF"] = match.group(1).strip()

        except (OSError, IOError) as e:
            logging.error("Error parsing I2C info: %s", e)
            sys.exit(1)

        return info

    def get_pin_configuration(
        self, msp_path: Path, peripheral: str, pin: str, label: str
    ) -> Dict[str, str]:
        """
        Get configuration for a specific pin from the MSP file.

        Parses `stm32xxx_hal_msp.c` to find GPIO initialization code for a
        specific peripheral pin.

        Args:
            msp_path (Path): Path to the MSP C file.
            peripheral (str): The peripheral instance name.
            pin (str): The pin name (e.g., 'PA10').
            label (str): The GPIO label given in CubeMX.

        Returns:
            Dict[str, str]: A dictionary of pin configuration
                            (Pin, Port, Mode, etc.).
        """
        info: Dict[str, str] = {
            "Pin": "", "Port": "", "Mode": "",
            "Pull": "", "Speed": "", "Alternate": ""
        }

        if not msp_path.exists():
            return info

        try:
            with open(msp_path, 'r', encoding='utf-8', errors='replace') as f:
                lines = f.readlines()
        except (OSError, IOError) as e:
            logging.error("Error parsing pin config: %s", e)
            sys.exit(1)

        pin_num = get_digit_at_end(pin)
        gpio_pin = f"GPIO_PIN_{pin_num}"
        # pin is like PA11 -> Port A
        match = re.search(r'P([A-Z])', pin)
        if not match:
            return info
        port_letter = match.group(1)
        gpio_port = f"GPIO{port_letter}"

        valid_section = False
        value_in_additional_line = False
        current_key = ""
        current_value_acc = ""

        for line in lines:
            line = line.strip()

            # Check section
            if "->Instance" in line:
                valid_section = peripheral in line

            if not valid_section:
                continue

            if "HAL_GPIO_Init" in line:
                port_match = (
                    gpio_port in line or
                    (label and f"{label}_GPIO_Port" in line)
                )
                if port_match:
                    # Check if the captured Pin matches this pin
                    values = [v.strip() for v in info["Pin"].split("|")]
                    is_pin_match = (
                        gpio_pin in values or
                        (label and f"{label}_Pin" in values)
                    )

                    if is_pin_match:
                        info["Pin"] = gpio_pin
                        info["Port"] = gpio_port
                        return info

            # Parse struct members
            if value_in_additional_line:
                current_value_acc += line
                if ";" in current_value_acc:
                    info[current_key] = current_value_acc.split(";")[0].strip()
                    value_in_additional_line = False
                    current_value_acc = ""
            else:
                for key in info:
                    # Search for ".Pin =", ".Mode =" etc.
                    if f".{key}" in line and "=" in line:
                        parts = line.split("=")
                        value = parts[1].strip()
                        if ";" in value:
                            info[key] = value.split(";")[0].strip()
                        else:
                            value_in_additional_line = True
                            current_key = key
                            current_value_acc = value

        return {}  # Empty if not found

    def get_pins(self, msp_path: Path, peripheral: str) -> Dict[str, Any]:
        """
        Get all pins associated with a peripheral.

        Args:
            msp_path (Path): Path to the MSP C file.
            peripheral (str): The peripheral instance name.

        Returns:
            Dict[str, Any]: A dictionary where keys are pin signals
                            (e.g. 'USART1_TX') and values are lists
                            [pin_name, configuration_dict].
        """
        pins_name = {}
        pins_label = {}
        pins_info = {}

        # Find signals
        find_phrase = f".Signal={peripheral}"
        for line in self.ioc.get_lines():
            if find_phrase in line and not line.startswith("VP_"):
                # PA11.Signal=USART1_TX
                parts = line.split(".Signal=")
                pins_name[parts[0].strip()] = parts[1].strip()

        # Find labels
        for line in self.ioc.get_lines():
            for pin in pins_name:
                if f"{pin}.GPIO_Label" in line:
                    value = line.split("=")[1].strip().split("[")[0].strip()
                    value = replace_special_chars(value, "_")
                    pins_label[pin] = value

        # Get detailed info
        for pin_key, pin_signal in pins_name.items():
            # Clean pin name
            p = (
                pin_key.split("\\")[0]
                .split("(")[0]
                .split(" ")[0]
                .split("_")[0]
                .split("-")[0]
            )

            label = pins_label.get(pin_key, "")
            info = self.get_pin_configuration(msp_path, peripheral, p, label)

            # If info is empty, provide a default empty dict structure
            if not info:
                info = {
                    "Pin": "", "Port": "", "Mode": "",
                    "Pull": "", "Speed": "", "Alternate": ""
                }

            pins_info[pin_signal] = [p, info]

        return pins_info

    def get_rte_define(self, peripheral: str, virtual_mode: str) -> Optional[str]:
        """
        Get the RTE define for a peripheral.

        Args:
            peripheral (str): Peripheral name (e.g., I2C1, USART2).
            virtual_mode (str): Virtual mode (e.g., Host_Only for USB).

        Returns:
            str: RTE define macro (e.g., RTE_Drivers_I2C1).
        """
        # Identify peripheral type and index by checking startswith against PERIPHERALS
        name = ""
        index = ""
        for p in PERIPHERALS:
            if peripheral.startswith(p):
                name = p
                index = peripheral[len(p):]
                break

        if not name:
            return None

            if name == "LPUART":
                try:
                    idx_val = int(index)
                    usart_idx = 20 + idx_val
                    return f"RTE_CMSIS_Driver_USART{usart_idx}"
                except ValueError:
                    return None

        if name not in RTE_MAPPING:
            return None

        mapping = RTE_MAPPING[name]

        if isinstance(mapping, dict):
            if name == "USB":
                if "Device" in virtual_mode:
                    return mapping["Device"].format(index if index else "0")
                if "Host" in virtual_mode:
                    return mapping["Host"].format(index if index else "0")
                return mapping["Host"].format(index if index else "0")
            return None

        if name == "ETH":
            return mapping

        return mapping.format(index)

    def create_define(self, name: str, value: str) -> str:
        """
        Create a C define statement string.

        Sanitizes the name by replacing invalid characters with underscores.

        Args:
            name (str): The name for the define (suffix).
            value (str): The value for the define.

        Returns:
            str: The formatted #define statement.
        """
        invalid_chars = ['=', ' ', '/', '(', ')', '[', ']', '\\', '-']
        for ch in invalid_chars:
            name = name.replace(ch, '_')
            value = value.replace(ch, '_')

        name = f"MX_{name}".ljust(39)
        return f"#define {name}{value}"

    def write_header(self, f: TextIO, file_name: str):
        """
        Write the header file preamble/boilerplate.

        Args:
            f (TextIO): The file object to write to.
            file_name (str): The name of the file being generated.
        """
        now = datetime.datetime.now()
        dt_string = now.strftime("%d/%m/%Y %H:%M:%S")
        f.write(f"""/******************************************************************************
 * File Name   : {file_name}
 * Date        : {dt_string}
 * Description : STM32Cube MX parameter definitions
 * Note        : This file is generated with a generator out of the
 *               STM32CubeMX project and its generated files (DO NOT EDIT!)
 ******************************************************************************/

#ifndef MX_DEVICE_H
#define __MX_DEVICE_H

""")

    def write_peripheral_cfg(
        self, f: TextIO, peripheral: str, vmode: str,
        i2c_info: Dict[str, str], pins: Dict[str, Any]
    ):
        """
        Write configuration defines for a specific peripheral to the file.

        Args:
            f (TextIO): The file object to write to.
            peripheral (str): The peripheral name.
            vmode (str): Virtual mode string.
            i2c_info (Dict[str, str]): I2C specific info.
            pins (Dict[str, Any]): Dictionary of pin configurations.
        """
        pin_define_name = {
            "Pin": "GPIO_Pin",
            "Port": "GPIOx",
            "Mode": "GPIO_Mode",
            "Pull": "GPIO_PuPd",
            "Speed": "GPIO_Speed",
            "Alternate": "GPIO_AF"
        }

        f.write(
            f"\n/*------------------------------ {peripheral}".ljust(52) +
            "-----------------------------*/\n"
        )
        f.write(self.create_define(peripheral, "1") + "\n\n")

        if i2c_info:
            f.write("/* Filter Settings */\n")
            for item, val in i2c_info.items():
                f.write(
                    self.create_define(f"{peripheral}_{item}", val) + "\n"
                )
            f.write("\n")

        if vmode:
            f.write("/* Virtual mode */\n")
            f.write(self.create_define(f"{peripheral}_VM", vmode) + "\n")
            f.write(
                self.create_define(f"{peripheral}_{vmode}", "1") + "\n\n"
            )

        if pins:
            f.write("/* Pins */\n")
            for pin_func, pin_data in pins.items():
                # pin_data is [p, info]
                real_pin = pin_data[0]
                info = pin_data[1]

                f.write(f"\n/* {pin_func} */\n")
                f.write(
                    self.create_define(f"{pin_func}_Pin", real_pin) + "\n"
                )

                for key, suffix in pin_define_name.items():
                    val = info.get(key, "")
                    if val:
                        f.write(
                            self.create_define(f"{pin_func}_{suffix}", val) + "\n"
                        )

    def write_rte_components(self, f: TextIO, peripherals: List[str]):
        """
        Write RTE_Components.h content.

        Args:
            f (TextIO): The file object to write to.
            peripherals (List[str]): List of peripheral names.
        """
        f.write("\n/*\n * Auto generated Run-Time-Environment Component Configuration File\n")
        f.write(" *      *** Do not modify ! ***\n")
        f.write(f" * Target:  '{self.get_device_family()}'\n")
        f.write(" */\n\n")
        f.write("#ifndef RTE_COMPONENTS_H\n")
        f.write("#define RTE_COMPONENTS_H\n\n")

        f.write("/*\n * Define the Device Header File:\n */\n")
        family = self.get_device_family().lower()
        header = f"{family}xx.h"
        f.write(f"#define CMSIS_device_header \"{header}\"\n\n")

        for peripheral in peripherals:
            vmode = self.get_virtual_mode(peripheral)
            define = self.get_rte_define(peripheral, vmode)
            if define:
                f.write(f"#define {define}              /* Driver {peripheral} */\n")

        f.write("\n#endif /* RTE_COMPONENTS_H */\n")


    def get_user_defines(self):
        retval = {}
        for line in self.ioc.get_lines():
            line = line.strip()
            key_val = line.split("=")
            if len(key_val) >= 2:
                key = key_val[0]
                val = key_val[1]
                if key == "Mcu.UserConstants":
                    defs = val.split(";")
                    for userdefconf in defs:
                        a = userdefconf.split(",")
                        if len(a) >= 2:
                            define_value = a[1]
                            b = a[0].split("-")
                            if len(b) >= 2:
                                define_name = b[0]
                                define_ctx = b[1]
                                if define_ctx not in retval:
                                    retval[define_ctx] = []    
                                retval[define_ctx].append({'name': define_name, 'value': define_value})
        return retval

    def run(self):
        """
        Execute the generation process.

        Reads the IOC file, identifies contexts and paths, and generates the output
        MX_Device.h files for each context.
        """
        logging.info("Starting MX_Device generator...")
        logging.info("Working directory: %s", Path.cwd())
        logging.info("Processing IOC file: %s", self.ioc_path)

        contexts = self.get_contexts()
        logging.info("Found contexts: %s", contexts)

        device_family = self.get_device_family().lower()
        logging.info("Device family: %s", device_family)

        msp_name = f"{device_family}xx_hal_msp.c"
        project_index = 1
        configs = []

        if contexts:
            for context in contexts:
                logging.info("Analyzing context: %s", context)
                context_folder = CONTEXT_MAP.get(context)
                if not context_folder:
                    logging.warning("Unknown context %s", context)
                    continue

                main_loc = self.get_main_location()
                main_folder = self.work_dir / context_folder / main_loc
                logging.info("Main folder for context %s: %s",
                             context, main_folder)

                configs.append({
                    "main_path": main_folder / "main.c",
                    "msp_path": main_folder / msp_name,
                    "context": context,
                    "index": project_index
                })
                project_index += 1
        else:
            logging.info("No contexts found, assuming single context.")
            main_loc = self.get_main_location()
            main_folder = self.work_dir / main_loc
            logging.info("Main folder: %s", main_folder)

            configs.append({
                "main_path": main_folder / "main.c",
                "msp_path": main_folder / msp_name,
                "context": None,
                "index": project_index
            })

        for cfg in configs:
            main_path = cfg["main_path"]
            msp_path = cfg["msp_path"]
            context = cfg["context"]
            idx = cfg["index"]

            logging.info("Configuration %d: Context=%s", idx, context)
            logging.info("  Main path: %s", main_path)
            logging.info("  MSP path: %s", msp_path)

            drv_cfg_dir = Path(main_path).parent.parent
            output_dir = drv_cfg_dir / "inc"
            if not output_dir.exists():
                output_dir.mkdir(parents=True, exist_ok=True)
                logging.info("Created output directory: %s", output_dir)

            output_file = output_dir / "MX_Device.h"
            rte_file = output_dir / "RTE_Components.h"

            logging.info("Generating %s...", output_file)

            try:
                with open(output_file, "w", encoding="utf-8") as f_out:
                    self.write_header(f_out, "MX_Device.h")

                    f_out.write("#define MX_DEVICE_VERSION 0x01000000\r\n")

                    peripherals = self.get_peripherals(context)
                    logging.info("  Found %d peripherals: %s",
                                 len(peripherals), peripherals)

                    # Generate RTE_Components.h
                    logging.info("Generating %s...", rte_file)
                    with open(rte_file, "w", encoding="utf-8") as f_rte:
                        self.write_rte_components(f_rte, peripherals)
                    userdefines = self.get_user_defines()
                    if context in userdefines:
                        users_define_list = userdefines[context]
                        
                        for users_define in users_define_list:
                            f_out.write(f"#define {users_define['name']} {users_define['value']}\r\n")
                            logging.info("  User define: #define %s %s", users_define['name'], users_define['value'])

                    for peripheral in peripherals:
                        logging.info("  Processing peripheral: %s", peripheral)
                        vmode = self.get_virtual_mode(peripheral)
                        if vmode:
                            logging.info("    Virtual mode: %s", vmode)

                        i2c_info = self.get_i2c_info(main_path, peripheral)
                        if i2c_info:
                            logging.info("    I2C info found: %s", i2c_info)

                        pins = self.get_pins(msp_path, peripheral)
                        if pins:
                            logging.info("    Found %d pins for %s",
                                         len(pins), peripheral)
                        else:
                            logging.info("    No pins found for %s",
                                         peripheral)

                        self.write_peripheral_cfg(
                            f_out, peripheral, vmode, i2c_info, pins
                        )

                    f_out.write("\n#endif  /* __MX_DEVICE_H */\n")
                    logging.info("Successfully generated %s", output_file)
                    logging.info("Successfully generated %s", rte_file)

            except (OSError, IOError) as e:
                logging.error("Error writing output: %s", e)
                sys.exit(1)


def validate_file(file_path: str) -> Path:
    """
    Argparse validator for file existence.

    Args:
        file_path (str): Path to the file.

    Returns:
        Path: Resolved path object.

    Raises:
        argparse.ArgumentTypeError: If file does not exist.
    """
    path = Path(file_path)
    if not path.is_file():
        raise argparse.ArgumentTypeError(f"Invalid file: {file_path}!")
    return path


def main():
    """Entry point."""
    parser = argparse.ArgumentParser(description="MX_Device_generator")
    parser.add_argument("ioc", metavar="<STM32CubeMX project file>",
                        help="<*.ioc>", type=validate_file)
    parser.add_argument("--logfile", help="Path to the log file",
                        default="mx_device_gen.log")

    args = parser.parse_args()
    setup_logging(args.logfile)

    generator = MXDeviceGenerator(args.ioc)
    generator.run()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        logging.info("\nInterrupted.")
        sys.exit(0)

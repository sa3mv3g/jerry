#!/usr/bin/env python3
"""
Modbus Register Code Generator

This tool generates C header and source files from a JSON register definition file.
It uses Jinja2 templates for flexible code generation.

Note: Callback implementations are maintained manually in the application source
directory to allow custom hardware logic.

Usage:
    python modbus_codegen.py <config.json> [--output-dir <dir>]

Copyright (c) 2026
"""

import argparse
import json
import logging
import sys
from pathlib import Path
from typing import Any

try:
    from jinja2 import Environment, FileSystemLoader, select_autoescape
except ImportError as e:
    print(f"Error: Missing required package: {e}")
    print("Install with: pip install jinja2")
    sys.exit(1)

try:
    import jsonschema
    HAS_JSONSCHEMA = True
except ImportError:
    HAS_JSONSCHEMA = False

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(levelname)s: %(message)s"
)
logger = logging.getLogger(__name__)


class ModbusCodeGenerator:
    """Generates C code from Modbus register JSON configuration."""

    def __init__(self, template_dir: Path | None = None) -> None:
        """Initialize the code generator.

        Args:
            template_dir: Directory containing Jinja2 templates.
                         Defaults to 'templates' subdirectory.
        """
        if template_dir is None:
            template_dir = Path(__file__).parent / "templates"

        self.template_dir = template_dir
        self.env = Environment(
            loader=FileSystemLoader(str(template_dir)),
            autoescape=select_autoescape(default=False),
            trim_blocks=True,
            lstrip_blocks=True,
            keep_trailing_newline=True,
        )

        # Add custom filters
        self.env.filters["upper_snake"] = self._to_upper_snake_case
        self.env.filters["camel_case"] = self._to_camel_case

    @staticmethod
    def _to_upper_snake_case(name: str) -> str:
        """Convert name to UPPER_SNAKE_CASE."""
        result = []
        for i, char in enumerate(name):
            if char.isupper() and i > 0 and name[i - 1].islower():
                result.append("_")
            result.append(char.upper())
        return "".join(result)

    @staticmethod
    def _to_camel_case(name: str) -> str:
        """Convert name to CamelCase."""
        parts = name.split("_")
        return "".join(part.capitalize() for part in parts)

    def load_config(self, config_path: Path) -> dict[str, Any]:
        """Load and validate JSON configuration.

        Args:
            config_path: Path to JSON configuration file.

        Returns:
            Validated configuration dictionary.

        Raises:
            FileNotFoundError: If config file doesn't exist.
            json.JSONDecodeError: If config is invalid JSON.
            jsonschema.ValidationError: If config doesn't match schema.
        """
        logger.info("Loading configuration from %s", config_path)

        with open(config_path, encoding="utf-8") as f:
            config = json.load(f)

        # Load and validate against schema
        schema_path = Path(__file__).parent / "schema" / "modbus_registers.schema.json"
        if schema_path.exists() and HAS_JSONSCHEMA:
            with open(schema_path, encoding="utf-8") as f:
                schema = json.load(f)
            jsonschema.validate(config, schema)
            logger.info("Configuration validated successfully")
        elif not HAS_JSONSCHEMA:
            logger.warning("jsonschema not installed, skipping validation")
        else:
            logger.warning("Schema file not found, skipping validation")

        return config

    def _preprocess_config(self, config: dict[str, Any]) -> dict[str, Any]:
        """Preprocess configuration for template rendering.

        Adds computed fields and organizes data for easier template access.

        Args:
            config: Raw configuration dictionary.

        Returns:
            Preprocessed configuration.
        """
        # Calculate statistics
        registers = config.get("registers", {})
        stats = {
            "num_coils": len(registers.get("coils", [])),
            "num_discrete_inputs": len(registers.get("discrete_inputs", [])),
            "num_holding_registers": len(registers.get("holding_registers", [])),
            "num_input_registers": len(registers.get("input_registers", [])),
        }

        # Add size information for multi-register types first
        for reg in registers.get("holding_registers", []):
            if "size" not in reg:
                reg["size"] = 1
            if "data_type" not in reg:
                reg["data_type"] = "uint16"

        for reg in registers.get("input_registers", []):
            if "size" not in reg:
                reg["size"] = 1
            if "data_type" not in reg:
                reg["data_type"] = "uint16"

        # Calculate address ranges (accounting for multi-register values)
        for reg_type in ["coils", "discrete_inputs", "holding_registers", "input_registers"]:
            regs = registers.get(reg_type, [])
            if regs:
                min_addr = min(r["address"] for r in regs)
                # For max_addr, account for register size (uint32 takes 2 addresses)
                max_addr = max(r["address"] + r.get("size", 1) - 1 for r in regs)
                stats[f"{reg_type}_min_addr"] = min_addr
                stats[f"{reg_type}_max_addr"] = max_addr
            else:
                stats[f"{reg_type}_min_addr"] = 0
                stats[f"{reg_type}_max_addr"] = 0

        config["stats"] = stats
        return config

    def generate_register_documentation(
        self,
        config: dict[str, Any],
        output_path: Path,
    ) -> Path:
        """Generate human-readable documentation of all Modbus registers.

        Creates a text file containing all register information including
        addresses, names, descriptions, data types, and access permissions.

        Args:
            config: Configuration dictionary (preprocessed).
            output_path: Path to write the documentation file.

        Returns:
            Path to the generated documentation file.
        """
        lines = []
        device = config.get("device", {})
        registers = config.get("registers", {})
        groups = {g["name"]: g["description"] for g in config.get("groups", [])}

        # Header (excludes device description to avoid exposing internal info)
        lines.append("=" * 80)
        lines.append(f"MODBUS REGISTER MAP - {device.get('name', 'Unknown Device').upper()}")
        lines.append("=" * 80)
        lines.append("")
        lines.append(f"Slave ID:    {device.get('slave_id', 'N/A')}")
        lines.append(f"Version:     {device.get('version', 'N/A')}")
        lines.append("")
        lines.append("-" * 80)
        lines.append("")

        # Coils (Function Code 01/05/15)
        coils = registers.get("coils", [])
        if coils:
            lines.append("COILS (FC 01 Read / FC 05 Write Single / FC 15 Write Multiple)")
            lines.append("-" * 80)
            lines.append(f"{'Address':<10} {'Name':<30} {'Access':<12} Description")
            lines.append("-" * 80)
            for reg in sorted(coils, key=lambda x: x["address"]):
                addr = reg["address"]
                name = reg["name"]
                access = reg.get("access", "read_write")
                desc = reg.get("description", "")
                lines.append(f"{addr:<10} {name:<30} {access:<12} {desc}")
            lines.append("")

        # Discrete Inputs (Function Code 02)
        discrete_inputs = registers.get("discrete_inputs", [])
        if discrete_inputs:
            lines.append("DISCRETE INPUTS (FC 02 Read Only)")
            lines.append("-" * 80)
            lines.append(f"{'Address':<10} {'Name':<30} {'Group':<15} Description")
            lines.append("-" * 80)
            for reg in sorted(discrete_inputs, key=lambda x: x["address"]):
                addr = reg["address"]
                name = reg["name"]
                group = reg.get("group", "")
                desc = reg.get("description", "")
                lines.append(f"{addr:<10} {name:<30} {group:<15} {desc}")
            lines.append("")

        # Holding Registers (Function Code 03/06/16)
        holding_registers = registers.get("holding_registers", [])
        if holding_registers:
            lines.append("HOLDING REGISTERS (FC 03 Read / FC 06 Write Single / FC 16 Write Multiple)")
            lines.append("-" * 80)
            lines.append(
                f"{'Address':<10} {'Name':<25} {'Type':<10} {'Size':<6} "
                f"{'Access':<12} Description"
            )
            lines.append("-" * 80)
            for reg in sorted(holding_registers, key=lambda x: x["address"]):
                addr = reg["address"]
                name = reg["name"]
                data_type = reg.get("data_type", "uint16")
                size = reg.get("size", 1)
                access = reg.get("access", "read_write")
                desc = reg.get("description", "")
                # Add range info if available
                range_info = ""
                if "min_value" in reg and "max_value" in reg:
                    range_info = f" [Range: {reg['min_value']}-{reg['max_value']}]"
                if "unit" in reg:
                    range_info += f" [{reg['unit']}]"
                lines.append(
                    f"{addr:<10} {name:<25} {data_type:<10} {size:<6} "
                    f"{access:<12} {desc}{range_info}"
                )
            lines.append("")

        # Input Registers (Function Code 04)
        input_registers = registers.get("input_registers", [])
        if input_registers:
            lines.append("INPUT REGISTERS (FC 04 Read Only)")
            lines.append("-" * 80)
            lines.append(
                f"{'Address':<10} {'Name':<25} {'Type':<10} {'Size':<6} Description"
            )
            lines.append("-" * 80)
            for reg in sorted(input_registers, key=lambda x: x["address"]):
                addr = reg["address"]
                name = reg["name"]
                data_type = reg.get("data_type", "uint16")
                size = reg.get("size", 1)
                desc = reg.get("description", "")
                lines.append(
                    f"{addr:<10} {name:<25} {data_type:<10} {size:<6} {desc}"
                )
            lines.append("")

        # Groups summary
        if groups:
            lines.append("REGISTER GROUPS")
            lines.append("-" * 80)
            for group_name, group_desc in groups.items():
                lines.append(f"  {group_name:<20} - {group_desc}")
            lines.append("")

        # Statistics
        stats = config.get("stats", {})
        lines.append("STATISTICS")
        lines.append("-" * 80)
        lines.append(f"  Total Coils:             {stats.get('num_coils', 0)}")
        lines.append(f"  Total Discrete Inputs:   {stats.get('num_discrete_inputs', 0)}")
        lines.append(f"  Total Holding Registers: {stats.get('num_holding_registers', 0)}")
        lines.append(f"  Total Input Registers:   {stats.get('num_input_registers', 0)}")
        lines.append("")
        lines.append("=" * 80)

        # Write to file
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text("\n".join(lines), encoding="utf-8")
        logger.info("Generated: %s", output_path)

        return output_path

    def generate(
        self,
        config: dict[str, Any],
        output_dir: Path,
        doc_output_dir: Path | None = None,
    ) -> list[Path]:
        """Generate C code files from configuration.

        Generates only register header and source files. Callback implementations
        are maintained manually in the application source directory.

        Args:
            config: Configuration dictionary.
            output_dir: Directory to write generated files.
            doc_output_dir: Directory to write documentation files (default: same as output_dir).

        Returns:
            List of generated file paths.
        """
        output_dir.mkdir(parents=True, exist_ok=True)

        config = self._preprocess_config(config)
        device_name = config["device"]["name"].lower()

        generated_files = []

        # Generate header file
        header_template = self.env.get_template("modbus_registers.h.j2")
        header_content = header_template.render(config=config)
        header_path = output_dir / f"{device_name}_registers.h"
        header_path.write_text(header_content, encoding="utf-8")
        generated_files.append(header_path)
        logger.info("Generated: %s", header_path)

        # Generate source file
        source_template = self.env.get_template("modbus_registers.c.j2")
        source_content = source_template.render(config=config)
        source_path = output_dir / f"{device_name}_registers.c"
        source_path.write_text(source_content, encoding="utf-8")
        generated_files.append(source_path)
        logger.info("Generated: %s", source_path)

        # Generate human-readable documentation
        if doc_output_dir is None:
            doc_output_dir = output_dir
        doc_path = doc_output_dir / f"{device_name}_register_map.txt"
        self.generate_register_documentation(config, doc_path)
        generated_files.append(doc_path)

        return generated_files


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Generate C code from Modbus register JSON configuration"
    )
    parser.add_argument(
        "config",
        type=Path,
        help="Path to JSON configuration file"
    )
    parser.add_argument(
        "--output-dir", "-o",
        type=Path,
        default=Path("generated"),
        help="Output directory for generated files (default: generated)"
    )
    parser.add_argument(
        "--doc-output-dir", "-d",
        type=Path,
        default=None,
        help="Output directory for documentation files (default: build)"
    )
    parser.add_argument(
        "--template-dir", "-t",
        type=Path,
        default=None,
        help="Directory containing Jinja2 templates"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable verbose output"
    )

    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    # Default doc output to build folder
    doc_output_dir = args.doc_output_dir
    if doc_output_dir is None:
        doc_output_dir = Path("build")

    try:
        generator = ModbusCodeGenerator(template_dir=args.template_dir)
        config = generator.load_config(args.config)
        generated = generator.generate(config, args.output_dir, doc_output_dir)

        logger.info("Successfully generated %d files", len(generated))
        return 0

    except FileNotFoundError as e:
        logger.error("File not found: %s", e)
        return 1
    except json.JSONDecodeError as e:
        logger.error("Invalid JSON: %s", e)
        return 1
    except Exception as e:
        if HAS_JSONSCHEMA and isinstance(e, jsonschema.ValidationError):
            logger.error("Configuration validation failed: %s", e.message)
            return 1
        raise


if __name__ == "__main__":
    sys.exit(main())

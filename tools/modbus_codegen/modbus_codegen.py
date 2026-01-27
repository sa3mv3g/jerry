#!/usr/bin/env python3
"""
Modbus Register Code Generator

This tool generates C header and source files from a JSON register definition file.
It uses Jinja2 templates for flexible code generation.

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
    import jsonschema
    from jinja2 import Environment, FileSystemLoader, select_autoescape
except ImportError as e:
    print(f"Error: Missing required package: {e}")
    print("Install with: pip install jinja2 jsonschema")
    sys.exit(1)

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
        if schema_path.exists():
            with open(schema_path, encoding="utf-8") as f:
                schema = json.load(f)
            jsonschema.validate(config, schema)
            logger.info("Configuration validated successfully")
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

        # Calculate address ranges
        for reg_type in ["coils", "discrete_inputs", "holding_registers", "input_registers"]:
            regs = registers.get(reg_type, [])
            if regs:
                addresses = [r["address"] for r in regs]
                stats[f"{reg_type}_min_addr"] = min(addresses)
                stats[f"{reg_type}_max_addr"] = max(addresses)
            else:
                stats[f"{reg_type}_min_addr"] = 0
                stats[f"{reg_type}_max_addr"] = 0

        # Add size information for multi-register types
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

        config["stats"] = stats
        return config

    def generate(
        self,
        config: dict[str, Any],
        output_dir: Path,
    ) -> list[Path]:
        """Generate C code files from configuration.

        Args:
            config: Configuration dictionary.
            output_dir: Directory to write generated files.

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

        # Generate callbacks implementation template
        callbacks_template = self.env.get_template("modbus_callbacks_impl.c.j2")
        callbacks_content = callbacks_template.render(config=config)
        callbacks_path = output_dir / f"{device_name}_callbacks.c"
        callbacks_path.write_text(callbacks_content, encoding="utf-8")
        generated_files.append(callbacks_path)
        logger.info("Generated: %s", callbacks_path)

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

    try:
        generator = ModbusCodeGenerator(template_dir=args.template_dir)
        config = generator.load_config(args.config)
        generated = generator.generate(config, args.output_dir)

        logger.info("Successfully generated %d files", len(generated))
        return 0

    except FileNotFoundError as e:
        logger.error("File not found: %s", e)
        return 1
    except json.JSONDecodeError as e:
        logger.error("Invalid JSON: %s", e)
        return 1
    except jsonschema.ValidationError as e:
        logger.error("Configuration validation failed: %s", e.message)
        return 1
    except Exception as e:
        logger.exception("Unexpected error: %s", e)
        return 1


if __name__ == "__main__":
    sys.exit(main())

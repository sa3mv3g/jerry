"""
Register Map Parser for Jerry Device Integration Tests.

Parses config/jerry_registers.json and exposes register lookup functions
so that test code never hardcodes register addresses. When the register
map changes in jerry_registers.json, all tests automatically pick up the
new addresses without any manual updates.

Usage:
    from register_map import RegisterMap

    reg_map = RegisterMap()

    # Look up a holding register by name
    addr = reg_map.hr("app_version_major")   # → 300
    size = reg_map.hr_size("app_build_number")  # → 2 (uint32 = 2 regs)

    # Look up an input register by name
    addr = reg_map.ir("app_version_major")   # → 100

    # Look up a coil by name
    addr = reg_map.coil("digital_output_0")  # → 0

    # Look up a discrete input by name
    addr = reg_map.di("digital_input_0")     # → 0

    # Get all registers in a group
    version_regs = reg_map.hr_group("version_info")
"""

import json
import pathlib
from typing import Any


# Path to the register map JSON relative to this file's location
_REGISTER_MAP_PATH = (
    pathlib.Path(__file__).resolve().parent.parent.parent
    / "config"
    / "jerry_registers.json"
)


class RegisterMap:
    """Parses jerry_registers.json and provides register address lookups.

    All lookups are by register name (snake_case, as defined in the JSON).
    Raises KeyError with a descriptive message if a register is not found.
    """

    def __init__(self, json_path: pathlib.Path = _REGISTER_MAP_PATH) -> None:
        """Load and parse the register map JSON file.

        Args:
            json_path: Path to jerry_registers.json. Defaults to the project
                       config directory relative to this file.
        """
        with open(json_path, encoding="utf-8") as f:
            data: dict[str, Any] = json.load(f)

        registers = data["registers"]

        # Build lookup dicts: name → full register entry
        self._coils: dict[str, dict[str, Any]] = {
            r["name"]: r for r in registers.get("coils", [])
        }
        self._discrete_inputs: dict[str, dict[str, Any]] = {
            r["name"]: r for r in registers.get("discrete_inputs", [])
        }
        self._holding_registers: dict[str, dict[str, Any]] = {
            r["name"]: r for r in registers.get("holding_registers", [])
        }
        self._input_registers: dict[str, dict[str, Any]] = {
            r["name"]: r for r in registers.get("input_registers", [])
        }

    # -------------------------------------------------------------------------
    # Coil lookups (FC01 read / FC05 write single / FC15 write multiple)
    # -------------------------------------------------------------------------

    def coil(self, name: str) -> int:
        """Return the Modbus address of a coil by name."""
        return self._lookup(self._coils, name, "coil")

    def coil_entry(self, name: str) -> dict[str, Any]:
        """Return the full JSON entry for a coil by name."""
        return self._entry(self._coils, name, "coil")

    # -------------------------------------------------------------------------
    # Discrete input lookups (FC02 read)
    # -------------------------------------------------------------------------

    def di(self, name: str) -> int:
        """Return the Modbus address of a discrete input by name."""
        return self._lookup(self._discrete_inputs, name, "discrete_input")

    def di_entry(self, name: str) -> dict[str, Any]:
        """Return the full JSON entry for a discrete input by name."""
        return self._entry(self._discrete_inputs, name, "discrete_input")

    # -------------------------------------------------------------------------
    # Holding register lookups (FC03 read / FC06 write single / FC16 write multiple)
    # -------------------------------------------------------------------------

    def hr(self, name: str) -> int:
        """Return the Modbus address of a holding register by name."""
        return self._lookup(self._holding_registers, name, "holding_register")

    def hr_size(self, name: str) -> int:
        """Return the number of Modbus registers occupied by a holding register.

        uint16 → 1 register, uint32 / float → 2 registers.
        """
        return self._entry(self._holding_registers, name, "holding_register").get(
            "size", 1
        )

    def hr_access(self, name: str) -> str:
        """Return the access mode of a holding register ('read_only' or 'read_write')."""
        return self._entry(self._holding_registers, name, "holding_register").get(
            "access", "read_write"
        )

    def hr_group(self, group: str) -> list[dict[str, Any]]:
        """Return all holding register entries belonging to the given group."""
        return [r for r in self._holding_registers.values() if r.get("group") == group]

    def hr_entry(self, name: str) -> dict[str, Any]:
        """Return the full JSON entry for a holding register by name."""
        return self._entry(self._holding_registers, name, "holding_register")

    # -------------------------------------------------------------------------
    # Input register lookups (FC04 read)
    # -------------------------------------------------------------------------

    def ir(self, name: str) -> int:
        """Return the Modbus address of an input register by name."""
        return self._lookup(self._input_registers, name, "input_register")

    def ir_size(self, name: str) -> int:
        """Return the number of Modbus registers occupied by an input register."""
        return self._entry(self._input_registers, name, "input_register").get("size", 1)

    def ir_group(self, group: str) -> list[dict[str, Any]]:
        """Return all input register entries belonging to the given group."""
        return [r for r in self._input_registers.values() if r.get("group") == group]

    def ir_entry(self, name: str) -> dict[str, Any]:
        """Return the full JSON entry for an input register by name."""
        return self._entry(self._input_registers, name, "input_register")

    # -------------------------------------------------------------------------
    # Internal helpers
    # -------------------------------------------------------------------------

    @staticmethod
    def _lookup(table: dict[str, dict[str, Any]], name: str, kind: str) -> int:
        """Look up a register address by name, raising KeyError if not found."""
        if name not in table:
            available = sorted(table.keys())
            raise KeyError(
                f"No {kind} named '{name}' in register map. Available: {available}"
            )
        return table[name]["address"]

    @staticmethod
    def _entry(
        table: dict[str, dict[str, Any]], name: str, kind: str
    ) -> dict[str, Any]:
        """Return the full register entry by name, raising KeyError if not found."""
        if name not in table:
            available = sorted(table.keys())
            raise KeyError(
                f"No {kind} named '{name}' in register map. Available: {available}"
            )
        return table[name]

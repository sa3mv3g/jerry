# Copyright (c) 2026
# All rights reserved.
"""
Modbus TCP Robot Framework Keyword Library.

Thin wrapper over pymodbus providing Robot Framework-friendly keywords for
reading and writing Modbus TCP registers on the Jerry STM32H5 DUT.
"""

import struct
from typing import Optional

from pymodbus.client import ModbusTcpClient
from pymodbus.exceptions import ConnectionException, ModbusException
from robot.api import logger
from robot.api.deco import keyword, library


@library(scope="SUITE", version="1.0.0", doc_format="reST")
class ModbusTcpLibrary:
    """Robot Framework keyword library for Modbus TCP communication.

    Provides keywords for FC01-FC06, FC15, FC16 function codes with
    Robot Framework-friendly error handling and assertion helpers.

    The library is opened once per suite (``scope="SUITE"``).
    """

    ROBOT_LIBRARY_SCOPE = "SUITE"

    def __init__(self) -> None:
        self._client: Optional[ModbusTcpClient] = None
        self._unit_id: int = 1

    # =========================================================================
    # Connection management
    # =========================================================================

    @keyword("Connect Modbus TCP")
    def connect_modbus_tcp(
        self,
        host: str,
        port: int = 502,
        unit_id: int = 1,
        timeout: float = 3.0,
    ) -> None:
        """Connect to the Modbus TCP server on the DUT.

        Args:
            host: IP address or hostname of the DUT.
            port: Modbus TCP port (default 502).
            unit_id: Modbus unit/slave ID (default 1).
            timeout: Connection timeout in seconds.

        Raises:
            RuntimeError: If connection cannot be established.
        """
        self._unit_id = int(unit_id)
        self._client = ModbusTcpClient(host=host, port=int(port), timeout=float(timeout))
        try:
            if not self._client.connect():
                raise RuntimeError(
                    f"Cannot connect to Modbus TCP at {host}:{port}"
                )
        except ConnectionException as exc:
            raise RuntimeError(f"Modbus TCP connection failed: {exc}") from exc
        logger.info(f"ModbusTcpLibrary: connected to {host}:{port} unit_id={unit_id}")

    @keyword("Disconnect Modbus TCP")
    def disconnect_modbus_tcp(self) -> None:
        """Close the Modbus TCP connection."""
        if self._client is not None and self._client.connected:
            self._client.close()
        self._client = None
        logger.info("ModbusTcpLibrary: disconnected")

    def _require_connected(self) -> None:
        if self._client is None or not self._client.connected:
            raise RuntimeError(
                "Not connected to Modbus TCP. Call 'Connect Modbus TCP' first."
            )

    # =========================================================================
    # FC01 — Read Coils
    # =========================================================================

    @keyword("Read Coil")
    def read_coil(self, address: int) -> bool:
        """Read a single coil (FC01).

        Args:
            address: Coil address (0-based).

        Returns:
            True if coil is ON, False if OFF.
        """
        self._require_connected()
        result = self._client.read_coils(
            address=int(address), count=1, slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC01 read coil {address} failed: {result}"
            )
        return bool(result.bits[0])

    @keyword("Read Coils")
    def read_coils(self, address: int, count: int) -> list:
        """Read multiple coils (FC01).

        Args:
            address: Starting coil address.
            count: Number of coils to read.

        Returns:
            List of bool values.
        """
        self._require_connected()
        result = self._client.read_coils(
            address=int(address), count=int(count), slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC01 read coils {address}+{count} failed: {result}"
            )
        return list(result.bits[:count])

    @keyword("Read Coil Raw")
    def read_coil_raw(self, address: int) -> object:
        """Read a single coil and return the raw pymodbus response (for negative tests).

        Args:
            address: Coil address.

        Returns:
            Raw pymodbus response object (may be an error response).
        """
        self._require_connected()
        return self._client.read_coils(
            address=int(address), count=1, slave=self._unit_id
        )

    # =========================================================================
    # FC02 — Read Discrete Inputs
    # =========================================================================

    @keyword("Read Discrete Input")
    def read_discrete_input(self, address: int) -> bool:
        """Read a single discrete input (FC02).

        Args:
            address: Discrete input address (0-based).

        Returns:
            True if input is ON, False if OFF.
        """
        self._require_connected()
        result = self._client.read_discrete_inputs(
            address=int(address), count=1, slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC02 read discrete input {address} failed: {result}"
            )
        return bool(result.bits[0])

    # =========================================================================
    # FC05 — Write Single Coil
    # =========================================================================

    @keyword("Write Coil")
    def write_coil(self, address: int, value: bool) -> None:
        """Write a single coil (FC05).

        Args:
            address: Coil address.
            value: True to set ON, False to set OFF.
        """
        self._require_connected()
        result = self._client.write_coil(
            address=int(address), value=bool(value), slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC05 write coil {address}={value} failed: {result}"
            )

    @keyword("Write Coil Raw")
    def write_coil_raw(self, address: int, value: bool) -> object:
        """Write a single coil and return raw response (for negative tests).

        Args:
            address: Coil address.
            value: Coil value.

        Returns:
            Raw pymodbus response object.
        """
        self._require_connected()
        return self._client.write_coil(
            address=int(address), value=bool(value), slave=self._unit_id
        )

    # =========================================================================
    # FC15 — Write Multiple Coils
    # =========================================================================

    @keyword("Write Multiple Coils")
    def write_multiple_coils(self, address: int, values: list) -> None:
        """Write multiple coils (FC15).

        Args:
            address: Starting coil address.
            values: List of bool values.
        """
        self._require_connected()
        result = self._client.write_coils(
            address=int(address), values=[bool(v) for v in values],
            slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC15 write coils {address}+{len(values)} failed: {result}"
            )

    # =========================================================================
    # FC03 — Read Holding Registers
    # =========================================================================

    @keyword("Read Holding Register")
    def read_holding_register(self, address: int) -> int:
        """Read a single holding register (FC03).

        Args:
            address: Register address (0-based).

        Returns:
            16-bit unsigned integer register value.
        """
        self._require_connected()
        result = self._client.read_holding_registers(
            address=int(address), count=1, slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC03 read holding register {address} failed: {result}"
            )
        return int(result.registers[0])

    @keyword("Read Holding Float")
    def read_holding_float(self, address: int) -> float:
        """Read a float32 value from two consecutive holding registers (FC03).

        Registers are in big-endian word order (high word first).

        Args:
            address: Starting register address.

        Returns:
            Float32 value.
        """
        self._require_connected()
        result = self._client.read_holding_registers(
            address=int(address), count=2, slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC03 read holding float {address} failed: {result}"
            )
        raw = struct.pack(">HH", result.registers[0], result.registers[1])
        return struct.unpack(">f", raw)[0]

    @keyword("Read Holding Registers Raw")
    def read_holding_registers_raw(self, address: int, count: int) -> object:
        """Read holding registers and return raw response (for negative tests).

        Args:
            address: Starting register address.
            count: Number of registers to read.

        Returns:
            Raw pymodbus response object.
        """
        self._require_connected()
        return self._client.read_holding_registers(
            address=int(address), count=int(count), slave=self._unit_id
        )

    # =========================================================================
    # FC06 — Write Single Holding Register
    # =========================================================================

    @keyword("Write Holding Register")
    def write_holding_register(self, address: int, value: int) -> None:
        """Write a single holding register (FC06).

        Args:
            address: Register address.
            value: 16-bit unsigned integer value.
        """
        self._require_connected()
        result = self._client.write_register(
            address=int(address), value=int(value), slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC06 write register {address}={value} failed: {result}"
            )

    @keyword("Write Holding Register Raw")
    def write_holding_register_raw(self, address: int, value: int) -> object:
        """Write a holding register and return raw response (for negative tests).

        Args:
            address: Register address.
            value: Register value.

        Returns:
            Raw pymodbus response object.
        """
        self._require_connected()
        return self._client.write_register(
            address=int(address), value=int(value), slave=self._unit_id
        )

    # =========================================================================
    # FC16 — Write Multiple Holding Registers
    # =========================================================================

    @keyword("Write Multiple Registers")
    def write_multiple_registers(self, address: int, values: list) -> None:
        """Write multiple holding registers (FC16).

        Args:
            address: Starting register address.
            values: List of 16-bit unsigned integer values.
        """
        self._require_connected()
        result = self._client.write_registers(
            address=int(address), values=[int(v) for v in values],
            slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC16 write registers {address}+{len(values)} failed: {result}"
            )

    @keyword("Write Holding Float")
    def write_holding_float(self, address: int, value: float) -> None:
        """Write a float32 value to two consecutive holding registers (FC16).

        Args:
            address: Starting register address.
            value: Float32 value to write.
        """
        raw = struct.pack(">f", float(value))
        high, low = struct.unpack(">HH", raw)
        self.write_multiple_registers(address, [high, low])

    # =========================================================================
    # FC04 — Read Input Registers
    # =========================================================================

    @keyword("Read Input Register")
    def read_input_register(self, address: int) -> int:
        """Read a single input register (FC04).

        Args:
            address: Input register address (0-based).

        Returns:
            16-bit unsigned integer register value.
        """
        self._require_connected()
        result = self._client.read_input_registers(
            address=int(address), count=1, slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC04 read input register {address} failed: {result}"
            )
        return int(result.registers[0])

    @keyword("Read Input Float")
    def read_input_float(self, address: int) -> float:
        """Read a float32 value from two consecutive input registers (FC04).

        Args:
            address: Starting input register address.

        Returns:
            Float32 value.
        """
        self._require_connected()
        result = self._client.read_input_registers(
            address=int(address), count=2, slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC04 read input float {address} failed: {result}"
            )
        raw = struct.pack(">HH", result.registers[0], result.registers[1])
        return struct.unpack(">f", raw)[0]

    @keyword("Read Input Registers Raw")
    def read_input_registers_raw(self, address: int, count: int) -> object:
        """Read input registers and return raw response (for negative tests).

        Args:
            address: Starting register address.
            count: Number of registers to read.

        Returns:
            Raw pymodbus response object.
        """
        self._require_connected()
        return self._client.read_input_registers(
            address=int(address), count=int(count), slave=self._unit_id
        )

    # =========================================================================
    # Assertion helpers
    # =========================================================================

    @keyword("Verify Coil State")
    def verify_coil_state(self, address: int, expected: bool) -> None:
        """Read a coil and assert it matches the expected state.

        Args:
            address: Coil address.
            expected: Expected coil state (True/False).
        """
        actual = self.read_coil(address)
        if actual != bool(expected):
            raise AssertionError(
                f"Coil {address}: expected {expected}, got {actual}"
            )

    @keyword("Verify Register Value")
    def verify_register_value(
        self, address: int, expected: int, tolerance: int = 0
    ) -> None:
        """Read a holding register and assert it matches the expected value.

        Args:
            address: Register address.
            expected: Expected register value.
            tolerance: Allowed deviation (default 0 for exact match).
        """
        actual = self.read_holding_register(address)
        if abs(actual - int(expected)) > int(tolerance):
            raise AssertionError(
                f"Register {address}: expected {expected} ±{tolerance}, got {actual}"
            )

    @keyword("Verify Response Is Exception")
    def verify_response_is_exception(
        self, response: object, expected_exception_code: int
    ) -> None:
        """Assert that a raw Modbus response is an exception with the given code.

        Args:
            response: Raw pymodbus response object from a ``*Raw`` keyword.
            expected_exception_code: Expected Modbus exception code
                (1=Illegal Function, 2=Illegal Data Address, 3=Illegal Data Value).
        """
        if not response.isError():
            raise AssertionError(
                f"Expected Modbus exception code {expected_exception_code}, "
                f"but request succeeded"
            )
        actual_code = getattr(response, "exception_code", None)
        if actual_code is not None and actual_code != int(expected_exception_code):
            raise AssertionError(
                f"Expected exception code {expected_exception_code}, "
                f"got {actual_code}"
            )
        logger.info(
            f"ModbusTcpLibrary: confirmed exception code {expected_exception_code}"
        )

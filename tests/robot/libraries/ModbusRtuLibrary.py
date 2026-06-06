# Copyright (c) 2026
# All rights reserved.
"""
Modbus RTU Robot Framework Keyword Library.

Wraps pymodbus serial client for RTU testing via the AD3 UART interface.

NOTE: This library is FUTURE WORK. The Jerry firmware currently only exposes
Modbus TCP (vModbusTask). A vModbusRtuTask must be added to the firmware
before these keywords can be used against real hardware.

The library is provided as a stub so the test suite structure is complete
and can be activated without code changes once the firmware RTU task exists.
"""

import struct
from typing import Optional

from robot.api import logger
from robot.api.deco import keyword, library

try:
    from pymodbus.client import ModbusSerialClient
    _PYMODBUS_SERIAL_AVAILABLE = True
except ImportError:
    _PYMODBUS_SERIAL_AVAILABLE = False


@library(scope="SUITE", version="1.0.0", doc_format="reST")
class ModbusRtuLibrary:
    """Robot Framework keyword library for Modbus RTU communication.

    .. warning::
        This library requires a Modbus RTU task in the Jerry firmware.
        The current firmware only supports Modbus TCP. This library is a
        stub for future use.

    Provides keywords for FC01-FC06, FC15, FC16 over Modbus RTU serial,
    plus raw frame send/receive for negative testing (bad CRC, wrong unit ID,
    partial frames, inter-character gap violations).
    """

    ROBOT_LIBRARY_SCOPE = "SUITE"

    def __init__(self) -> None:
        self._client: Optional[object] = None
        self._unit_id: int = 1
        self._port: str = ""

    # =========================================================================
    # Connection management
    # =========================================================================

    @keyword("Connect Modbus RTU")
    def connect_modbus_rtu(
        self,
        port: str,
        baudrate: int = 115200,
        parity: str = "N",
        stopbits: int = 1,
        unit_id: int = 1,
        timeout: float = 1.0,
    ) -> None:
        """Connect to the Modbus RTU slave via serial port.

        Args:
            port: Serial port (e.g., ``/dev/ttyUSB0`` or ``COM3``).
            baudrate: Baud rate (default 115200).
            parity: Parity: ``N``, ``E``, or ``O`` (default ``N``).
            stopbits: Stop bits (default 1).
            unit_id: Modbus unit/slave ID (default 1).
            timeout: Response timeout in seconds.

        Raises:
            RuntimeError: If pymodbus serial client is not available or
                connection fails.
        """
        if not _PYMODBUS_SERIAL_AVAILABLE:
            raise RuntimeError(
                "pymodbus serial client not available. "
                "Ensure pymodbus is installed with serial support."
            )
        self._unit_id = int(unit_id)
        self._port = port
        self._client = ModbusSerialClient(
            port=port,
            baudrate=int(baudrate),
            parity=parity.upper(),
            stopbits=int(stopbits),
            timeout=float(timeout),
        )
        if not self._client.connect():
            raise RuntimeError(f"Cannot connect to Modbus RTU on {port}")
        logger.info(
            f"ModbusRtuLibrary: connected to {port} "
            f"{baudrate} {parity} {stopbits} unit_id={unit_id}"
        )

    @keyword("Disconnect Modbus RTU")
    def disconnect_modbus_rtu(self) -> None:
        """Close the Modbus RTU serial connection."""
        if self._client is not None:
            self._client.close()
            self._client = None
        logger.info("ModbusRtuLibrary: disconnected")

    def _require_connected(self) -> None:
        if self._client is None:
            raise RuntimeError(
                "Not connected to Modbus RTU. Call 'Connect Modbus RTU' first."
            )

    # =========================================================================
    # FC01 — Read Coils
    # =========================================================================

    @keyword("Read Coil RTU")
    def read_coil_rtu(self, address: int) -> bool:
        """Read a single coil via Modbus RTU (FC01).

        Args:
            address: Coil address.

        Returns:
            True if coil is ON, False if OFF.
        """
        self._require_connected()
        result = self._client.read_coils(
            address=int(address), count=1, slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(f"FC01 RTU read coil {address} failed: {result}")
        return bool(result.bits[0])

    # =========================================================================
    # FC05 — Write Single Coil
    # =========================================================================

    @keyword("Write Coil RTU")
    def write_coil_rtu(self, address: int, value: bool) -> None:
        """Write a single coil via Modbus RTU (FC05).

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
                f"FC05 RTU write coil {address}={value} failed: {result}"
            )

    # =========================================================================
    # FC03 — Read Holding Registers
    # =========================================================================

    @keyword("Read Holding Register RTU")
    def read_holding_register_rtu(self, address: int) -> int:
        """Read a single holding register via Modbus RTU (FC03).

        Args:
            address: Register address.

        Returns:
            16-bit unsigned integer register value.
        """
        self._require_connected()
        result = self._client.read_holding_registers(
            address=int(address), count=1, slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC03 RTU read register {address} failed: {result}"
            )
        return int(result.registers[0])

    @keyword("Read Holding Float RTU")
    def read_holding_float_rtu(self, address: int) -> float:
        """Read a float32 from two holding registers via Modbus RTU (FC03).

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
                f"FC03 RTU read float {address} failed: {result}"
            )
        raw = struct.pack(">HH", result.registers[0], result.registers[1])
        return struct.unpack(">f", raw)[0]

    # =========================================================================
    # FC06 — Write Single Holding Register
    # =========================================================================

    @keyword("Write Holding Register RTU")
    def write_holding_register_rtu(self, address: int, value: int) -> None:
        """Write a single holding register via Modbus RTU (FC06).

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
                f"FC06 RTU write register {address}={value} failed: {result}"
            )

    # =========================================================================
    # FC04 — Read Input Registers
    # =========================================================================

    @keyword("Read Input Register RTU")
    def read_input_register_rtu(self, address: int) -> int:
        """Read a single input register via Modbus RTU (FC04).

        Args:
            address: Input register address.

        Returns:
            16-bit unsigned integer register value.
        """
        self._require_connected()
        result = self._client.read_input_registers(
            address=int(address), count=1, slave=self._unit_id
        )
        if result.isError():
            raise RuntimeError(
                f"FC04 RTU read input register {address} failed: {result}"
            )
        return int(result.registers[0])

    # =========================================================================
    # Raw frame send/receive (for negative testing)
    # =========================================================================

    @keyword("Send Raw RTU Frame")
    def send_raw_rtu_frame(
        self, frame_bytes: bytes, wait_response_s: float = 0.5
    ) -> bytes:
        """Send a raw RTU frame and capture the response bytes.

        Used for negative tests (bad CRC, wrong unit ID, partial frames).

        Args:
            frame_bytes: Raw bytes to send (including address, FC, data, CRC).
            wait_response_s: Time to wait for a response in seconds.

        Returns:
            Raw response bytes (may be empty if no response received).
        """
        self._require_connected()
        import serial  # pyserial
        ser = serial.Serial(
            port=self._port,
            baudrate=self._client.baudrate,
            timeout=wait_response_s,
        )
        ser.write(bytes(frame_bytes))
        response = ser.read(256)
        ser.close()
        logger.info(
            f"ModbusRtuLibrary: sent {bytes(frame_bytes).hex()}, "
            f"received {response.hex()}"
        )
        return response

    @keyword("Verify RTU Response")
    def verify_rtu_response(
        self, response: bytes, expected_bytes: bytes
    ) -> None:
        """Assert that a raw RTU response matches expected bytes.

        Args:
            response: Actual response bytes from ``Send Raw RTU Frame``.
            expected_bytes: Expected response bytes.
        """
        if bytes(response) != bytes(expected_bytes):
            raise AssertionError(
                f"RTU response mismatch: "
                f"expected {bytes(expected_bytes).hex()}, "
                f"got {bytes(response).hex()}"
            )

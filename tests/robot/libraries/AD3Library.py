# Copyright (c) 2026
# All rights reserved.
"""
Analog Discovery 3 Robot Framework Keyword Library.

Wraps the Digilent WaveForms SDK (dwf ctypes bindings) to provide Robot
Framework keywords for:
  - Analog waveform generation (Wavegen W1/W2)
  - Oscilloscope measurement (Scope CH1/CH2)
  - Digital I/O (DIO 0-15)
  - UART protocol analyzer
  - I2C protocol analyzer (snoop and master modes)

Prerequisites:
  - Digilent WaveForms SDK installed on host OS (provides libdwf.so / dwf.dll)
  - dwf Python package installed (fetched via CMake FetchContent)
"""

import time
from typing import Optional

from robot.api import logger
from robot.api.deco import keyword, library

try:
    import dwf
    _DWF_AVAILABLE = True
except ImportError:
    _DWF_AVAILABLE = False
    logger.warn(
        "dwf module not available. AD3Library will run in simulation mode. "
        "Ensure Digilent WaveForms SDK is installed and dwf package is fetched."
    )


@library(scope="SUITE", version="1.0.0", doc_format="reST")
class AD3Library:
    """Robot Framework keyword library for the Analog Discovery 3 instrument.

    Provides keywords for analog output (Wavegen), oscilloscope measurement,
    digital I/O, UART, and I2C protocol analyzer functions.

    The library is opened once per suite (``scope="SUITE"``) to avoid the
    overhead of USB enumeration on every test.

    Example usage in a Robot Framework test::

        *** Settings ***
        Library    libraries/AD3Library.py

        *** Test Cases ***
        Inject 1.65V On ADC Channel 0
            Open AD3 Device
            Set Analog Output    channel=1    voltage_v=1.65
            Sleep    50ms
            Close AD3 Device
    """

    ROBOT_LIBRARY_SCOPE = "SUITE"

    def __init__(self) -> None:
        self._device: Optional[object] = None
        self._wavegen: Optional[object] = None
        self._scope: Optional[object] = None
        self._digital_io: Optional[object] = None
        self._uart: Optional[object] = None
        self._i2c: Optional[object] = None
        self._i2c_snoop_records: list = []
        self._simulation_mode: bool = not _DWF_AVAILABLE

    # =========================================================================
    # Device lifecycle
    # =========================================================================

    @keyword("Open AD3 Device")
    def open_ad3_device(self, serial_number: str = "") -> None:
        """Open connection to the Analog Discovery 3.

        Args:
            serial_number: AD3 serial number string. Leave empty to use the
                first available device.

        Raises:
            RuntimeError: If no AD3 device is found.
        """
        if self._simulation_mode:
            logger.warn("AD3Library: simulation mode — no hardware connected")
            return

        dwf_api = dwf.DwfLibrary()
        if serial_number:
            self._device = dwf_api.device.openBySerialNumber(serial_number)
        else:
            self._device = dwf_api.device.open()

        if self._device is None:
            raise RuntimeError("No Analog Discovery 3 device found")

        self._wavegen = dwf.DwfAnalogOut(self._device)
        self._scope = dwf.DwfAnalogIn(self._device)
        self._digital_io = dwf.DwfDigitalIO(self._device)
        self._uart = dwf.DwfDigitalUart(self._device)
        self._i2c = dwf.DwfDigitalI2c(self._device)
        logger.info("AD3Library: device opened successfully")

    @keyword("Close AD3 Device")
    def close_ad3_device(self) -> None:
        """Release the Analog Discovery 3 device handle."""
        if self._simulation_mode:
            return
        if self._device is not None:
            self._device.close()
            self._device = None
            self._wavegen = None
            self._scope = None
            self._digital_io = None
            self._uart = None
            self._i2c = None
        logger.info("AD3Library: device closed")

    @keyword("Reset AD3")
    def reset_ad3(self) -> None:
        """Reset all AD3 outputs to a safe state.

        Sets all Wavegen outputs to 0V, all DIO outputs to low, and stops
        any running UART/I2C captures. Should be called in suite teardown.
        """
        if self._simulation_mode:
            logger.warn("AD3Library: simulation mode — reset skipped")
            return
        if self._wavegen is not None:
            for ch in (0, 1):
                self._wavegen.reset(ch)
        if self._digital_io is not None:
            self._digital_io.outputEnableSet(0x0000)
            self._digital_io.outputSet(0x0000)
        logger.info("AD3Library: all outputs reset to safe state")

    # =========================================================================
    # Analog output (Wavegen)
    # =========================================================================

    @keyword("Set Analog Output")
    def set_analog_output(self, channel: int, voltage_v: float) -> None:
        """Set a Wavegen channel to a DC voltage.

        Args:
            channel: Wavegen channel number (1 or 2).
            voltage_v: Output voltage in Volts (0.0 to 3.3V for 3.3V VREF).

        Raises:
            ValueError: If channel or voltage is out of range.
        """
        if channel not in (1, 2):
            raise ValueError(f"Invalid Wavegen channel {channel}. Must be 1 or 2.")
        if not 0.0 <= voltage_v <= 5.0:
            raise ValueError(f"Voltage {voltage_v}V out of range (0.0-5.0V)")

        if self._simulation_mode:
            logger.info(f"AD3Library [SIM]: Wavegen CH{channel} = {voltage_v}V DC")
            return

        ch_idx = channel - 1
        self._wavegen.reset(ch_idx)
        self._wavegen.nodeFunctionSet(ch_idx, dwf.DwfAnalogOut.NODE.CARRIER,
                                      dwf.DwfAnalogOut.FUNC.DC)
        self._wavegen.nodeOffsetSet(ch_idx, dwf.DwfAnalogOut.NODE.CARRIER, voltage_v)
        self._wavegen.nodeAmplitudeSet(ch_idx, dwf.DwfAnalogOut.NODE.CARRIER, 0.0)
        self._wavegen.configure(ch_idx, True)
        logger.info(f"AD3Library: Wavegen CH{channel} set to {voltage_v}V DC")

    @keyword("Set Analog Waveform")
    def set_analog_waveform(
        self,
        channel: int,
        waveform: str,
        frequency_hz: float,
        amplitude_v: float,
        offset_v: float = 0.0,
    ) -> None:
        """Set a Wavegen channel to a periodic waveform.

        Args:
            channel: Wavegen channel number (1 or 2).
            waveform: Waveform type: ``sine``, ``square``, or ``triangle``.
            frequency_hz: Waveform frequency in Hz.
            amplitude_v: Peak amplitude in Volts (half peak-to-peak).
            offset_v: DC offset in Volts.
        """
        if channel not in (1, 2):
            raise ValueError(f"Invalid Wavegen channel {channel}. Must be 1 or 2.")

        waveform_map = {
            "sine": dwf.DwfAnalogOut.FUNC.SINE if _DWF_AVAILABLE else None,
            "square": dwf.DwfAnalogOut.FUNC.SQUARE if _DWF_AVAILABLE else None,
            "triangle": dwf.DwfAnalogOut.FUNC.TRIANGLE if _DWF_AVAILABLE else None,
        }
        if waveform.lower() not in waveform_map:
            raise ValueError(f"Unknown waveform '{waveform}'. Use sine/square/triangle.")

        if self._simulation_mode:
            logger.info(
                f"AD3Library [SIM]: Wavegen CH{channel} = {waveform} "
                f"{frequency_hz}Hz amp={amplitude_v}V offset={offset_v}V"
            )
            return

        ch_idx = channel - 1
        func = waveform_map[waveform.lower()]
        self._wavegen.reset(ch_idx)
        self._wavegen.nodeFunctionSet(ch_idx, dwf.DwfAnalogOut.NODE.CARRIER, func)
        self._wavegen.nodeFrequencySet(ch_idx, dwf.DwfAnalogOut.NODE.CARRIER, frequency_hz)
        self._wavegen.nodeAmplitudeSet(ch_idx, dwf.DwfAnalogOut.NODE.CARRIER, amplitude_v)
        self._wavegen.nodeOffsetSet(ch_idx, dwf.DwfAnalogOut.NODE.CARRIER, offset_v)
        self._wavegen.configure(ch_idx, True)
        logger.info(
            f"AD3Library: Wavegen CH{channel} = {waveform} {frequency_hz}Hz "
            f"amp={amplitude_v}V offset={offset_v}V"
        )

    # =========================================================================
    # Oscilloscope measurement
    # =========================================================================

    @keyword("Get Scope Measurement")
    def get_scope_measurement(self, channel: int) -> dict:
        """Measure frequency and duty cycle on a scope channel.

        Args:
            channel: Scope channel number (1 or 2).

        Returns:
            Dictionary with keys ``frequency_hz`` and ``duty_cycle_percent``.
        """
        if channel not in (1, 2):
            raise ValueError(f"Invalid scope channel {channel}. Must be 1 or 2.")

        if self._simulation_mode:
            logger.warn("AD3Library [SIM]: returning dummy scope measurement")
            return {"frequency_hz": 1000.0, "duty_cycle_percent": 50.0}

        # Configure scope for frequency/duty measurement
        ch_idx = channel - 1
        self._scope.channelEnableSet(ch_idx, True)
        self._scope.channelRangeSet(ch_idx, 5.0)
        self._scope.frequencySet(1e6)
        self._scope.bufferSizeSet(8192)
        self._scope.configure(False, True)

        # Wait for acquisition
        timeout = 2.0
        start = time.monotonic()
        while time.monotonic() - start < timeout:
            status = self._scope.status(True)
            if status == dwf.DwfState.Done:
                break
            time.sleep(0.01)

        samples = self._scope.statusData(ch_idx, 8192)
        vref = max(samples)
        threshold = vref * 0.5

        # Count rising edges for frequency
        crossings = []
        for i in range(1, len(samples)):
            if samples[i - 1] < threshold <= samples[i]:
                crossings.append(i)

        sample_rate = 1e6
        if len(crossings) >= 2:
            period_samples = (crossings[-1] - crossings[0]) / (len(crossings) - 1)
            frequency_hz = sample_rate / period_samples
        else:
            frequency_hz = 0.0

        # Duty cycle: fraction of samples above threshold
        high_count = sum(1 for s in samples if s >= threshold)
        duty_cycle_percent = (high_count / len(samples)) * 100.0

        result = {
            "frequency_hz": round(frequency_hz, 2),
            "duty_cycle_percent": round(duty_cycle_percent, 2),
        }
        logger.info(f"AD3Library: Scope CH{channel} measurement: {result}")
        return result

    # =========================================================================
    # Digital I/O
    # =========================================================================

    @keyword("Set Digital Output")
    def set_digital_output(self, pin: int, value: int) -> None:
        """Set a single DIO pin high or low.

        Args:
            pin: DIO pin number (0-15).
            value: 1 for high, 0 for low.
        """
        if not 0 <= pin <= 15:
            raise ValueError(f"DIO pin {pin} out of range (0-15)")

        if self._simulation_mode:
            logger.info(f"AD3Library [SIM]: DIO {pin} = {value}")
            return

        mask = 1 << pin
        self._digital_io.outputEnableSet(
            self._digital_io.outputEnableGet() | mask
        )
        current = self._digital_io.outputGet()
        if value:
            self._digital_io.outputSet(current | mask)
        else:
            self._digital_io.outputSet(current & ~mask)

    @keyword("Set Digital Output Mask")
    def set_digital_output_mask(self, enable_mask: int, value_mask: int) -> None:
        """Set multiple DIO pins at once.

        Args:
            enable_mask: Bitmask of pins to configure as outputs.
            value_mask: Bitmask of output values (1=high, 0=low).
        """
        if self._simulation_mode:
            logger.info(
                f"AD3Library [SIM]: DIO mask enable=0x{enable_mask:04X} "
                f"value=0x{value_mask:04X}"
            )
            return
        self._digital_io.outputEnableSet(enable_mask)
        self._digital_io.outputSet(value_mask)

    @keyword("Get Digital Input")
    def get_digital_input(self, pin: int) -> bool:
        """Read the state of a single DIO pin.

        Args:
            pin: DIO pin number (0-15).

        Returns:
            True if pin is high, False if low.
        """
        if not 0 <= pin <= 15:
            raise ValueError(f"DIO pin {pin} out of range (0-15)")

        if self._simulation_mode:
            logger.warn(f"AD3Library [SIM]: DIO {pin} read = False")
            return False

        state = self._digital_io.inputStatus()
        return bool((state >> pin) & 1)

    @keyword("Get Digital Input Mask")
    def get_digital_input_mask(self, mask: int) -> int:
        """Read multiple DIO pins and return masked value.

        Args:
            mask: Bitmask of pins to read.

        Returns:
            Integer with bits set for pins that are high.
        """
        if self._simulation_mode:
            logger.warn("AD3Library [SIM]: DIO mask read = 0")
            return 0
        state = self._digital_io.inputStatus()
        return state & mask

    # =========================================================================
    # UART
    # =========================================================================

    @keyword("Set UART Config")
    def set_uart_config(
        self,
        baudrate: int = 115200,
        parity: str = "N",
        stop_bits: int = 1,
    ) -> None:
        """Configure the AD3 UART protocol analyzer.

        Args:
            baudrate: Baud rate (e.g., 115200).
            parity: Parity: ``N`` (none), ``E`` (even), ``O`` (odd).
            stop_bits: Number of stop bits (1 or 2).
        """
        if self._simulation_mode:
            logger.info(
                f"AD3Library [SIM]: UART config {baudrate} {parity} {stop_bits}"
            )
            return
        parity_map = {"N": 0, "E": 2, "O": 1}
        self._uart.rateSet(baudrate)
        self._uart.bitsSet(8)
        self._uart.paritySet(parity_map.get(parity.upper(), 0))
        self._uart.stopSet(stop_bits)

    @keyword("Send UART Bytes")
    def send_uart_bytes(self, data: bytes) -> None:
        """Send raw bytes over the AD3 UART TX line.

        Args:
            data: Bytes to transmit.
        """
        if self._simulation_mode:
            logger.info(f"AD3Library [SIM]: UART TX {data.hex()}")
            return
        self._uart.tx(data)

    @keyword("Receive UART Bytes")
    def receive_uart_bytes(self, count: int, timeout_s: float = 1.0) -> bytes:
        """Receive bytes from the AD3 UART RX line.

        Args:
            count: Number of bytes to receive.
            timeout_s: Receive timeout in seconds.

        Returns:
            Received bytes.
        """
        if self._simulation_mode:
            logger.warn("AD3Library [SIM]: UART RX returning empty bytes")
            return b""
        deadline = time.monotonic() + timeout_s
        buf = b""
        while len(buf) < count and time.monotonic() < deadline:
            chunk, _ = self._uart.rx(count - len(buf))
            buf += bytes(chunk)
            if len(buf) < count:
                time.sleep(0.005)
        return buf

    # =========================================================================
    # I2C protocol analyzer
    # =========================================================================

    @keyword("Configure I2C")
    def configure_i2c(
        self,
        clock_hz: int = 100000,
        scl_pin: int = 0,
        sda_pin: int = 1,
    ) -> None:
        """Configure the AD3 I2C protocol analyzer.

        Args:
            clock_hz: I2C clock frequency in Hz (default 100000 = 100kHz).
            scl_pin: DIO pin used for SCL (default 0).
            sda_pin: DIO pin used for SDA (default 1).
        """
        if self._simulation_mode:
            logger.info(
                f"AD3Library [SIM]: I2C config {clock_hz}Hz "
                f"SCL=DIO{scl_pin} SDA=DIO{sda_pin}"
            )
            return
        self._i2c.reset()
        self._i2c.rateSet(clock_hz)
        self._i2c.sclSet(scl_pin)
        self._i2c.sdaSet(sda_pin)

    @keyword("I2C Write")
    def i2c_write(self, address: int, data_bytes: bytes) -> None:
        """Write bytes to an I2C slave address (AD3 as I2C master).

        Args:
            address: 7-bit I2C slave address.
            data_bytes: Bytes to write.

        Raises:
            RuntimeError: If the slave NACKs the address or data.
        """
        if self._simulation_mode:
            logger.info(
                f"AD3Library [SIM]: I2C write addr=0x{address:02X} "
                f"data={bytes(data_bytes).hex()}"
            )
            return
        nak = self._i2c.write(address, list(data_bytes))
        if nak:
            raise RuntimeError(
                f"I2C NACK received writing to address 0x{address:02X}"
            )

    @keyword("I2C Read")
    def i2c_read(self, address: int, count: int) -> bytes:
        """Read bytes from an I2C slave address (AD3 as I2C master).

        Args:
            address: 7-bit I2C slave address.
            count: Number of bytes to read.

        Returns:
            Bytes read from the slave.

        Raises:
            RuntimeError: If the slave NACKs the address.
        """
        if self._simulation_mode:
            logger.warn(
                f"AD3Library [SIM]: I2C read addr=0x{address:02X} "
                f"count={count} returning zeros"
            )
            return bytes(count)
        data, nak = self._i2c.read(address, count)
        if nak:
            raise RuntimeError(
                f"I2C NACK received reading from address 0x{address:02X}"
            )
        return bytes(data)

    @keyword("I2C Write Read")
    def i2c_write_read(
        self, address: int, write_bytes: bytes, read_count: int
    ) -> bytes:
        """Combined I2C write then repeated-start read.

        Args:
            address: 7-bit I2C slave address.
            write_bytes: Bytes to write before the repeated start.
            read_count: Number of bytes to read after the repeated start.

        Returns:
            Bytes read from the slave.
        """
        if self._simulation_mode:
            logger.warn("AD3Library [SIM]: I2C write-read returning zeros")
            return bytes(read_count)
        data, nak = self._i2c.writeRead(address, list(write_bytes), read_count)
        if nak:
            raise RuntimeError(
                f"I2C NACK during write-read to address 0x{address:02X}"
            )
        return bytes(data)

    @keyword("Start I2C Snoop")
    def start_i2c_snoop(self) -> None:
        """Begin passive capture of I2C bus transactions.

        The AD3 monitors SDA/SCL without driving the bus. All transactions
        are buffered internally until ``Stop I2C Snoop`` is called.
        """
        self._i2c_snoop_records = []
        if self._simulation_mode:
            logger.info("AD3Library [SIM]: I2C snoop started")
            return
        self._i2c.snoopStart()
        logger.info("AD3Library: I2C snoop started")

    @keyword("Stop I2C Snoop")
    def stop_i2c_snoop(self) -> list:
        """Stop I2C bus capture and return captured transactions.

        Returns:
            List of transaction dictionaries with keys:
            ``address``, ``direction`` (``write``/``read``), ``data``, ``nak``.
        """
        if self._simulation_mode:
            logger.warn("AD3Library [SIM]: I2C snoop stopped, returning empty list")
            return []
        records = self._i2c.snoopStop()
        self._i2c_snoop_records = [
            {
                "address": r.address,
                "direction": "write" if r.write else "read",
                "data": bytes(r.data),
                "nak": r.nak,
            }
            for r in records
        ]
        logger.info(
            f"AD3Library: I2C snoop stopped, captured "
            f"{len(self._i2c_snoop_records)} transactions"
        )
        return self._i2c_snoop_records

    @keyword("Get I2C Snoop Records")
    def get_i2c_snoop_records(self) -> list:
        """Return the last captured I2C snoop records.

        Returns:
            List of transaction dictionaries (same format as ``Stop I2C Snoop``).
        """
        return self._i2c_snoop_records

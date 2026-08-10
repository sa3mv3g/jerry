"""
to save calibration data there is a specific 
sequence that needs to be followed. need to test
if the sequence and other also have same negative 
testcase. 

Also need to have testcase to validate the case when 
EEPROM is full and cleanup is required

"""

import struct
import pytest

from test_holding_registers import *

# Helper function to pack a float32 into two 16-bit registers
def pack_float32_to_registers(float_value):
    float_bytes = struct.pack('<f', float_value)
    return [
        int.from_bytes(float_bytes[0:2], byteorder='little'),
        int.from_bytes(float_bytes[2:4], byteorder='little')
    ]

# Helper function to unpack two 16-bit registers into a float32
def unpack_registers_to_float32(registers):
    if len(registers) != 2:
        raise ValueError("Expected exactly two registers for float32 conversion")
    combined_bytes = registers[0].to_bytes(2, 'little') + registers[1].to_bytes(2, 'little')
    return struct.unpack('<f', combined_bytes)[0]

@pytest.mark.hardware
@pytest.mark.calibration
class TestCalibrationSequence:
    def _reset_api_status(self, modbus_client, unit_id) -> None:
        status = modbus_client.write_registers(307, [0xffff, 0xffff], device_id=unit_id)
        assert not status.isError(), f"Failed to reset apiStatus: {status}"

    def _get_api_status(self, modbus_client, unit_id) -> int:
        status = modbus_client.read_holding_registers(307, count=2, device_id=unit_id)
        assert not status.isError(), f"Failed to reset apiStatus: {status}"
        return ((status.registers[0] << 16) | status.registers[1])

    def _write_calibration(self, modbus_client, unit_id):
        # ADC 0 Calibration Parameters (float32, 2 registers each)
        # 104: adc_0_scale_factor
        # 106: adc_0_offset_term
        # 108: adc_0_dead_zone

        # Write random values for calibration parameters
        scale_factor_val = 10.0
        offset_term_val = 0.0
        dead_zone_val = 0.0

        # Write adc_0_scale_factor (register 104)
        registers_to_write = pack_float32_to_registers(scale_factor_val)
        write_result = modbus_client.write_registers(104, registers_to_write, device_id=unit_id)
        assert not write_result.isError(), f"Failed to write scale factor: {write_result}"

        # Write adc_0_offset_term (register 106)
        registers_to_write = pack_float32_to_registers(offset_term_val)
        write_result = modbus_client.write_registers(106, registers_to_write, device_id=unit_id)
        assert not write_result.isError(), f"Failed to write offset term: {write_result}"

        # Write adc_0_dead_zone (register 108)
        registers_to_write = pack_float32_to_registers(dead_zone_val)
        write_result = modbus_client.write_registers(108, registers_to_write, device_id=unit_id)
        assert not write_result.isError(), f"Failed to write dead zone: {write_result}"

        # Read back and verify the written values
        # Read adc_0_scale_factor
        read_result = modbus_client.read_holding_registers(104, count=2, device_id=unit_id)
        assert not read_result.isError(), f"Failed to read scale factor: {read_result}"
        read_scale_factor = unpack_registers_to_float32(read_result.registers)
        assert abs(read_scale_factor - scale_factor_val) < 1e-6, f"Scale factor mismatch: expected {scale_factor_val}, got {read_scale_factor}"

        # Read adc_0_offset_term
        read_result = modbus_client.read_holding_registers(106, count=2, device_id=unit_id)
        assert not read_result.isError(), f"Failed to read offset term: {read_result}"
        read_offset_term = unpack_registers_to_float32(read_result.registers)
        assert abs(read_offset_term - offset_term_val) < 1e-6, f"Offset term mismatch: expected {offset_term_val}, got {read_offset_term}"

        # Read adc_0_dead_zone
        read_result = modbus_client.read_holding_registers(108, count=2, device_id=unit_id)
        assert not read_result.isError(), f"Failed to read dead zone: {read_result}"
        read_dead_zone = unpack_registers_to_float32(read_result.registers)
        assert abs(read_dead_zone - dead_zone_val) < 1e-6, f"Dead zone mismatch: expected {dead_zone_val}, got {read_dead_zone}"

    def test_calibration_sequence(self, modbus_client, unit_id):
        adc_0_calibrated_value = []

        read_result = modbus_client.read_input_registers(4, count=2, device_id=unit_id)
        assert not read_result.isError(), f"Failed to read adc_0_calibrated_value: {read_result}"
        adc_0_calibrated_value.append(unpack_registers_to_float32(read_result.registers))

        self._write_calibration(modbus_client, unit_id)

        self._reset_api_status(modbus_client, unit_id)
        assert self._get_api_status(modbus_client, unit_id) == 0xffffffff

        modbus_client.write_register(128, 0x5555, device_id=unit_id)
        modbus_client.write_register(129, 0xDDDD, device_id=unit_id)

        assert self._get_api_status(modbus_client, unit_id) == 0

        read_result = modbus_client.read_input_registers(4, count=2, device_id=unit_id)
        assert not read_result.isError(), f"Failed to read adc_0_calibrated_value: {read_result}"
        adc_0_calibrated_value.append(unpack_registers_to_float32(read_result.registers))

    def test_eeprom_cleanup(self, modbus_client, unit_id):
        lastOpDetails = 0

        self._write_calibration(modbus_client, unit_id)

        for itr in range(2000):
            self._reset_api_status(modbus_client, unit_id)
            assert self._get_api_status(modbus_client, unit_id) == 0xffffffff
            
            modbus_client.write_register(128, 0x5555, device_id=unit_id)
            modbus_client.write_register(129, 0xDDDD, device_id=unit_id)

            lastOpDetails = self._get_api_status(modbus_client, unit_id)

            assert lastOpDetails == 0 or lastOpDetails == 0x100, f"EEPROM Emulation layer return flash write error \"{lastOpDetails}\" during iteration {itr}"

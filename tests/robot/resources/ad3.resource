*** Settings ***
# =============================================================================
# AD3 Resource File
# =============================================================================
# Higher-level keywords composing AD3Library primitives for common HIL
# test operations: ADC injection, digital I/O, PWM measurement, I2C.
# =============================================================================
Documentation    AD3 hardware control keywords for Jerry HIL tests.

Library    ${CURDIR}/../libraries/AD3Library.py
Library    BuiltIn


*** Keywords ***
# =============================================================================
# ADC stimulus keywords
# =============================================================================

Inject DC Voltage On ADC Channel
    [Documentation]    Set AD3 Wavegen to a DC voltage and wait for ADC filter to settle.
    [Arguments]    ${wavegen_channel}    ${voltage_v}
    Set Analog Output    channel=${wavegen_channel}    voltage_v=${voltage_v}
    Sleep    200ms    # Wait for 12-stage biquad filter to settle (~102ms + margin)

Inject Sine Wave On ADC Channel
    [Documentation]    Set AD3 Wavegen to a sine wave for filter testing.
    [Arguments]    ${wavegen_channel}    ${frequency_hz}    ${amplitude_v}    ${offset_v}=0.0
    Set Analog Waveform
    ...    channel=${wavegen_channel}
    ...    waveform=sine
    ...    frequency_hz=${frequency_hz}
    ...    amplitude_v=${amplitude_v}
    ...    offset_v=${offset_v}
    Sleep    500ms    # Allow multiple cycles to be captured

Stop ADC Stimulus
    [Documentation]    Set AD3 Wavegen to 0V (safe state).
    [Arguments]    ${wavegen_channel}
    Set Analog Output    channel=${wavegen_channel}    voltage_v=0.0

# =============================================================================
# Digital I/O keywords
# =============================================================================

Drive Digital Input High
    [Documentation]    Drive a STM32 digital input high via AD3 DIO output.
    [Arguments]    ${di_index}
    Set Digital Output    pin=${di_index}    value=${1}
    Sleep    10ms

Drive Digital Input Low
    [Documentation]    Drive a STM32 digital input low via AD3 DIO output.
    [Arguments]    ${di_index}
    Set Digital Output    pin=${di_index}    value=${0}
    Sleep    10ms

Drive All Digital Inputs
    [Documentation]    Drive all 8 digital inputs to a given pattern via AD3 DIO 0-7.
    [Arguments]    ${pattern}
    Set Digital Output Mask    enable_mask=${255}    value_mask=${pattern}
    Sleep    10ms

Read Digital Output State Via AD3
    [Documentation]    Read a STM32 digital output state via AD3 DIO input (DIO 8-15).
    [Arguments]    ${do_index}
    ${ad3_pin}=    Evaluate    ${do_index} + 8
    ${state}=    Get Digital Input    pin=${ad3_pin}
    RETURN    ${state}

Read All Digital Outputs Via AD3
    [Documentation]    Read all 8 digital outputs (DO0-DO7) via AD3 DIO 8-15.
    ...    Returns an integer bitmask.
    ${mask}=    Get Digital Input Mask    mask=${65280}    # 0xFF00 = DIO 8-15
    ${shifted}=    Evaluate    ${mask} >> 8
    RETURN    ${shifted}

# =============================================================================
# PWM measurement keywords
# =============================================================================

Measure PWM Frequency
    [Documentation]    Measure PWM frequency on an AD3 scope channel.
    [Arguments]    ${scope_channel}
    ${result}=    Get Scope Measurement    channel=${scope_channel}
    RETURN    ${result}[frequency_hz]

Measure PWM Duty Cycle
    [Documentation]    Measure PWM duty cycle on an AD3 scope channel.
    [Arguments]    ${scope_channel}
    ${result}=    Get Scope Measurement    channel=${scope_channel}
    RETURN    ${result}[duty_cycle_percent]

# =============================================================================
# I2C keywords
# =============================================================================

Snoop I2C And Write Coil
    [Documentation]    Start I2C snoop, write a Modbus coil, stop snoop, return records.
    ...    Used to verify the STM32 sent the correct I2C frame to PCF8574/PCF8574A.
    [Arguments]    ${coil_address}    ${coil_value}
    Start I2C Snoop
    Sleep    10ms
    Write Coil    address=${coil_address}    value=${coil_value}
    Sleep    50ms    # Allow I2C transaction to complete
    ${records}=    Stop I2C Snoop
    RETURN    ${records}

Assert I2C Write Captured
    [Documentation]    Assert that an I2C write to a specific address with specific data
    ...    was captured in the snoop records.
    [Arguments]    ${records}    ${expected_addr}    ${expected_data_byte}
    ${found}=    Set Variable    ${False}
    FOR    ${record}    IN    @{records}
        IF    '${record}[direction]' == 'write' and ${record}[address] == ${expected_addr}
            ${data_byte}=    Evaluate    ${record}[data][0] if len(${record}[data]) > 0 else -1
            IF    ${data_byte} == ${expected_data_byte}
                ${found}=    Set Variable    ${True}
            END
        END
    END
    Should Be True    ${found}
    ...    Expected I2C write to addr=0x${expected_addr:02X} data=0x${expected_data_byte:02X} not found in snoop records

Read PCF8574 Via AD3
    [Documentation]    Read the current output state of PCF8574 (DO0-DO7) via AD3 I2C master.
    [Arguments]    ${pcf8574_addr}=${0x20}
    ${data}=    I2C Read    address=${pcf8574_addr}    count=${1}
    RETURN    ${data}[0]

Read PCF8574A Via AD3
    [Documentation]    Read the current output state of PCF8574A (DO8-DO15) via AD3 I2C master.
    [Arguments]    ${pcf8574a_addr}=${0x21}
    ${data}=    I2C Read    address=${pcf8574a_addr}    count=${1}
    RETURN    ${data}[0]

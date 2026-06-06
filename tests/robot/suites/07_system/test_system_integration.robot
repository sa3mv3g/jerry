*** Settings ***
Documentation    System integration tests — end-to-end cross-domain tests that
...              verify the complete signal chain from hardware stimulus through
...              firmware processing to Modbus register readback.
...
...              Tags: hardware, system, positive

Resource    ../../resources/common.resource
Resource    ../../resources/ad3.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    system    positive


*** Test Cases ***
End-To-End ADC Voltage Injection To Modbus Register
    [Documentation]    Inject 1.65V via AD3 Wavegen → read adc_0_value input register
    ...                → verify within ±2% of expected 2048 counts.
    Inject DC Voltage On ADC Channel    wavegen_channel=${1}    voltage_v=${1.65}
    ${raw}=    Read ADC Raw Value    channel=${0}
    ${expected}=    Voltage To Raw ADC    ${1.65}
    Assert ADC Within Tolerance    ${raw}    ${expected}
    Log    End-to-end ADC: injected 1.65V, read ${raw} counts (expected ~${expected})
    [Teardown]    Stop ADC Stimulus    wavegen_channel=${1}

End-To-End Digital Output Coil To AD3 DIO And I2C Snoop
    [Documentation]    Write DO0 coil ON → verify AD3 DIO reads high AND I2C snoop
    ...                confirms correct PCF8574 byte was sent.
    ${records}=    Snoop I2C And Write Coil    coil_address=${0}    coil_value=${True}
    ${dio_state}=    Read Digital Output State Via AD3    do_index=${0}
    Should Be True    ${dio_state}    DO0 should be HIGH via AD3 DIO
    Assert I2C Write Captured    records=${records}    expected_addr=${32}    expected_data_byte=${1}
    [Teardown]    Write Coil    address=${0}    value=${False}

End-To-End Digital Input AD3 Drive To Modbus Discrete Input
    [Documentation]    Drive DI0 high via AD3 DIO → verify FC02 discrete input AND
    ...                mirror coil (address 16) both read ON.
    Drive Digital Input High    di_index=${0}
    ${di_state}=    Read Discrete Input    address=${0}
    ${mirror_state}=    Read Coil    address=${16}
    Should Be True    ${di_state}    DI0 discrete input should be ON
    Should Be True    ${mirror_state}    DI0 mirror coil should be ON
    [Teardown]    Drive Digital Input Low    di_index=${0}

System Tick Register Increments Monotonically
    [Documentation]    Read system tick register twice with 2s delay and verify it increments.
    ${tick1}=    Read System Tick
    Sleep    2000ms
    ${tick2}=    Read System Tick
    Should Be True    ${tick2} > ${tick1}
    ...    System tick should increment: tick1=${tick1}, tick2=${tick2}

RTC Write And Read Back Round Trip
    [Documentation]    Write a known date/time to RTC registers and read back to verify.
    ${orig_year}=    Read Holding Register    address=${210}
    ${orig_month}=    Read Holding Register    address=${211}
    ${orig_day}=    Read Holding Register    address=${212}
    Write Holding Register    address=${210}    value=${2026}
    Write Holding Register    address=${211}    value=${6}
    Write Holding Register    address=${212}    value=${4}
    ${year}=    Read Holding Register    address=${210}
    ${month}=    Read Holding Register    address=${211}
    ${day}=    Read Holding Register    address=${212}
    Should Be Equal As Integers    ${year}    ${2026}    RTC year mismatch
    Should Be Equal As Integers    ${month}    ${6}      RTC month mismatch
    Should Be Equal As Integers    ${day}    ${4}        RTC day mismatch
    [Teardown]    Run Keywords
    ...    Write Holding Register    address=${210}    value=${orig_year}    AND
    ...    Write Holding Register    address=${211}    value=${orig_month}    AND
    ...    Write Holding Register    address=${212}    value=${orig_day}

ADC Calibration Round Trip Scale Factor 2x
    [Documentation]    Write scale_factor=2.0 → inject 1.0V → verify calibrated value ≈ 2x raw.
    ${orig_high}=    Read Holding Register    address=${104}
    ${orig_low}=    Read Holding Register    address=${105}
    Write Holding Float    address=${104}    value=${2.0}
    Inject DC Voltage On ADC Channel    wavegen_channel=${1}    voltage_v=${1.0}
    ${raw}=    Read ADC Raw Value    channel=${0}
    ${cal}=    Read ADC Calibrated Value    channel=${0}
    # With scale=2.0, calibrated should be ~2x the normalized raw
    ${raw_normalized}=    Evaluate    ${raw} / 4095.0
    ${expected_cal}=    Evaluate    ${raw_normalized} * 2.0
    ${diff}=    Evaluate    abs(${cal} - ${expected_cal})
    Should Be True    ${diff} < 0.05
    ...    Calibrated value ${cal} should be ~2x normalized raw ${raw_normalized} (expected ~${expected_cal})
    [Teardown]    Run Keywords
    ...    Stop ADC Stimulus    wavegen_channel=${1}    AND
    ...    Write Multiple Registers    address=${104}    values=${[${orig_high}, ${orig_low}]}

Firmware Version Registers Are Consistent
    [Documentation]    Verify FC04 input register version matches FC03 holding register mirror.
    ${ir_major}=    Read Input Register    address=${100}
    ${ir_minor}=    Read Input Register    address=${101}
    ${ir_patch}=    Read Input Register    address=${102}
    ${hr_major}=    Read Holding Register    address=${300}
    ${hr_minor}=    Read Holding Register    address=${301}
    ${hr_patch}=    Read Holding Register    address=${302}
    Should Be Equal As Integers    ${ir_major}    ${hr_major}    Version major mismatch
    Should Be Equal As Integers    ${ir_minor}    ${hr_minor}    Version minor mismatch
    Should Be Equal As Integers    ${ir_patch}    ${hr_patch}    Version patch mismatch
    Log    Firmware version: ${ir_major}.${ir_minor}.${ir_patch}

*** Settings ***
# =============================================================================
# Modbus TCP Resource File
# =============================================================================
# Higher-level Modbus TCP keywords for common register access patterns
# used across multiple test suites.
# =============================================================================
Documentation    Modbus TCP helper keywords for Jerry HIL tests.

Library    ${CURDIR}/../libraries/ModbusTcpLibrary.py
Library    BuiltIn


*** Keywords ***
# =============================================================================
# Digital output helpers
# =============================================================================

Set Digital Output Via Modbus
    [Documentation]    Write a single digital output coil via Modbus TCP FC05.
    [Arguments]    ${do_index}    ${value}
    Write Coil    address=${do_index}    value=${value}
    Sleep    20ms    # Allow I2C expander write to complete

Set All Digital Outputs Via Modbus
    [Documentation]    Write all 16 digital output coils via Modbus TCP FC15.
    [Arguments]    ${pattern_16bit}
    ${values}=    Create List
    FOR    ${i}    IN RANGE    16
        ${bit}=    Evaluate    bool((${pattern_16bit} >> ${i}) & 1)
        Append To List    ${values}    ${bit}
    END
    Write Multiple Coils    address=${0}    values=${values}
    Sleep    20ms

Read Digital Output Via Modbus
    [Documentation]    Read a single digital output coil state via Modbus TCP FC01.
    [Arguments]    ${do_index}
    ${state}=    Read Coil    address=${do_index}
    RETURN    ${state}

Read Digital Input Via Modbus
    [Documentation]    Read a single discrete input via Modbus TCP FC02.
    [Arguments]    ${di_index}
    ${state}=    Read Discrete Input    address=${di_index}
    RETURN    ${state}

# =============================================================================
# ADC register helpers
# =============================================================================

Read ADC Raw Value
    [Documentation]    Read the raw 12-bit ADC value for a channel via FC04.
    [Arguments]    ${channel}
    ${reg}=    Evaluate    ${channel}
    ${value}=    Read Input Register    address=${reg}
    RETURN    ${value}

Read ADC Calibrated Value
    [Documentation]    Read the calibrated float32 ADC value for a channel via FC04.
    [Arguments]    ${channel}
    ${reg}=    Evaluate    4 + ${channel} * 2
    ${value}=    Read Input Float    address=${reg}
    RETURN    ${value}

# =============================================================================
# PWM register helpers
# =============================================================================

Enable PWM Channel
    [Documentation]    Enable a PWM channel via Modbus coil.
    [Arguments]    ${channel}
    ${coil}=    Evaluate    24 + ${channel}
    Write Coil    address=${coil}    value=${True}

Disable PWM Channel
    [Documentation]    Disable a PWM channel via Modbus coil.
    [Arguments]    ${channel}
    ${coil}=    Evaluate    24 + ${channel}
    Write Coil    address=${coil}    value=${False}

Set PWM Frequency
    [Documentation]    Set PWM frequency via holding register.
    [Arguments]    ${channel}    ${frequency_hz}
    ${reg}=    Evaluate    50 + ${channel} * 2
    Write Holding Register    address=${reg}    value=${frequency_hz}

Set PWM Duty Cycle
    [Documentation]    Set PWM duty cycle (0-100) via holding register.
    [Arguments]    ${channel}    ${duty_pct}
    ${reg}=    Evaluate    51 + ${channel} * 2
    Write Holding Register    address=${reg}    value=${duty_pct}

# =============================================================================
# System register helpers
# =============================================================================

Read System Tick
    [Documentation]    Read the 32-bit system tick counter (two 16-bit registers).
    ${low}=     Read Holding Register    address=${200}
    ${high}=    Read Holding Register    address=${201}
    ${tick}=    Evaluate    (${high} << 16) | ${low}
    RETURN    ${tick}

Read Firmware Version String
    [Documentation]    Read firmware version as a string (e.g., "1.2.3").
    ${major}=    Read Input Register    address=${100}
    ${minor}=    Read Input Register    address=${101}
    ${patch}=    Read Input Register    address=${102}
    ${version}=    Set Variable    ${major}.${minor}.${patch}
    RETURN    ${version}

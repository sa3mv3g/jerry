*** Settings ***
# =============================================================================
# Common Resource File
# =============================================================================
# Shared keywords, variables, and suite setup/teardown for all Jerry HIL tests.
# =============================================================================
Documentation    Common resources for Jerry Robot Framework HIL tests.

Library    ${CURDIR}/../libraries/AD3Library.py
Library    ${CURDIR}/../libraries/ModbusTcpLibrary.py
Library    ${CURDIR}/../libraries/FlashLibrary.py
Library    Collections
Library    String
Library    OperatingSystem

Variables    ${CURDIR}/../config/hardware_config.yaml


*** Variables ***
# DUT connection — override via --variable on command line
${DUT_HOST}             192.168.1.100
${DUT_PORT}             ${502}
${DUT_UNIT_ID}          ${1}
${DUT_TIMEOUT}          ${3.0}

# Test control flags
${SKIP_FLASH}           ${False}    # Set True to skip firmware flashing

# Tolerances
${ADC_TOLERANCE_PCT}    ${2.0}      # ADC accuracy tolerance in percent
${PWM_FREQ_TOL_PCT}     ${1.0}      # PWM frequency tolerance in percent
${PWM_DUTY_TOL_PCT}     ${1.0}      # PWM duty cycle tolerance in percent
${I2C_CLK_TOL_PCT}      ${10.0}     # I2C clock tolerance in percent

# ADC constants
${ADC_MAX_RAW}          ${4095}     # 12-bit ADC maximum value
${ADC_VREF}             ${3.3}      # ADC reference voltage

# Modbus register addresses (from jerry_registers.json)
${REG_ADC0_RAW}         ${0}
${REG_ADC1_RAW}         ${1}
${REG_ADC2_RAW}         ${2}
${REG_ADC3_RAW}         ${3}
${REG_ADC0_CAL}         ${4}        # float32, 2 registers
${REG_ADC1_CAL}         ${6}
${REG_ADC2_CAL}         ${8}
${REG_ADC3_CAL}         ${10}
${REG_VERSION_MAJOR}    ${100}      # input register
${REG_SYS_TICK_LOW}     ${200}      # holding register
${REG_SYS_TICK_HIGH}    ${201}
${REG_RTC_YEAR}         ${210}
${REG_RTC_MONTH}        ${211}
${REG_RTC_DAY}          ${212}
${REG_RTC_HOUR}         ${213}
${REG_RTC_MINUTE}       ${214}
${REG_RTC_SECOND}       ${215}
${REG_HR_VERSION_MAJOR} ${300}      # holding register mirror
${REG_HR_VERSION_MINOR} ${301}
${REG_HR_VERSION_PATCH} ${302}

# Coil addresses
${COIL_DO_START}        ${0}        # DO0-DO15
${COIL_DI_MIRROR_START} ${16}       # DI0-DI7 mirror (read-only)
${COIL_PWM0_ENABLE}     ${24}
${COIL_PWM1_ENABLE}     ${25}
${COIL_PWM2_ENABLE}     ${26}
${COIL_PWM3_ENABLE}     ${27}

# Discrete input addresses
${DI_START}             ${0}        # DI0-DI7


*** Keywords ***
Suite Hardware Setup
    [Documentation]    Open AD3 device and connect Modbus TCP. Called in Suite Setup.
    Open AD3 Device
    Configure I2C    clock_hz=100000    scl_pin=0    sda_pin=1
    Connect Modbus TCP    ${DUT_HOST}    ${DUT_PORT}    ${DUT_UNIT_ID}    ${DUT_TIMEOUT}
    Wait For DUT Ready    host=${DUT_HOST}    port=${DUT_PORT}    timeout=15.0

Suite Hardware Teardown
    [Documentation]    Reset AD3 outputs and disconnect Modbus TCP. Called in Suite Teardown.
    Reset AD3
    Disconnect Modbus TCP
    Close AD3 Device

Flash And Setup
    [Documentation]    Flash firmware (if needed) then run Suite Hardware Setup.
    IF    not ${SKIP_FLASH}
        Flash Firmware If Needed
    END
    Suite Hardware Setup

Voltage To Raw ADC
    [Documentation]    Convert a voltage to expected 12-bit ADC raw value.
    [Arguments]    ${voltage_v}
    ${raw}=    Evaluate    int(round(${voltage_v} / ${ADC_VREF} * ${ADC_MAX_RAW}))
    RETURN    ${raw}

Assert ADC Within Tolerance
    [Documentation]    Assert that an ADC raw value is within tolerance of expected.
    [Arguments]    ${actual_raw}    ${expected_raw}    ${tolerance_pct}=${ADC_TOLERANCE_PCT}
    ${tolerance_counts}=    Evaluate    int(${ADC_MAX_RAW} * ${tolerance_pct} / 100.0)
    ${diff}=    Evaluate    abs(${actual_raw} - ${expected_raw})
    Should Be True    ${diff} <= ${tolerance_counts}
    ...    ADC value ${actual_raw} not within ${tolerance_pct}% of expected ${expected_raw} (tolerance ±${tolerance_counts} counts)

Assert Frequency Within Tolerance
    [Documentation]    Assert that a measured frequency is within tolerance of expected.
    [Arguments]    ${actual_hz}    ${expected_hz}    ${tolerance_pct}=${PWM_FREQ_TOL_PCT}
    ${tolerance_hz}=    Evaluate    ${expected_hz} * ${tolerance_pct} / 100.0
    ${diff}=    Evaluate    abs(${actual_hz} - ${expected_hz})
    Should Be True    ${diff} <= ${tolerance_hz}
    ...    Frequency ${actual_hz}Hz not within ${tolerance_pct}% of expected ${expected_hz}Hz

Assert Duty Cycle Within Tolerance
    [Documentation]    Assert that a measured duty cycle is within tolerance of expected.
    [Arguments]    ${actual_pct}    ${expected_pct}    ${tolerance_pct}=${PWM_DUTY_TOL_PCT}
    ${diff}=    Evaluate    abs(${actual_pct} - ${expected_pct})
    Should Be True    ${diff} <= ${tolerance_pct}
    ...    Duty cycle ${actual_pct}% not within ±${tolerance_pct}% of expected ${expected_pct}%

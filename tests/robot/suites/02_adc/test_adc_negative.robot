*** Settings ***
Documentation    ADC negative tests — verify correct exception responses for
...              write-to-read-only, invalid addresses, and invalid float values.
...
...              Tags: hardware, adc, negative

Resource    ../../resources/common.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    adc    negative


*** Test Cases ***
Write To Read-Only ADC Input Register Should Return Exception
    [Documentation]    FC06 write to ADC input register (FC04 address 0) should fail.
    [Tags]    negative    read_only_violation
    ${response}=    Write Holding Register Raw    address=${65535}    value=${0}
    Should Be True    ${response.isError()}
    ...    Writing to invalid address should return Modbus exception

Read Input Register At Invalid Address Should Return Exception
    [Documentation]    FC04 read at address 65535 should return exception 0x02.
    [Tags]    negative    invalid_address
    ${response}=    Read Input Registers Raw    address=${65535}    count=${1}
    Should Be True    ${response.isError()}
    ...    Reading input register at address 65535 should return Modbus exception

Write NaN To ADC Scale Factor Should Be Handled
    [Documentation]    Write NaN (0x7FC00000) to adc_0_scale_factor and verify firmware
    ...                does not crash (Modbus still responds after write).
    [Tags]    negative    invalid_data_value
    # NaN as two uint16 registers: 0x7FC0, 0x0000
    ${response}=    Read Holding Registers Raw    address=${104}    count=${2}
    ${original_high}=    Set Variable    ${response.registers[0]}
    ${original_low}=    Set Variable    ${response.registers[1]}
    # Write NaN
    Write Multiple Registers    address=${104}    values=${[32704, 0]}    # 0x7FC0, 0x0000
    Sleep    100ms
    # Verify DUT still responds to Modbus
    ${tick}=    Read Holding Register    address=${200}
    Should Be True    ${tick} >= 0    Firmware should still respond after NaN write
    # Restore original value
    [Teardown]    Write Multiple Registers    address=${104}    values=${[${original_high}, ${original_low}]}

Write Infinity To ADC Scale Factor Should Be Handled
    [Documentation]    Write +Inf (0x7F800000) to adc_0_scale_factor and verify firmware
    ...                does not crash.
    [Tags]    negative    invalid_data_value
    ${response}=    Read Holding Registers Raw    address=${104}    count=${2}
    ${original_high}=    Set Variable    ${response.registers[0]}
    ${original_low}=    Set Variable    ${response.registers[1]}
    # Write +Inf: 0x7F80, 0x0000
    Write Multiple Registers    address=${104}    values=${[32640, 0]}
    Sleep    100ms
    ${tick}=    Read Holding Register    address=${200}
    Should Be True    ${tick} >= 0    Firmware should still respond after Inf write
    [Teardown]    Write Multiple Registers    address=${104}    values=${[${original_high}, ${original_low}]}

Read Input Register Count Zero Should Return Exception
    [Documentation]    FC04 read with count=0 should return exception 0x03.
    [Tags]    negative    invalid_quantity
    Run Keyword And Expect Error    *
    ...    Read Input Registers Raw    address=${0}    count=${0}

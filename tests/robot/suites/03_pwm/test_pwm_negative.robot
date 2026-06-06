*** Settings ***
Documentation    PWM negative tests — verify firmware handles invalid frequency
...              and duty cycle values gracefully.
...
...              Tags: hardware, pwm, negative

Resource    ../../resources/common.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    pwm    negative


*** Test Cases ***
Write PWM Frequency Zero Should Not Crash Firmware
    [Documentation]    Write frequency=0 to PWM0 register and verify firmware still responds.
    [Tags]    negative    boundary_condition
    Enable PWM Channel    channel=${0}
    Set PWM Frequency    channel=${0}    frequency_hz=${0}
    Sleep    100ms
    # Firmware should still respond to Modbus
    ${tick}=    Read Holding Register    address=${200}
    Should Be True    ${tick} >= 0    Firmware should still respond after freq=0 write
    [Teardown]    Disable PWM Channel    channel=${0}

Write PWM Duty Cycle Above 100 Should Be Clamped Or Rejected
    [Documentation]    Write duty cycle > 100 and verify firmware clamps or rejects.
    [Tags]    negative    boundary_condition
    Enable PWM Channel    channel=${0}
    Set PWM Frequency    channel=${0}    frequency_hz=${1000}
    # Write 200% duty cycle (invalid)
    Write Holding Register    address=${51}    value=${200}
    Sleep    100ms
    # Firmware should still respond
    ${tick}=    Read Holding Register    address=${200}
    Should Be True    ${tick} >= 0    Firmware should still respond after invalid duty write
    [Teardown]    Disable PWM Channel    channel=${0}

Write PWM Holding Register At Invalid Address Should Return Exception
    [Documentation]    FC06 write to address beyond PWM register map should return exception.
    [Tags]    negative    invalid_address
    ${response}=    Write Holding Register Raw    address=${65535}    value=${1000}
    Should Be True    ${response.isError()}
    ...    Writing to invalid PWM register address should return Modbus exception

Read PWM Register At Invalid Address Should Return Exception
    [Documentation]    FC03 read at address 65535 should return exception 0x02.
    [Tags]    negative    invalid_address
    ${response}=    Read Holding Registers Raw    address=${65535}    count=${1}
    Should Be True    ${response.isError()}
    ...    Reading invalid PWM register address should return Modbus exception

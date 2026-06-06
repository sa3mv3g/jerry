*** Settings ***
Documentation    PWM duty cycle tests — set duty cycle via holding registers
...              and measure with AD3 scope oscilloscope.
...
...              Tags: hardware, pwm, positive

Resource    ../../resources/common.resource
Resource    ../../resources/ad3.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    pwm    positive


*** Test Cases ***
PWM Channel 0 Duty Cycle 25 Percent
    [Documentation]    Set PWM0 duty cycle to 25% and verify with AD3 scope.
    Enable PWM Channel    channel=${0}
    Set PWM Frequency    channel=${0}    frequency_hz=${1000}
    Set PWM Duty Cycle    channel=${0}    duty_pct=${25}
    Sleep    100ms
    ${measured}=    Measure PWM Duty Cycle    scope_channel=${1}
    Assert Duty Cycle Within Tolerance    ${measured}    ${25}
    [Teardown]    Disable PWM Channel    channel=${0}

PWM Channel 0 Duty Cycle 50 Percent
    [Documentation]    Set PWM0 duty cycle to 50% and verify with AD3 scope.
    Enable PWM Channel    channel=${0}
    Set PWM Frequency    channel=${0}    frequency_hz=${1000}
    Set PWM Duty Cycle    channel=${0}    duty_pct=${50}
    Sleep    100ms
    ${measured}=    Measure PWM Duty Cycle    scope_channel=${1}
    Assert Duty Cycle Within Tolerance    ${measured}    ${50}
    [Teardown]    Disable PWM Channel    channel=${0}

PWM Channel 0 Duty Cycle 75 Percent
    [Documentation]    Set PWM0 duty cycle to 75% and verify with AD3 scope.
    Enable PWM Channel    channel=${0}
    Set PWM Frequency    channel=${0}    frequency_hz=${1000}
    Set PWM Duty Cycle    channel=${0}    duty_pct=${75}
    Sleep    100ms
    ${measured}=    Measure PWM Duty Cycle    scope_channel=${1}
    Assert Duty Cycle Within Tolerance    ${measured}    ${75}
    [Teardown]    Disable PWM Channel    channel=${0}

PWM Channel 0 Duty Cycle 0 Percent
    [Documentation]    Set PWM0 duty cycle to 0% and verify output is always low.
    Enable PWM Channel    channel=${0}
    Set PWM Frequency    channel=${0}    frequency_hz=${1000}
    Set PWM Duty Cycle    channel=${0}    duty_pct=${0}
    Sleep    100ms
    ${measured}=    Measure PWM Duty Cycle    scope_channel=${1}
    Should Be True    ${measured} < 2.0
    ...    0% duty cycle should produce near-zero measured duty cycle, got ${measured}%
    [Teardown]    Disable PWM Channel    channel=${0}

PWM Channel 0 Duty Cycle 100 Percent
    [Documentation]    Set PWM0 duty cycle to 100% and verify output is always high.
    Enable PWM Channel    channel=${0}
    Set PWM Frequency    channel=${0}    frequency_hz=${1000}
    Set PWM Duty Cycle    channel=${0}    duty_pct=${100}
    Sleep    100ms
    ${measured}=    Measure PWM Duty Cycle    scope_channel=${1}
    Should Be True    ${measured} > 98.0
    ...    100% duty cycle should produce near-100% measured duty cycle, got ${measured}%
    [Teardown]    Disable PWM Channel    channel=${0}

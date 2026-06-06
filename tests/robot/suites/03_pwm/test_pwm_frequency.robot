*** Settings ***
Documentation    PWM frequency tests — enable PWM channels via Modbus coils,
...              set frequency via holding registers, and measure with AD3 scope.
...
...              Tags: hardware, pwm, positive

Resource    ../../resources/common.resource
Resource    ../../resources/ad3.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    pwm    positive


*** Variables ***
@{PWM_TEST_FREQUENCIES}    100    1000    10000    50000


*** Test Cases ***
PWM Channel 0 Frequency 100Hz
    [Documentation]    Set PWM0 to 100Hz and verify with AD3 scope CH1.
    Enable PWM Channel    channel=${0}
    Set PWM Frequency    channel=${0}    frequency_hz=${100}
    Sleep    100ms
    ${measured}=    Measure PWM Frequency    scope_channel=${1}
    Assert Frequency Within Tolerance    ${measured}    ${100}
    [Teardown]    Disable PWM Channel    channel=${0}

PWM Channel 0 Frequency 1kHz
    [Documentation]    Set PWM0 to 1kHz and verify with AD3 scope CH1.
    Enable PWM Channel    channel=${0}
    Set PWM Frequency    channel=${0}    frequency_hz=${1000}
    Sleep    100ms
    ${measured}=    Measure PWM Frequency    scope_channel=${1}
    Assert Frequency Within Tolerance    ${measured}    ${1000}
    [Teardown]    Disable PWM Channel    channel=${0}

PWM Channel 0 Frequency 10kHz
    [Documentation]    Set PWM0 to 10kHz and verify with AD3 scope CH1.
    Enable PWM Channel    channel=${0}
    Set PWM Frequency    channel=${0}    frequency_hz=${10000}
    Sleep    100ms
    ${measured}=    Measure PWM Frequency    scope_channel=${1}
    Assert Frequency Within Tolerance    ${measured}    ${10000}
    [Teardown]    Disable PWM Channel    channel=${0}

PWM Channel 0 Frequency 50kHz
    [Documentation]    Set PWM0 to 50kHz and verify with AD3 scope CH1.
    Enable PWM Channel    channel=${0}
    Set PWM Frequency    channel=${0}    frequency_hz=${50000}
    Sleep    100ms
    ${measured}=    Measure PWM Frequency    scope_channel=${1}
    Assert Frequency Within Tolerance    ${measured}    ${50000}
    [Teardown]    Disable PWM Channel    channel=${0}

PWM Channel 1 Frequency 1kHz
    [Documentation]    Set PWM1 to 1kHz and verify with AD3 scope CH2.
    Enable PWM Channel    channel=${1}
    Set PWM Frequency    channel=${1}    frequency_hz=${1000}
    Sleep    100ms
    ${measured}=    Measure PWM Frequency    scope_channel=${2}
    Assert Frequency Within Tolerance    ${measured}    ${1000}
    [Teardown]    Disable PWM Channel    channel=${1}

PWM Disabled Channel Produces No Output
    [Documentation]    Disable PWM0 and verify scope reads no frequency.
    Disable PWM Channel    channel=${0}
    Sleep    50ms
    ${measured}=    Measure PWM Frequency    scope_channel=${1}
    Should Be True    ${measured} == 0.0 or ${measured} < 10.0
    ...    Disabled PWM channel should produce no measurable frequency

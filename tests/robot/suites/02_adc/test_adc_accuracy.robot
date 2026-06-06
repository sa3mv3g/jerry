*** Settings ***
Documentation    ADC accuracy tests — inject known voltages via AD3 Wavegen and
...              verify STM32 ADC raw and calibrated values via Modbus TCP FC04.
...
...              Tags: hardware, adc, positive

Resource    ../../resources/common.resource
Resource    ../../resources/ad3.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    adc    positive


*** Variables ***
@{TEST_VOLTAGES}    0.0    0.5    1.0    1.65    2.5    3.3


*** Test Cases ***
ADC Channel 0 Accuracy Sweep
    [Documentation]    Inject 6 voltage levels on ADC CH0 and verify raw register within ±2%.
    FOR    ${voltage}    IN    @{TEST_VOLTAGES}
        Inject DC Voltage On ADC Channel    wavegen_channel=${1}    voltage_v=${voltage}
        ${actual_raw}=    Read ADC Raw Value    channel=${0}
        ${expected_raw}=    Voltage To Raw ADC    ${voltage}
        Assert ADC Within Tolerance    ${actual_raw}    ${expected_raw}
        Log    ADC0 @ ${voltage}V: raw=${actual_raw} expected=${expected_raw}
    END
    [Teardown]    Stop ADC Stimulus    wavegen_channel=${1}

ADC Channel 1 Accuracy Sweep
    [Documentation]    Inject 6 voltage levels on ADC CH1 and verify raw register within ±2%.
    FOR    ${voltage}    IN    @{TEST_VOLTAGES}
        Inject DC Voltage On ADC Channel    wavegen_channel=${2}    voltage_v=${voltage}
        ${actual_raw}=    Read ADC Raw Value    channel=${1}
        ${expected_raw}=    Voltage To Raw ADC    ${voltage}
        Assert ADC Within Tolerance    ${actual_raw}    ${expected_raw}
        Log    ADC1 @ ${voltage}V: raw=${actual_raw} expected=${expected_raw}
    END
    [Teardown]    Stop ADC Stimulus    wavegen_channel=${2}

ADC Channel 0 Calibrated Value Matches Raw
    [Documentation]    With default scale=1.0 offset=0.0, calibrated value should match raw.
    Inject DC Voltage On ADC Channel    wavegen_channel=${1}    voltage_v=${1.65}
    ${raw}=    Read ADC Raw Value    channel=${0}
    ${cal}=    Read ADC Calibrated Value    channel=${0}
    # Calibrated value is normalized 0.0-1.0; raw/4095 should match within tolerance
    ${raw_normalized}=    Evaluate    ${raw} / 4095.0
    ${diff}=    Evaluate    abs(${cal} - ${raw_normalized})
    Should Be True    ${diff} < 0.02
    ...    Calibrated value ${cal} does not match normalized raw ${raw_normalized}
    [Teardown]    Stop ADC Stimulus    wavegen_channel=${1}

ADC Channel 0 Linearity 10 Point Sweep
    [Documentation]    Verify ADC linearity across 10 evenly spaced voltage points.
    FOR    ${step}    IN RANGE    11
        ${voltage}=    Evaluate    ${step} * 3.3 / 10.0
        Inject DC Voltage On ADC Channel    wavegen_channel=${1}    voltage_v=${voltage}
        ${actual_raw}=    Read ADC Raw Value    channel=${0}
        ${expected_raw}=    Voltage To Raw ADC    ${voltage}
        Assert ADC Within Tolerance    ${actual_raw}    ${expected_raw}
    END
    [Teardown]    Stop ADC Stimulus    wavegen_channel=${1}

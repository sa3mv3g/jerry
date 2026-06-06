*** Settings ***
Documentation    ADC filter tests — verify the 12-stage biquad cascade filter
...              (4th order Butterworth LPF + 10 notch filters for 50Hz rejection).
...
...              Tags: hardware, adc, positive

Resource    ../../resources/common.resource
Resource    ../../resources/ad3.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    adc    positive


*** Test Cases ***
ADC Filter Settles After Power On
    [Documentation]    After firmware boot, ADC filter should settle within 200ms.
    ...                Verify calibrated value is stable (not changing rapidly).
    Sleep    200ms    # Allow filter to settle
    ${val1}=    Read ADC Calibrated Value    channel=${0}
    Sleep    50ms
    ${val2}=    Read ADC Calibrated Value    channel=${0}
    ${diff}=    Evaluate    abs(${val1} - ${val2})
    Should Be True    ${diff} < 0.01
    ...    ADC filter not settled: values ${val1} and ${val2} differ by ${diff}

ADC Filter Rejects 50Hz Mains Frequency
    [Documentation]    Inject 50Hz sine wave and verify the notch filter attenuates it.
    ...                The calibrated value should remain near the DC offset (0.5 * VREF).
    # Inject 50Hz sine: 0.5V amplitude around 1.65V DC offset
    Inject Sine Wave On ADC Channel
    ...    wavegen_channel=${1}
    ...    frequency_hz=${50}
    ...    amplitude_v=${0.5}
    ...    offset_v=${1.65}
    Sleep    500ms    # Allow multiple 50Hz cycles and filter to process
    # Read multiple samples and check variance is low (50Hz attenuated)
    ${samples}=    Create List
    FOR    ${_}    IN RANGE    10
        ${val}=    Read ADC Calibrated Value    channel=${0}
        Append To List    ${samples}    ${val}
        Sleep    20ms
    END
    # Calculate peak-to-peak variation
    ${max_val}=    Evaluate    max(${samples})
    ${min_val}=    Evaluate    min(${samples})
    ${pp}=    Evaluate    ${max_val} - ${min_val}
    # With 50Hz notch filter, peak-to-peak should be < 5% of full scale
    Should Be True    ${pp} < 0.05
    ...    50Hz not attenuated: peak-to-peak variation ${pp} exceeds 5% of full scale
    [Teardown]    Stop ADC Stimulus    wavegen_channel=${1}

ADC Filter Preserves DC Component With 50Hz Injected
    [Documentation]    Inject DC + 50Hz and verify DC component is preserved.
    # Inject 50Hz sine around 1.65V DC (50% of VREF)
    Inject Sine Wave On ADC Channel
    ...    wavegen_channel=${1}
    ...    frequency_hz=${50}
    ...    amplitude_v=${0.3}
    ...    offset_v=${1.65}
    Sleep    500ms
    ${cal}=    Read ADC Calibrated Value    channel=${0}
    # DC component should be ~0.5 (1.65V / 3.3V)
    ${diff}=    Evaluate    abs(${cal} - 0.5)
    Should Be True    ${diff} < 0.05
    ...    DC component not preserved: calibrated value ${cal} should be ~0.5
    [Teardown]    Stop ADC Stimulus    wavegen_channel=${1}

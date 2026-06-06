*** Settings ***
Documentation    Modbus TCP register tests — verify FC03, FC04, FC06, FC16 function codes
...              for holding registers, input registers, and float32 values.
...
...              Tags: hardware, modbus_tcp, positive

Resource    ../../resources/common.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    modbus_tcp    positive


*** Test Cases ***
FC04 Read ADC Raw Input Registers
    [Documentation]    FC04 read of ADC raw input registers (0-3) should return 0-4095.
    FOR    ${i}    IN RANGE    4
        ${value}=    Read Input Register    address=${i}
        Should Be True    0 <= ${value} <= 4095
        ...    ADC${i} raw value ${value} should be in range 0-4095
    END

FC04 Read ADC Calibrated Float Input Registers
    [Documentation]    FC04 read of ADC calibrated float registers should return 0.0-1.0.
    FOR    ${i}    IN RANGE    4
        ${reg}=    Evaluate    4 + ${i} * 2
        ${value}=    Read Input Float    address=${reg}
        Should Be True    -0.1 <= ${value} <= 1.1
        ...    ADC${i} calibrated value ${value} should be in range 0.0-1.0
    END

FC04 Read Firmware Version Input Registers
    [Documentation]    FC04 read of version registers (100-102) should return valid values.
    ${major}=    Read Input Register    address=${100}
    ${minor}=    Read Input Register    address=${101}
    ${patch}=    Read Input Register    address=${102}
    Should Be True    ${major} >= 0    Version major should be non-negative
    Should Be True    ${minor} >= 0    Version minor should be non-negative
    Should Be True    ${patch} >= 0    Version patch should be non-negative
    Log    Firmware version: ${major}.${minor}.${patch}

FC03 Read System Tick Holding Registers
    [Documentation]    FC03 read of system tick registers (200-201) should return incrementing values.
    ${tick1}=    Read System Tick
    Sleep    1000ms
    ${tick2}=    Read System Tick
    Should Be True    ${tick2} > ${tick1}
    ...    System tick should increment over time (tick1=${tick1}, tick2=${tick2})

FC03 Read RTC Holding Registers
    [Documentation]    FC03 read of RTC registers (210-215) should return valid date/time values.
    ${year}=    Read Holding Register    address=${210}
    ${month}=    Read Holding Register    address=${211}
    ${day}=    Read Holding Register    address=${212}
    ${hour}=    Read Holding Register    address=${213}
    ${minute}=    Read Holding Register    address=${214}
    ${second}=    Read Holding Register    address=${215}
    Should Be True    2000 <= ${year} <= 2099    RTC year ${year} out of range
    Should Be True    1 <= ${month} <= 12        RTC month ${month} out of range
    Should Be True    1 <= ${day} <= 31          RTC day ${day} out of range
    Should Be True    0 <= ${hour} <= 23         RTC hour ${hour} out of range
    Should Be True    0 <= ${minute} <= 59       RTC minute ${minute} out of range
    Should Be True    0 <= ${second} <= 59       RTC second ${second} out of range

FC06 Write And Read Back Holding Register
    [Documentation]    FC06 write to RTC year register and FC03 read back should match.
    ${original}=    Read Holding Register    address=${210}
    Write Holding Register    address=${210}    value=${2026}
    ${readback}=    Read Holding Register    address=${210}
    Should Be Equal As Integers    ${readback}    ${2026}
    ...    RTC year register should read back 2026 after FC06 write
    [Teardown]    Write Holding Register    address=${210}    value=${original}

FC16 Write And Read Back Float32 ADC Scale Factor
    [Documentation]    FC16 write float32 scale factor and FC03 read back should match.
    ${original_high}=    Read Holding Register    address=${104}
    ${original_low}=    Read Holding Register    address=${105}
    Write Holding Float    address=${104}    value=${2.0}
    ${readback}=    Read Holding Float    address=${104}
    ${diff}=    Evaluate    abs(${readback} - 2.0)
    Should Be True    ${diff} < 0.001
    ...    ADC scale factor should read back 2.0 after FC16 write, got ${readback}
    [Teardown]    Write Multiple Registers    address=${104}    values=${[${original_high}, ${original_low}]}

FC03 Read Version Holding Register Mirror
    [Documentation]    FC03 read of version holding register mirror (300-302) should match FC04 input registers.
    ${ir_major}=    Read Input Register    address=${100}
    ${hr_major}=    Read Holding Register    address=${300}
    Should Be Equal As Integers    ${ir_major}    ${hr_major}
    ...    Version major holding register mirror should match input register

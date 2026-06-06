*** Settings ***
Documentation    I2C expander tests — verify PCF8574 (DO0-DO7) and PCF8574A (DO8-DO15)
...              via AD3 I2C snoop and AD3 I2C master read.
...
...              Background:
...              - PCF8574  at 0x20 controls DO0-DO7  (lower byte of BSP_I2CDO_Write)
...              - PCF8574A at 0x21 controls DO8-DO15 (upper byte of BSP_I2CDO_Write)
...
...              Tags: hardware, i2c, positive

Resource    ../../resources/common.resource
Resource    ../../resources/ad3.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    i2c    positive


*** Variables ***
${PCF8574_ADDR}     ${32}     # 0x20
${PCF8574A_ADDR}    ${33}     # 0x21


*** Test Cases ***
Write DO0 ON And Verify I2C Snoop Captures PCF8574 Write
    [Documentation]    Write DO0 coil ON and verify AD3 I2C snoop captures write to PCF8574 (0x20)
    ...                with bit 0 set (data byte = 0x01).
    ${records}=    Snoop I2C And Write Coil    coil_address=${0}    coil_value=${True}
    Assert I2C Write Captured
    ...    records=${records}
    ...    expected_addr=${PCF8574_ADDR}
    ...    expected_data_byte=${1}
    [Teardown]    Write Coil    address=${0}    value=${False}

Write DO8 ON And Verify I2C Snoop Captures PCF8574A Write
    [Documentation]    Write DO8 coil ON and verify AD3 I2C snoop captures write to PCF8574A (0x21)
    ...                with bit 0 set (data byte = 0x01).
    ${records}=    Snoop I2C And Write Coil    coil_address=${8}    coil_value=${True}
    Assert I2C Write Captured
    ...    records=${records}
    ...    expected_addr=${PCF8574A_ADDR}
    ...    expected_data_byte=${1}
    [Teardown]    Write Coil    address=${8}    value=${False}

Write All DO0-DO7 ON And Verify PCF8574 Via AD3 I2C Master
    [Documentation]    Write DO0-DO7 all ON via Modbus and verify PCF8574 reads 0xFF via AD3 I2C master.
    # Write all lower 8 coils ON
    ${values}=    Create List
    FOR    ${_}    IN RANGE    8
        Append To List    ${values}    ${True}
    END
    Write Multiple Coils    address=${0}    values=${values}
    Sleep    30ms
    ${byte}=    Read PCF8574 Via AD3    pcf8574_addr=${PCF8574_ADDR}
    Should Be Equal As Integers    ${byte}    ${255}
    ...    PCF8574 should read 0xFF when all DO0-DO7 are ON
    [Teardown]    Write Multiple Coils    address=${0}    values=${[False, False, False, False, False, False, False, False]}

Write All DO8-DO15 ON And Verify PCF8574A Via AD3 I2C Master
    [Documentation]    Write DO8-DO15 all ON via Modbus and verify PCF8574A reads 0xFF.
    ${values}=    Create List
    FOR    ${_}    IN RANGE    8
        Append To List    ${values}    ${True}
    END
    Write Multiple Coils    address=${8}    values=${values}
    Sleep    30ms
    ${byte}=    Read PCF8574A Via AD3    pcf8574a_addr=${PCF8574A_ADDR}
    Should Be Equal As Integers    ${byte}    ${255}
    ...    PCF8574A should read 0xFF when all DO8-DO15 are ON
    [Teardown]    Write Multiple Coils    address=${8}    values=${[False, False, False, False, False, False, False, False]}

Write Alternating Pattern And Verify Both Expanders Via AD3 I2C Master
    [Documentation]    Write 0xAA55 pattern: PCF8574 should read 0x55, PCF8574A should read 0xAA.
    # DO0-DO7 = 0x55 (01010101), DO8-DO15 = 0xAA (10101010)
    ${values}=    Create List
    FOR    ${i}    IN RANGE    16
        ${bit}=    Evaluate    bool((0xAA55 >> ${i}) & 1)
        Append To List    ${values}    ${bit}
    END
    Write Multiple Coils    address=${0}    values=${values}
    Sleep    30ms
    ${pcf8574_byte}=    Read PCF8574 Via AD3    pcf8574_addr=${PCF8574_ADDR}
    ${pcf8574a_byte}=    Read PCF8574A Via AD3    pcf8574a_addr=${PCF8574A_ADDR}
    Should Be Equal As Integers    ${pcf8574_byte}    ${85}     # 0x55
    ...    PCF8574 should read 0x55 for DO0-DO7 pattern
    Should Be Equal As Integers    ${pcf8574a_byte}    ${170}   # 0xAA
    ...    PCF8574A should read 0xAA for DO8-DO15 pattern
    [Teardown]    Write Multiple Coils    address=${0}    values=${[False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False]}

Walking Bit Test Verifies Correct Expander Addressed
    [Documentation]    Set each DO individually and verify correct I2C byte sent to correct expander.
    FOR    ${i}    IN RANGE    8
        # Test DO0-DO7 (PCF8574)
        Write Coil    address=${i}    value=${True}
        Sleep    20ms
        ${byte}=    Read PCF8574 Via AD3    pcf8574_addr=${PCF8574_ADDR}
        ${expected}=    Evaluate    1 << ${i}
        Should Be Equal As Integers    ${byte}    ${expected}
        ...    PCF8574 byte wrong for DO${i}: expected 0x${expected:02X} got 0x${byte:02X}
        Write Coil    address=${i}    value=${False}
    END
    FOR    ${i}    IN RANGE    8
        # Test DO8-DO15 (PCF8574A)
        ${coil}=    Evaluate    ${i} + 8
        Write Coil    address=${coil}    value=${True}
        Sleep    20ms
        ${byte}=    Read PCF8574A Via AD3    pcf8574a_addr=${PCF8574A_ADDR}
        ${expected}=    Evaluate    1 << ${i}
        Should Be Equal As Integers    ${byte}    ${expected}
        ...    PCF8574A byte wrong for DO${coil}: expected 0x${expected:02X} got 0x${byte:02X}
        Write Coil    address=${coil}    value=${False}
    END

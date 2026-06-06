*** Settings ***
Documentation    Digital Output tests — verify DO0-DO15 via Modbus TCP coils and
...              confirm physical state via AD3 DIO inputs and I2C snoop.
...
...              Tags: hardware, digital_io, positive

Resource    ../../resources/common.resource
Resource    ../../resources/ad3.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    digital_io    positive


*** Test Cases ***
Write Single Digital Output ON And Verify Via AD3 DIO
    [Documentation]    Write DO0 coil ON via Modbus TCP and verify AD3 DIO 8 reads high.
    Write Coil    address=${0}    value=${True}
    Sleep    30ms
    ${state}=    Read Digital Output State Via AD3    do_index=${0}
    Should Be True    ${state}    DO0 should be HIGH after writing coil ON

Write Single Digital Output OFF And Verify Via AD3 DIO
    [Documentation]    Write DO0 coil OFF via Modbus TCP and verify AD3 DIO 8 reads low.
    Write Coil    address=${0}    value=${False}
    Sleep    30ms
    ${state}=    Read Digital Output State Via AD3    do_index=${0}
    Should Not Be True    ${state}    DO0 should be LOW after writing coil OFF

Write All Digital Outputs ON Pattern
    [Documentation]    Write all 16 DO coils ON (0xFFFF) and verify via Modbus readback.
    Set All Digital Outputs Via Modbus    pattern_16bit=${65535}
    FOR    ${i}    IN RANGE    16
        ${state}=    Read Coil    address=${i}
        Should Be True    ${state}    DO${i} should be ON
    END

Write All Digital Outputs OFF Pattern
    [Documentation]    Write all 16 DO coils OFF (0x0000) and verify via Modbus readback.
    Set All Digital Outputs Via Modbus    pattern_16bit=${0}
    FOR    ${i}    IN RANGE    16
        ${state}=    Read Coil    address=${i}
        Should Not Be True    ${state}    DO${i} should be OFF

Write Alternating Pattern 0xAAAA
    [Documentation]    Write alternating pattern (even bits ON) and verify readback.
    Set All Digital Outputs Via Modbus    pattern_16bit=${43690}    # 0xAAAA
    FOR    ${i}    IN RANGE    16
        ${expected}=    Evaluate    bool((43690 >> ${i}) & 1)
        ${actual}=    Read Coil    address=${i}
        Should Be Equal    ${actual}    ${expected}    DO${i} pattern mismatch

Write Alternating Pattern 0x5555
    [Documentation]    Write alternating pattern (odd bits ON) and verify readback.
    Set All Digital Outputs Via Modbus    pattern_16bit=${21845}    # 0x5555
    FOR    ${i}    IN RANGE    16
        ${expected}=    Evaluate    bool((21845 >> ${i}) & 1)
        ${actual}=    Read Coil    address=${i}
        Should Be Equal    ${actual}    ${expected}    DO${i} pattern mismatch

Walking Bit Test All Digital Outputs
    [Documentation]    Set each DO individually and verify only that DO is ON.
    FOR    ${i}    IN RANGE    16
        Set All Digital Outputs Via Modbus    pattern_16bit=${0}
        Write Coil    address=${i}    value=${True}
        Sleep    20ms
        FOR    ${j}    IN RANGE    16
            ${expected}=    Evaluate    ${i} == ${j}
            ${actual}=    Read Coil    address=${j}
            Should Be Equal    ${actual}    ${expected}
            ...    Walking bit: DO${j} state wrong when DO${i} is set
        END
    END

DO State Persists After Modbus Reconnect
    [Documentation]    Write DO0 ON, disconnect, reconnect, verify state preserved.
    Write Coil    address=${0}    value=${True}
    Disconnect Modbus TCP
    Sleep    500ms
    Connect Modbus TCP    ${DUT_HOST}    ${DUT_PORT}    ${DUT_UNIT_ID}    ${DUT_TIMEOUT}
    ${state}=    Read Coil    address=${0}
    Should Be True    ${state}    DO0 state should persist after reconnect
    [Teardown]    Write Coil    address=${0}    value=${False}

Verify DO0 Via Both AD3 DIO And I2C Snoop
    [Documentation]    Write DO0 ON and verify via AD3 DIO readback AND I2C snoop.
    ${records}=    Snoop I2C And Write Coil    coil_address=${0}    coil_value=${True}
    # Verify AD3 DIO readback
    ${dio_state}=    Read Digital Output State Via AD3    do_index=${0}
    Should Be True    ${dio_state}    DO0 should be HIGH via AD3 DIO
    # Verify I2C snoop captured write to PCF8574 (0x20) with bit 0 set
    Assert I2C Write Captured    records=${records}    expected_addr=${32}    expected_data_byte=${1}
    [Teardown]    Write Coil    address=${0}    value=${False}

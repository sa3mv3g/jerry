*** Settings ***
Documentation    Modbus TCP coil tests — verify FC01, FC05, FC15 function codes
...              for digital outputs, digital input mirrors, and PWM enable coils.
...
...              Migrated from tests/integration/test_coils.py to Robot Framework.
...
...              Tags: hardware, modbus_tcp, positive

Resource    ../../resources/common.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    modbus_tcp    positive


*** Test Cases ***
FC01 Read Single Coil Returns Valid State
    [Documentation]    FC01 read of coil 0 should return True or False without error.
    ${state}=    Read Coil    address=${0}
    Should Be True    '${state}' in ['True', 'False']
    ...    Coil 0 should return a valid boolean state

FC01 Read All Digital Output Coils
    [Documentation]    FC01 read of all 16 DO coils should succeed.
    FOR    ${i}    IN RANGE    16
        ${state}=    Read Coil    address=${i}
        Should Be True    '${state}' in ['True', 'False']
        ...    Coil ${i} should return a valid boolean state
    END

FC05 Write Single Coil ON And Read Back
    [Documentation]    FC05 write coil 0 ON, then FC01 read back should return True.
    Write Coil    address=${0}    value=${True}
    ${state}=    Read Coil    address=${0}
    Should Be True    ${state}    Coil 0 should be ON after FC05 write
    [Teardown]    Write Coil    address=${0}    value=${False}

FC05 Write Single Coil OFF And Read Back
    [Documentation]    FC05 write coil 0 OFF, then FC01 read back should return False.
    Write Coil    address=${0}    value=${False}
    ${state}=    Read Coil    address=${0}
    Should Not Be True    ${state}    Coil 0 should be OFF after FC05 write

FC15 Write Multiple Coils All ON
    [Documentation]    FC15 write all 16 DO coils ON, then FC01 read back all should be True.
    ${values}=    Create List
    FOR    ${_}    IN RANGE    16
        Append To List    ${values}    ${True}
    END
    Write Multiple Coils    address=${0}    values=${values}
    FOR    ${i}    IN RANGE    16
        ${state}=    Read Coil    address=${i}
        Should Be True    ${state}    Coil ${i} should be ON after FC15 write
    END
    [Teardown]    Write Multiple Coils    address=${0}    values=${[False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False]}

FC15 Write Multiple Coils Alternating Pattern
    [Documentation]    FC15 write alternating pattern and verify readback.
    ${values}=    Create List
    FOR    ${i}    IN RANGE    16
        ${bit}=    Evaluate    bool(${i} % 2 == 0)
        Append To List    ${values}    ${bit}
    END
    Write Multiple Coils    address=${0}    values=${values}
    FOR    ${i}    IN RANGE    16
        ${expected}=    Evaluate    bool(${i} % 2 == 0)
        ${actual}=    Read Coil    address=${i}
        Should Be Equal    ${actual}    ${expected}    Coil ${i} pattern mismatch
    END

FC01 Read PWM Enable Coils
    [Documentation]    FC01 read of PWM enable coils (24-27) should succeed.
    FOR    ${i}    IN RANGE    4
        ${coil}=    Evaluate    24 + ${i}
        ${state}=    Read Coil    address=${coil}
        Should Be True    '${state}' in ['True', 'False']
        ...    PWM enable coil ${coil} should return valid state
    END

FC02 Read All Discrete Inputs
    [Documentation]    FC02 read of all 8 discrete inputs should succeed.
    FOR    ${i}    IN RANGE    8
        ${state}=    Read Discrete Input    address=${i}
        Should Be True    '${state}' in ['True', 'False']
        ...    Discrete input ${i} should return valid state
    END

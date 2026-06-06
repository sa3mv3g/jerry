*** Settings ***
Documentation    Digital Input tests — verify DI0-DI7 via AD3 DIO stimulus and
...              Modbus TCP discrete input / mirror coil readback.
...
...              Tags: hardware, digital_io, positive

Resource    ../../resources/common.resource
Resource    ../../resources/ad3.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    digital_io    positive


*** Test Cases ***
Drive Single Digital Input High And Read Via Modbus
    [Documentation]    Drive DI0 high via AD3 DIO 0 and verify FC02 discrete input reads ON.
    Drive Digital Input High    di_index=${0}
    ${state}=    Read Discrete Input    address=${0}
    Should Be True    ${state}    DI0 should read HIGH after AD3 drives it high
    [Teardown]    Drive Digital Input Low    di_index=${0}

Drive Single Digital Input Low And Read Via Modbus
    [Documentation]    Drive DI0 low via AD3 DIO 0 and verify FC02 discrete input reads OFF.
    Drive Digital Input Low    di_index=${0}
    ${state}=    Read Discrete Input    address=${0}
    Should Not Be True    ${state}    DI0 should read LOW after AD3 drives it low

Drive All Digital Inputs High
    [Documentation]    Drive all 8 DIs high via AD3 DIO 0-7 and verify all FC02 inputs ON.
    Drive All Digital Inputs    pattern=${255}    # 0xFF
    FOR    ${i}    IN RANGE    8
        ${state}=    Read Discrete Input    address=${i}
        Should Be True    ${state}    DI${i} should be HIGH
    END
    [Teardown]    Drive All Digital Inputs    pattern=${0}

Drive All Digital Inputs Low
    [Documentation]    Drive all 8 DIs low via AD3 DIO 0-7 and verify all FC02 inputs OFF.
    Drive All Digital Inputs    pattern=${0}
    FOR    ${i}    IN RANGE    8
        ${state}=    Read Discrete Input    address=${i}
        Should Not Be True    ${state}    DI${i} should be LOW
    END

Walking Bit Test All Digital Inputs
    [Documentation]    Drive each DI individually and verify only that DI reads ON.
    FOR    ${i}    IN RANGE    8
        Drive All Digital Inputs    pattern=${0}
        Drive Digital Input High    di_index=${i}
        FOR    ${j}    IN RANGE    8
            ${expected}=    Evaluate    ${i} == ${j}
            ${actual}=    Read Discrete Input    address=${j}
            Should Be Equal    ${actual}    ${expected}
            ...    Walking bit: DI${j} state wrong when DI${i} is driven high
        END
    END
    [Teardown]    Drive All Digital Inputs    pattern=${0}

DI Mirror Coil Matches Discrete Input
    [Documentation]    Verify DI mirror coils (FC01 address 16-23) match FC02 discrete inputs.
    Drive All Digital Inputs    pattern=${170}    # 0xAA = alternating
    FOR    ${i}    IN RANGE    8
        ${di_state}=    Read Discrete Input    address=${i}
        ${mirror_state}=    Read Coil    address=${16 + ${i}}
        Should Be Equal    ${di_state}    ${mirror_state}
        ...    DI${i} mirror coil (addr ${16 + ${i}}) does not match discrete input
    END
    [Teardown]    Drive All Digital Inputs    pattern=${0}

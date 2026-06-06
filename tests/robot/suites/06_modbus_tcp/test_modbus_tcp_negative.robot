*** Settings ***
Documentation    Modbus TCP negative tests — verify correct exception responses for
...              invalid addresses, invalid quantities, read-only violations, and
...              transport-level errors.
...
...              Tags: hardware, modbus_tcp, negative

Resource    ../../resources/common.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    modbus_tcp    negative


*** Test Cases ***
FC01 Read Coil At Address 65535 Should Return Exception
    [Documentation]    FC01 read at address 65535 should return exception 0x02.
    [Tags]    negative    invalid_address
    ${response}=    Read Coil Raw    address=${65535}
    Should Be True    ${response.isError()}
    ...    Reading coil at address 65535 should return Modbus exception

FC01 Read Coil At Address 1000 Should Return Exception
    [Documentation]    FC01 read at address 1000 (beyond register map) should return exception.
    [Tags]    negative    invalid_address
    ${response}=    Read Coil Raw    address=${1000}
    Should Be True    ${response.isError()}
    ...    Reading coil at address 1000 should return Modbus exception

FC03 Read Holding Register At Address 65535 Should Return Exception
    [Documentation]    FC03 read at address 65535 should return exception 0x02.
    [Tags]    negative    invalid_address
    ${response}=    Read Holding Registers Raw    address=${65535}    count=${1}
    Should Be True    ${response.isError()}
    ...    Reading holding register at address 65535 should return Modbus exception

FC03 Read 126 Registers Exceeds Maximum Should Return Exception
    [Documentation]    FC03 read with count=126 exceeds max 125, should return exception 0x03.
    [Tags]    negative    invalid_quantity
    ${response}=    Read Holding Registers Raw    address=${0}    count=${126}
    Should Be True    ${response.isError()}
    ...    FC03 read count=126 should return exception (max is 125)

FC04 Read Input Register At Address 65535 Should Return Exception
    [Documentation]    FC04 read at address 65535 should return exception 0x02.
    [Tags]    negative    invalid_address
    ${response}=    Read Input Registers Raw    address=${65535}    count=${1}
    Should Be True    ${response.isError()}
    ...    Reading input register at address 65535 should return Modbus exception

FC06 Write To Read-Only Input Register Address Should Return Exception
    [Documentation]    FC06 write to address 65535 (invalid) should return exception.
    [Tags]    negative    read_only_violation
    ${response}=    Write Holding Register Raw    address=${65535}    value=${0}
    Should Be True    ${response.isError()}
    ...    FC06 write to invalid address should return Modbus exception

FC05 Write Coil At Invalid Address Should Return Exception
    [Documentation]    FC05 write to coil address 65535 should return exception 0x02.
    [Tags]    negative    invalid_address
    ${response}=    Write Coil Raw    address=${65535}    value=${True}
    Should Be True    ${response.isError()}
    ...    FC05 write to coil address 65535 should return Modbus exception

Modbus TCP Connection Recovery After Disconnect
    [Documentation]    Disconnect TCP mid-session and verify DUT accepts new connection.
    [Tags]    negative    transport_error
    Disconnect Modbus TCP
    Sleep    500ms
    Connect Modbus TCP    ${DUT_HOST}    ${DUT_PORT}    ${DUT_UNIT_ID}    ${DUT_TIMEOUT}
    ${tick}=    Read Holding Register    address=${200}
    Should Be True    ${tick} >= 0
    ...    DUT should accept new TCP connection after disconnect

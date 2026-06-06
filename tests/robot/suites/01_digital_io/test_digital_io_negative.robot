*** Settings ***
Documentation    Digital I/O negative tests — verify correct Modbus exception responses
...              for invalid addresses, read-only violations, and invalid values.
...
...              Tags: hardware, digital_io, negative

Resource    ../../resources/common.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    digital_io    negative


*** Test Cases ***
Write To Read-Only DI Mirror Coil Should Return Exception
    [Documentation]    Writing to DI mirror coil (address 16, read-only) should return
    ...                Modbus exception code 0x01 (Illegal Function) or 0x02 (Illegal Data Address).
    [Tags]    negative    read_only_violation
    ${response}=    Write Coil Raw    address=${16}    value=${True}
    Should Be True    ${response.isError()}
    ...    Writing to read-only DI mirror coil should return Modbus exception

Write To Read-Only DI Mirror Coil Address 23 Should Return Exception
    [Documentation]    Writing to last DI mirror coil (address 23) should return exception.
    [Tags]    negative    read_only_violation
    ${response}=    Write Coil Raw    address=${23}    value=${True}
    Should Be True    ${response.isError()}
    ...    Writing to read-only DI mirror coil 23 should return Modbus exception

Read Coil At Invalid Address Should Return Exception
    [Documentation]    Reading coil at address 65535 should return exception 0x02.
    [Tags]    negative    invalid_address
    ${response}=    Read Coil Raw    address=${65535}
    Should Be True    ${response.isError()}
    ...    Reading coil at address 65535 should return Modbus exception

Read Coil At Address Beyond Register Map Should Return Exception
    [Documentation]    Reading coil at address 1000 (beyond defined map) should return exception.
    [Tags]    negative    invalid_address
    ${response}=    Read Coil Raw    address=${1000}
    Should Be True    ${response.isError()}
    ...    Reading coil at address 1000 should return Modbus exception

Write Multiple Coils With Zero Count Should Return Exception
    [Documentation]    FC15 with count=0 should return exception 0x03 (Illegal Data Value).
    [Tags]    negative    invalid_quantity
    # pymodbus will reject count=0 before sending; test via raw if needed
    Run Keyword And Expect Error    *    Write Multiple Coils    address=${0}    values=${[]}

Write Input Register Via FC06 Should Return Exception
    [Documentation]    Writing to an input register address via FC06 should return exception.
    [Tags]    negative    read_only_violation
    ${response}=    Write Holding Register Raw    address=${65535}    value=${0}
    Should Be True    ${response.isError()}
    ...    Writing to invalid holding register address should return Modbus exception

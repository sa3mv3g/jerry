*** Settings ***
Documentation    I2C negative tests — verify bus fault handling, NACK on non-existent
...              addresses, and firmware recovery after I2C errors.
...
...              Tags: hardware, i2c, negative

Resource    ../../resources/common.resource
Resource    ../../resources/ad3.resource
Resource    ../../resources/modbus_tcp.resource

Suite Setup       Suite Hardware Setup
Suite Teardown    Suite Hardware Teardown
Test Tags         hardware    i2c    negative


*** Test Cases ***
AD3 I2C Master Read From Non-Existent Address Should Get NACK
    [Documentation]    AD3 I2C master reads from address 0x77 (no device present).
    ...                Should receive NACK (RuntimeError from AD3Library).
    [Tags]    negative    bus_fault
    Run Keyword And Expect Error    *NACK*
    ...    I2C Read    address=${119}    count=${1}    # 0x77

AD3 I2C Master Read From Non-Existent Address 0x10 Should Get NACK
    [Documentation]    AD3 I2C master reads from address 0x10 (no device present).
    [Tags]    negative    bus_fault
    Run Keyword And Expect Error    *NACK*
    ...    I2C Read    address=${16}    count=${1}    # 0x10

Firmware Recovers After I2C Bus Error
    [Documentation]    After an I2C bus error (NACK from AD3), verify the STM32 firmware
    ...                still responds to Modbus TCP (no firmware crash or hang).
    [Tags]    negative    bus_fault
    # Trigger a NACK by reading non-existent address
    Run Keyword And Ignore Error    I2C Read    address=${119}    count=${1}
    Sleep    100ms
    # Verify DUT still responds to Modbus
    ${tick}=    Read Holding Register    address=${200}
    Should Be True    ${tick} >= 0
    ...    Firmware should still respond to Modbus after I2C bus error

Conflicting I2C Master Write Does Not Crash Firmware
    [Documentation]    AD3 writes to PCF8574 address (0x20) as I2C master while STM32
    ...                also owns the bus. Verify firmware handles bus arbitration gracefully
    ...                and Modbus remains responsive.
    [Tags]    negative    bus_fault
    # AD3 writes to PCF8574 (may conflict with STM32 I2C4 master)
    Run Keyword And Ignore Error    I2C Write    address=${32}    data_bytes=${b'\x00'}
    Sleep    100ms
    # Verify DUT still responds
    ${tick}=    Read Holding Register    address=${200}
    Should Be True    ${tick} >= 0
    ...    Firmware should still respond after conflicting I2C master write

Write DO Coil After I2C Error Still Works
    [Documentation]    After an I2C bus error, verify DO coil writes still function correctly.
    [Tags]    negative    bus_fault
    # Trigger error
    Run Keyword And Ignore Error    I2C Read    address=${119}    count=${1}
    Sleep    100ms
    # Normal DO write should still work
    Write Coil    address=${0}    value=${True}
    Sleep    30ms
    ${state}=    Read Coil    address=${0}
    Should Be True    ${state}    DO0 should be ON after I2C error recovery
    [Teardown]    Write Coil    address=${0}    value=${False}

*** Settings ***
Documentation    Modbus RTU test stubs — FUTURE WORK.
...
...              These tests require a vModbusRtuTask in the Jerry firmware.
...              The current firmware only supports Modbus TCP (vModbusTask).
...
...              Prerequisites before enabling:
...              1. Add vModbusRtuTask to application/src/ wired to a UART peripheral
...              2. Wire AD3 UART TX/RX to the STM32 UART RX/TX pins
...              3. Update hardware_config.yaml with the correct serial port
...
...              Tags: hardware, modbus_rtu, future

Resource    ../../resources/common.resource

Suite Setup       Log    Modbus RTU suite skipped — firmware RTU task not yet implemented    WARN
Test Tags         hardware    modbus_rtu    future


*** Test Cases ***
Modbus RTU Suite Is Future Work
    [Documentation]    Placeholder test confirming RTU suite is pending firmware implementation.
    ...
    ...                To activate: implement vModbusRtuTask in firmware, then replace this
    ...                file with the actual RTU test implementations from the architecture plan.
    [Tags]    future
    Skip    Modbus RTU requires vModbusRtuTask in firmware (not yet implemented)

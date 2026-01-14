# Project Requirements

| Property | Value |
| :--- | :--- |
| Version | 0.1 |
| Author | sanmveg saini |

# Microcontroller requirements 

This application is intended to work on STM32H5xxx series of device. However, to mange the supply shortage, in future, this application can also work on different vendor's micro-controller. However, the device should have the following capabilities:
1. Atleast 2 UART/USART
2. Atleast 1 SPI
3. atleast 1 I2C
4. atleast 1 CAN-FD controller
5. atleast 1 10/100 mbps ethernet controller
6. atleast USB-HOST/USB-OTG.
7. Debug via JTAG/SWD.
8. internal flash to store application
9.  internal flash should be dual bank, to support FOTA usecase. 
10. atleast 32 Output pins.
11. atleast 16 input pins.
12. atleast 4 PWM output pins.
13. atleast 4 analog input.
14. ADC should be atleast 12bits, >1MSPS, ENOB of 10 at max sampling speed.
15. Atleast AES engine.
16. atleast TRNG support.
17. atleast HASH calculation support.
18. atleast private key burning support.

## Software architecture requirements

Here `application` or `app` refers to the firmware that goes into the memory of microcontroller and not any Python application. Any python application will be either referred as `python application` or `python app` or `tool`. When referring to the `app`, it does not includes 3p libraries, rather, these 3p libraries are dependencies of the `app` and are referred as `3p libs` or `3p libraries` or `3p dependencies`.


This project will be architected the way below:

1. for the firmware, only C and assembly language will be used. However, `3p libraries` can be of any language.
2. `3p libraries` will be statically linked. 
3. For the tools required to test or automate this firmware will only be written in Python.
4. Build system will be strictly CMake.
5. `CMake` to generate `ninja` files.
6. `clang-format`is to be used to maintain consistent formatting of C/h files of the app. 
7. ruff is to be use for formatting of the python files.
8. use `cppcheck` to perform static analysis. It must do CERT-C with MISRA enabled on all the `app` c/h files. 
9. use Lizard for calculating the HIS metrics of the `app`.
10. multiple compilers must be supported which is based on the microcontroller's vendor.
11. For any compiler, all warnings must be treated as errors and all warnings must be enabled.
12. C/H files should follow [Google CPP Coding style](refs/cpp_coding_style.md).
13. python files should follow [Google Python Coding style](refs/python_coding_style.md).
14. shell script should follow [Google Shell Coding style](refs/shell_style_guide.md) 
15. `pylint` must be use for linting of the python files.
16. python version must be >=3.10.X (64bits)
17. `FreeRTOS` must be used as the OS of this `application`. 
18. `FreeRTOS` must statically allocate all of its object.
19. In entire `app`, there must be no dynamic memory allocation. 
20. [Unity for c](https://github.com/ThrowTheSwitch/Unity) must be used as the testing framework for the `app`. 
22. `pytest` must be used for testing `python tools`.
23. `uv` must be used for managing python projects.
24. All `app` dependencies must be managed by `CMake`.
25. All `tools` dependencies must be handled by `uv`.
26. `app` and `tools` documentation should be managed by `doxygen`.
27. All `app` and `tools` testing code should be in `test` folder.
28. All build artifact `app` and `tools` should be placed in bulid folder.


### Directory structure

```
jerry
├── .git
├── .gitignore
├── .vscode
├── config
│   └── <cmake configuration>
│   └── <pylint configuration>
│   └── <ruff configuration>
│   └── <cppcheck configuration>
│   └── <lizard configuration>
│   └── <clang-format configuration>
├── application
│   ├── <dependancy 1>
│   ├── <dependancy 2>
│   ├── <dependancy 3>
│   ├── <dependancy n>
│   ├── src/
│   │   └── .c/.s files
│   ├── inc/
│   │   └── .h files
│   ├── CMSIS_5/
│   ├── bsp
│   │   └── <vendor name, eg. stm, ti, nxp....>
│   │       ├── <vendor bsp files>
│   │       └── <compiler configuration>
│   │           └── toolchain.cmake
│   └── CMakeLists.txt
├── tools
│   ├── tool_1/
│   └── tool_n/
├── test
│   ├── app_test/
│   │   ├── app test 1/
│   │   └── app test 2/
│   ├── tool_1_test/
│   └── tool_n_test/
├── build/
├── refs/
├── docs/
└── CMakeLists.txt

```

To enable application to be independent of the vendor, `CMSIS_5` is used.
Since, CMake configures the `app`, in config folder, there should be cmake file that selects which microcontroller vendor has be to used.

## Application requirements

1. Must have task to monitor stack overflow. 
2. Should have task to track stack usage.
3. Must implement secure FOTA.
4. Must implement logging over UART. This uart is called log_uart. This part must have baudrate of 115200 bps.
5. Must implement MODBUS-TCP/IP which would use the given ethernet port.
6. Must implement MODBUS-RTU over UART. This uart should not be same as log_uart and is called modbus_uart. uart config should be 19200bps, 2 stop bits, no parity.
7. Should have event logging over usb memory stick.
8. Can have configuration/info save on usb memory stick.
9. must take digital input from 8 digital input pins and must store their value in memory. These values must be accessible from MODBUS-TCP/IP and MODBUS-RTU.
10. must control 16 digital output on pins based on some internal memory. This internal memory must be accessible from MODBUS-TCP/IP and MODBUS-RTU.
11. must have 4 ADC input and there value must be stored in internal memory and these values must be accessible from MODBUS-TCP/IP and MODBUS-RTU.
12. must have 4 PWM output pins and each PWM's duty cycle and frequency must be controlled from internal memory and its values must be accessible from MODBUS-TCP/IP and MODBUS-RTU.
13. code should be readable, scalable and testable.

# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.

set(VENDOR "stm32h563" CACHE STRING "Microcontroller Vendor")
set(ENV{VENDOR} "${VENDOR}")

set(BSP_DIR "${CMAKE_SOURCE_DIR}/application/bsp")

if(VENDOR STREQUAL "stm32h563")
    set(CMAKE_TOOLCHAIN_FILE "${BSP_DIR}/toolchain.cmake")
else()
    message(FATAL_ERROR "Unsupported Vendor: ${VENDOR}")
endif()

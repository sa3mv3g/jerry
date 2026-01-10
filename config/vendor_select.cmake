# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.

set(VENDOR "stm" CACHE STRING "Microcontroller Vendor")
set(ENV{VENDOR} "${VENDOR}")

if(VENDOR STREQUAL "stm")
    set(BSP_DIR "${CMAKE_SOURCE_DIR}/application/bsp/stm")
    set(CMAKE_TOOLCHAIN_FILE "${BSP_DIR}/toolchain.cmake")
else()
    message(FATAL_ERROR "Unsupported Vendor: ${VENDOR}")
endif()

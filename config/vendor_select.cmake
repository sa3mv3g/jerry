# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.

set(CHIPS_CFG "stm32h563" CACHE STRING "Target chip configuration")
set(ENV{CHIPS_CFG} "${CHIPS_CFG}")

set(BSP_DIR "${CMAKE_SOURCE_DIR}/application/bsp")

if(CHIPS_CFG STREQUAL "stm32h563" OR CHIPS_CFG STREQUAL "stm32f429")
    set(CMAKE_TOOLCHAIN_FILE "${BSP_DIR}/toolchain.cmake")
else()
    message(FATAL_ERROR "Unsupported chip: ${CHIPS_CFG}. Supported chips: stm32h563, stm32f429")
endif()

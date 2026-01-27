# =============================================================================
# Modbus Library CMakeLists.txt
# =============================================================================
# Custom Modbus library supporting RTU, ASCII, and TCP/IP protocols
# for STM32H563 embedded systems with FreeRTOS and lwIP.
#
# Copyright (c) 2026
# =============================================================================

cmake_minimum_required(VERSION 3.22)

# -----------------------------------------------------------------------------
# Library Configuration Options
# -----------------------------------------------------------------------------
option(MODBUS_ENABLE_RTU "Enable Modbus RTU protocol support" ON)
option(MODBUS_ENABLE_ASCII "Enable Modbus ASCII protocol support" ON)
option(MODBUS_ENABLE_TCP "Enable Modbus TCP/IP protocol support" ON)
option(MODBUS_BUILD_TESTS "Build Modbus library unit tests" OFF)

# -----------------------------------------------------------------------------
# Source Files
# -----------------------------------------------------------------------------

# Core sources (always included)
set(MODBUS_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/modbus_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/modbus_pdu.c
)

# Utility sources (always included)
set(MODBUS_UTIL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/util/modbus_crc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/util/modbus_lrc.c
)

# Protocol sources (conditionally included)
set(MODBUS_PROTOCOL_SOURCES "")

if(MODBUS_ENABLE_RTU)
    list(APPEND MODBUS_PROTOCOL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/modbus_rtu.c
    )
endif()

if(MODBUS_ENABLE_ASCII)
    list(APPEND MODBUS_PROTOCOL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/modbus_ascii.c
    )
endif()

if(MODBUS_ENABLE_TCP)
    list(APPEND MODBUS_PROTOCOL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/modbus_tcp.c
    )
endif()

# Combine all sources
set(MODBUS_SOURCES
    ${MODBUS_CORE_SOURCES}
    ${MODBUS_UTIL_SOURCES}
    ${MODBUS_PROTOCOL_SOURCES}
)

# -----------------------------------------------------------------------------
# Library Target
# -----------------------------------------------------------------------------
add_library(modbus_stack STATIC ${MODBUS_SOURCES})

# Include directories
target_include_directories(modbus_stack
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# -----------------------------------------------------------------------------
# Compile Definitions
# -----------------------------------------------------------------------------
target_compile_definitions(modbus_stack
    PUBLIC
        $<$<BOOL:${MODBUS_ENABLE_RTU}>:MODBUS_ENABLE_RTU=1>
        $<$<NOT:$<BOOL:${MODBUS_ENABLE_RTU}>>:MODBUS_ENABLE_RTU=0>
        $<$<BOOL:${MODBUS_ENABLE_ASCII}>:MODBUS_ENABLE_ASCII=1>
        $<$<NOT:$<BOOL:${MODBUS_ENABLE_ASCII}>>:MODBUS_ENABLE_ASCII=0>
        $<$<BOOL:${MODBUS_ENABLE_TCP}>:MODBUS_ENABLE_TCP=1>
        $<$<NOT:$<BOOL:${MODBUS_ENABLE_TCP}>>:MODBUS_ENABLE_TCP=0>
)

# -----------------------------------------------------------------------------
# Compiler Options
# -----------------------------------------------------------------------------
target_compile_options(modbus_stack
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<CONFIG:Debug>:-g3 -O0>
        $<$<CONFIG:Release>:-O2>
)

# -----------------------------------------------------------------------------
# Dependencies (when building for embedded target)
# -----------------------------------------------------------------------------
# Note: These dependencies are optional and only linked when available
# The library is designed to work standalone for unit testing

# Check if lwIP is available (for TCP support)
if(TARGET lwip_stack AND MODBUS_ENABLE_TCP)
    target_link_libraries(modbus_stack
        PRIVATE
            lwip_stack
    )
    target_compile_definitions(modbus_stack
        PRIVATE
            MODBUS_HAS_LWIP=1
    )
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
install(TARGETS modbus_stack
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/inc/
    DESTINATION include/modbus
    FILES_MATCHING PATTERN "*.h"
)

# -----------------------------------------------------------------------------
# Unit Tests (optional)
# -----------------------------------------------------------------------------
if(MODBUS_BUILD_TESTS)
    enable_testing()

    # Test executable would be added here
    # add_executable(modbus_tests
    #     tests/test_modbus_crc.c
    #     tests/test_modbus_pdu.c
    #     tests/test_modbus_rtu.c
    #     tests/test_modbus_ascii.c
    #     tests/test_modbus_tcp.c
    # )
    # target_link_libraries(modbus_tests modbus_stack)
    # add_test(NAME modbus_tests COMMAND modbus_tests)
endif()

# -----------------------------------------------------------------------------
# Print Configuration Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== Modbus Library Configuration ===")
message(STATUS "  RTU Protocol:   ${MODBUS_ENABLE_RTU}")
message(STATUS "  ASCII Protocol: ${MODBUS_ENABLE_ASCII}")
message(STATUS "  TCP Protocol:   ${MODBUS_ENABLE_TCP}")
message(STATUS "  Build Tests:    ${MODBUS_BUILD_TESTS}")
message(STATUS "====================================")
message(STATUS "")

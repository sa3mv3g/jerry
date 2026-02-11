# =============================================================================
# Modbus Library Unit Tests CMakeLists.txt
# =============================================================================
# Unity-based unit tests for the custom Modbus library
#
# Copyright (c) 2026
# =============================================================================

cmake_minimum_required(VERSION 3.22)
project(modbus_tests C)

# -----------------------------------------------------------------------------
# Unity Test Framework
# -----------------------------------------------------------------------------
# Unity is fetched from GitHub if not already available
include(FetchContent)

# Disable Unity config file requirement before fetching
set(UNITY_EXTENSION_FIXTURE OFF CACHE BOOL "" FORCE)
set(UNITY_EXTENSION_MEMORY OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)

# Set compile definition for Unity before making it available
add_compile_definitions(UNITY_INCLUDE_CONFIG_H=0)

FetchContent_MakeAvailable(unity)

# Copy unity_config.h to the Unity source directory so it can be found
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/unity_config.h
     DESTINATION ${unity_SOURCE_DIR}/src)

# -----------------------------------------------------------------------------
# Test Sources
# -----------------------------------------------------------------------------
set(TEST_SOURCES
    test_runner.c
    test_modbus_crc.c
    test_modbus_lrc.c
    test_modbus_pdu.c
    test_modbus_rtu.c
    test_modbus_ascii.c
    test_modbus_tcp.c
    test_modbus_callbacks.c
)

# -----------------------------------------------------------------------------
# Modbus Library Sources (for host compilation)
# -----------------------------------------------------------------------------
set(MODBUS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../application/dependencies/modbus/src/util/modbus_crc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../application/dependencies/modbus/src/util/modbus_lrc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../application/dependencies/modbus/src/core/modbus_pdu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../application/dependencies/modbus/src/protocol/modbus_rtu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../application/dependencies/modbus/src/protocol/modbus_ascii.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../application/dependencies/modbus/src/protocol/modbus_tcp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../application/dependencies/modbus/src/core/modbus_core.c
)

# -----------------------------------------------------------------------------
# Test Executable
# -----------------------------------------------------------------------------
add_executable(modbus_tests
    ${TEST_SOURCES}
    ${MODBUS_SOURCES}
)

target_include_directories(modbus_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../application/dependencies/modbus/inc
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(modbus_tests PRIVATE
    unity
)

# Enable all warnings
target_compile_options(modbus_tests PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

# Define test configuration
target_compile_definitions(modbus_tests PRIVATE
    MODBUS_ENABLE_RTU=1
    MODBUS_ENABLE_ASCII=1
    MODBUS_ENABLE_TCP=1
    UNITY_INCLUDE_CONFIG_H=0
)

# -----------------------------------------------------------------------------
# CTest Integration
# -----------------------------------------------------------------------------
enable_testing()
add_test(NAME modbus_unit_tests COMMAND modbus_tests)

# -----------------------------------------------------------------------------
# Custom Target for Running Tests
# -----------------------------------------------------------------------------
add_custom_target(run_modbus_tests
    COMMAND modbus_tests
    DEPENDS modbus_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running Modbus unit tests..."
)

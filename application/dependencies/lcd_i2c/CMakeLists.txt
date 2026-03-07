# =============================================================================
# LCD I2C Library CMakeLists.txt
# =============================================================================
# Library for controlling HD44780-compatible LCD displays via PCF8574 I2C
# I/O expander. Designed for embedded systems with no dynamic memory allocation.
#
# Features:
#   - Support for 16x2, 20x4, 16x4, and 20x2 LCD configurations
#   - Custom character support
#   - Backlight control
#   - Thread-safe design for FreeRTOS environments
#
# Usage:
#   add_subdirectory(dependencies/lcd_i2c)
#   target_link_libraries(your_target PRIVATE lcd_i2c)
#
# Copyright (c) 2026
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# -----------------------------------------------------------------------------
# Library Configuration Options
# -----------------------------------------------------------------------------
option(LCD_I2C_BUILD_TESTS "Build LCD I2C library unit tests" OFF)

# Library name
set(LIB_NAME lcd_i2c)

# -----------------------------------------------------------------------------
# Source Files
# -----------------------------------------------------------------------------
set(LCD_I2C_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lcd_i2c.c
)

# Header files (for IDE integration)
set(LCD_I2C_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/lcd_i2c.h
)

# -----------------------------------------------------------------------------
# Create Static Library
# -----------------------------------------------------------------------------
add_library(${LIB_NAME} STATIC
    ${LCD_I2C_SOURCES}
    ${LCD_I2C_HEADERS}
)

# Include directories
target_include_directories(${LIB_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

# -----------------------------------------------------------------------------
# Compiler Options
# -----------------------------------------------------------------------------
target_compile_options(${LIB_NAME}
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<CONFIG:Release>:-O2>
        $<$<NOT:$<CONFIG:Release>>:-g3 -O0>
)

# -----------------------------------------------------------------------------
# Unit Tests (optional)
# -----------------------------------------------------------------------------
if(LCD_I2C_BUILD_TESTS)
    enable_testing()
    # Test executable would be added here
    # add_executable(lcd_i2c_tests
    #     tests/test_lcd_i2c.c
    # )
    # target_link_libraries(lcd_i2c_tests ${LIB_NAME})
    # add_test(NAME lcd_i2c_tests COMMAND lcd_i2c_tests)
endif()

# -----------------------------------------------------------------------------
# Print Configuration Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== LCD I2C Library Configuration ===")
message(STATUS "  Allocation:     Application-managed")
message(STATUS "  Build Tests:    ${LCD_I2C_BUILD_TESTS}")
message(STATUS "=====================================")
message(STATUS "")

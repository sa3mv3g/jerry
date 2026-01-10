/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 */

cmake_minimum_required(VERSION 3.20)

# Select Vendor and Toolchain
include(config/vendor_select.cmake)

project(jerry C ASM)

# Add application
add_subdirectory(application)

# Setup testing (Optional/Placeholder)
enable_testing()

# Custom target for formatting (using uv and python script for OS agnosticism)
find_program(UV_EXECUTABLE uv)
if(UV_EXECUTABLE)
    add_custom_target(format
        COMMAND ${UV_EXECUTABLE} run python tools/format.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format via uv..."
    )

    add_custom_target(cppcheck
        COMMAND ${UV_EXECUTABLE} run python tools/run_cppcheck.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running CppCheck..."
    )

    add_custom_target(lizard
        COMMAND ${UV_EXECUTABLE} run python tools/run_lizard.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Lizard metrics..."
    )

    add_custom_target(pylint
        COMMAND ${UV_EXECUTABLE} run python tools/run_pylint.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Pylint..."
    )

    add_custom_target(ruff
        COMMAND ${UV_EXECUTABLE} run python tools/run_ruff.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Ruff..."
    )

    add_custom_target(lint
        DEPENDS cppcheck lizard pylint ruff
    )
else()
    message(WARNING "uv not found! 'format' target will not be available. Please install uv.")
endif()

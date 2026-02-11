# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.

cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Select Vendor and Toolchain
include(config/vendor_select.cmake)
include(config/lizard_config.cmake)
include(config/cppcheck_config.cmake)

project(jerry C ASM)

# Add application
add_subdirectory(application)

# Setup testing (Optional/Placeholder)
enable_testing()

# UV command configuration - can be set via command line or cache
# On systems where uv is on PATH: -DUV_COMMAND="uv"
# On systems where uv is via Python: -DUV_COMMAND="py;-m;uv"
set(UV_COMMAND "uv" CACHE STRING "Command to invoke uv (e.g., 'uv' or 'py;-m;uv')")

# Try to find uv using the configured command
if(UV_COMMAND MATCHES "^py")
    # For py -m uv, check if py exists
    find_program(PY_EXECUTABLE py)
    if(PY_EXECUTABLE)
        set(UV_FOUND TRUE)
        set(UV_RUN_CMD ${UV_COMMAND} run)
    else()
        set(UV_FOUND FALSE)
    endif()
else()
    # For direct uv command
    find_program(UV_EXECUTABLE ${UV_COMMAND})
    if(UV_EXECUTABLE)
        set(UV_FOUND TRUE)
        set(UV_RUN_CMD ${UV_EXECUTABLE} run)
    else()
        set(UV_FOUND FALSE)
    endif()
endif()

if(UV_FOUND)
    add_custom_target(format
        COMMAND ${UV_RUN_CMD} python tools/format.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format via uv..."
    )

    add_custom_target(cppcheck
        COMMAND cppcheck ${CPPCHECK_CLI_ARGS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running CppCheck..."
    )

    add_custom_target(lizard
        COMMAND ${UV_RUN_CMD} lizard ${LIZARD_CLI_PARAMS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Lizard metrics..."
    )

    add_custom_target(pylint
        COMMAND ${UV_RUN_CMD} python tools/run_pylint.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Pylint..."
    )

    add_custom_target(ruff
        COMMAND ${UV_RUN_CMD} python tools/run_ruff.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running Ruff..."
    )

    add_custom_target(lint
        DEPENDS cppcheck lizard pylint ruff
    )
else()
    message(WARNING "uv not found! 'format' and lint targets will not be available. Set UV_COMMAND to configure (e.g., -DUV_COMMAND=\"py;-m;uv\")")
endif()

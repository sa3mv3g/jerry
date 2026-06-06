# =============================================================================
# Robot Framework HIL Tests CMakeLists.txt
# =============================================================================
# Fetches Robot Framework and all Python test dependencies from GitHub using
# FetchContent, then installs them into the uv-managed virtual environment.
#
# Pattern mirrors tests/unit/CMakeLists.txt (Unity via FetchContent).
#
# Copyright (c) 2026
# All rights reserved.
# =============================================================================

cmake_minimum_required(VERSION 3.22)
project(robot_hil_tests NONE)

include(FetchContent)

# -----------------------------------------------------------------------------
# 1. Robot Framework — fetched from GitHub
# -----------------------------------------------------------------------------
FetchContent_Declare(
    robotframework
    GIT_REPOSITORY https://github.com/robotframework/robotframework.git
    GIT_TAG        v7.2.2
    SOURCE_DIR     ${CMAKE_CURRENT_BINARY_DIR}/_deps/robotframework-src
)

# -----------------------------------------------------------------------------
# 2. pymodbus — fetched from GitHub (already used by integration tests)
# -----------------------------------------------------------------------------
FetchContent_Declare(
    pymodbus
    GIT_REPOSITORY https://github.com/pymodbus-dev/pymodbus.git
    GIT_TAG        v3.9.2
    SOURCE_DIR     ${CMAKE_CURRENT_BINARY_DIR}/_deps/pymodbus-src
)

# -----------------------------------------------------------------------------
# 3. dwf Python bindings — fetched from GitHub
#    ctypes wrapper around the native Digilent WaveForms libdwf.so / dwf.dll
#    NOTE: The native libdwf.so must be installed separately from the
#    Digilent WaveForms SDK installer (system-level prerequisite).
# -----------------------------------------------------------------------------
FetchContent_Declare(
    dwf_python
    GIT_REPOSITORY https://github.com/amuramatsu/dwf.git
    GIT_TAG        master
    SOURCE_DIR     ${CMAKE_CURRENT_BINARY_DIR}/_deps/dwf-src
)

# -----------------------------------------------------------------------------
# 4. pyserial — fetched from GitHub (needed for Modbus RTU future work)
# -----------------------------------------------------------------------------
FetchContent_Declare(
    pyserial
    GIT_REPOSITORY https://github.com/pyserial/pyserial.git
    GIT_TAG        v3.5
    SOURCE_DIR     ${CMAKE_CURRENT_BINARY_DIR}/_deps/pyserial-src
)

# Fetch all declared content (downloads happen here at configure time)
FetchContent_MakeAvailable(robotframework pymodbus dwf_python pyserial)

# -----------------------------------------------------------------------------
# Locate the Python interpreter inside the uv-managed virtual environment.
# uv creates the venv at <project_root>/.venv by default.
# -----------------------------------------------------------------------------
if(WIN32)
    set(_UV_VENV_PYTHON_DEFAULT "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
else()
    set(_UV_VENV_PYTHON_DEFAULT "${CMAKE_SOURCE_DIR}/.venv/bin/python")
endif()

set(UV_VENV_PYTHON "${_UV_VENV_PYTHON_DEFAULT}"
    CACHE FILEPATH "Path to Python interpreter inside uv venv")

# -----------------------------------------------------------------------------
# robot_fetch_deps — installs all fetched packages into the uv venv.
# Uses pip install --no-build-isolation -e <src> so the fetched source is used
# directly (editable install), consistent with the FetchContent philosophy.
# -----------------------------------------------------------------------------
add_custom_target(robot_fetch_deps
    COMMAND ${UV_VENV_PYTHON} -m pip install --quiet --no-build-isolation -e
        ${robotframework_SOURCE_DIR}
    COMMAND ${UV_VENV_PYTHON} -m pip install --quiet --no-build-isolation -e
        ${pymodbus_SOURCE_DIR}
    COMMAND ${UV_VENV_PYTHON} -m pip install --quiet --no-build-isolation -e
        ${dwf_python_SOURCE_DIR}
    COMMAND ${UV_VENV_PYTHON} -m pip install --quiet --no-build-isolation -e
        ${pyserial_SOURCE_DIR}
    COMMENT "Installing Robot Framework HIL test dependencies from fetched sources"
    VERBATIM
)

# -----------------------------------------------------------------------------
# robot_tests — run all Robot Framework HIL test suites
# -----------------------------------------------------------------------------
add_custom_target(robot_tests
    DEPENDS robot_fetch_deps
    COMMAND ${UV_VENV_PYTHON} -m robot
        --outputdir ${CMAKE_BINARY_DIR}/robot_results
        --report  report.html
        --log     log.html
        ${CMAKE_CURRENT_SOURCE_DIR}/suites/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running Robot Framework HIL tests"
    VERBATIM
)

# -----------------------------------------------------------------------------
# robot_tests_negative_only — run only tests tagged 'negative'
# -----------------------------------------------------------------------------
add_custom_target(robot_tests_negative_only
    DEPENDS robot_fetch_deps
    COMMAND ${UV_VENV_PYTHON} -m robot
        --include  negative
        --outputdir ${CMAKE_BINARY_DIR}/robot_results_negative
        --report  report_negative.html
        --log     log_negative.html
        ${CMAKE_CURRENT_SOURCE_DIR}/suites/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running Robot Framework negative HIL tests only"
    VERBATIM
)

# -----------------------------------------------------------------------------
# robot_tests_positive_only — run only tests NOT tagged 'negative' (CI fast path)
# -----------------------------------------------------------------------------
add_custom_target(robot_tests_positive_only
    DEPENDS robot_fetch_deps
    COMMAND ${UV_VENV_PYTHON} -m robot
        --exclude  negative
        --outputdir ${CMAKE_BINARY_DIR}/robot_results_positive
        --report  report_positive.html
        --log     log_positive.html
        ${CMAKE_CURRENT_SOURCE_DIR}/suites/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running Robot Framework positive HIL tests only"
    VERBATIM
)

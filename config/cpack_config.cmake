# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.
#
# CPack Configuration for Jerry Firmware
# This file configures the packaging of Jerry firmware binaries for distribution.
#
# Usage:
#   cmake -S . -B build -G Ninja
#   cmake --build build --target jerry_app
#   cd build && cpack
#
# This will generate packages in the build directory.

# =============================================================================
# Git Repository Status Check
# =============================================================================
# Find git executable
find_program(GIT_EXECUTABLE git)

if(GIT_EXECUTABLE)
    # Check if repository is dirty (has uncommitted changes)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_STATUS_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # Get the short commit hash
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    # Check if we're in a git repository
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --is-inside-work-tree
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_IS_REPO
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_REPO_RESULT
    )

    if(GIT_REPO_RESULT EQUAL 0)
        set(GIT_REPO_FOUND TRUE)
        if(GIT_STATUS_OUTPUT)
            set(GIT_REPO_DIRTY TRUE)
            message(WARNING "Git repository has uncommitted changes. CPack packaging will fail.")
            message(WARNING "Uncommitted files:\n${GIT_STATUS_OUTPUT}")
        else()
            set(GIT_REPO_DIRTY FALSE)
        endif()
    else()
        set(GIT_REPO_FOUND FALSE)
        set(GIT_COMMIT_HASH "nogit")
        message(WARNING "Not a git repository. Using 'nogit' as commit hash.")
    endif()
else()
    set(GIT_REPO_FOUND FALSE)
    set(GIT_COMMIT_HASH "nogit")
    message(WARNING "Git not found. Using 'nogit' as commit hash.")
endif()

# =============================================================================
# Package Metadata
# =============================================================================
set(CPACK_PACKAGE_NAME "jerry-firmware")
set(CPACK_PACKAGE_VENDOR "Advance Instrumentation 'n' Control Systems")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Jerry Data Acquisition Firmware for STM32H563")
set(CPACK_PACKAGE_DESCRIPTION "Jerry is a data acquisition firmware designed for STM32H5xxx series microcontrollers. It features FreeRTOS with static allocation, Modbus TCP/IP and RTU support, secure boot with TrustZone, and comprehensive I/O capabilities.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/aics/jerry")
set(CPACK_PACKAGE_CONTACT "support@aics.com")

# Version information - can be overridden via command line
if(NOT DEFINED CPACK_PACKAGE_VERSION_MAJOR)
    set(CPACK_PACKAGE_VERSION_MAJOR "1")
endif()
if(NOT DEFINED CPACK_PACKAGE_VERSION_MINOR)
    set(CPACK_PACKAGE_VERSION_MINOR "0")
endif()
if(NOT DEFINED CPACK_PACKAGE_VERSION_PATCH)
    set(CPACK_PACKAGE_VERSION_PATCH "0")
endif()
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

# Package file name format: jerry-firmware-1.0.0-stm32h563-<commit_hash>
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-stm32h563-${GIT_COMMIT_HASH}")

# =============================================================================
# Resource Files
# =============================================================================
# License file (optional - only set if exists)
if(EXISTS "${CMAKE_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
endif()

# README file
if(EXISTS "${CMAKE_SOURCE_DIR}/README.md")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
endif()

# =============================================================================
# Generator Selection
# =============================================================================
# Default generators based on platform
# Note: CPACK_GENERATOR must be set BEFORE including CPack module
if(WIN32)
    # Use ZIP on Windows (NSIS requires separate installation)
    set(CPACK_GENERATOR "ZIP" CACHE STRING "CPack generator to use")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ" CACHE STRING "CPack generator to use")
else()
    set(CPACK_GENERATOR "TGZ" CACHE STRING "CPack generator to use")
endif()

# =============================================================================
# Archive Generator Configuration (ZIP/TGZ)
# =============================================================================
# Disable component-based archive installation for single package
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)

# =============================================================================
# Debian Package Configuration
# =============================================================================
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "AICS <support@aics.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "embedded")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "all")
# No dependencies for firmware binaries
set(CPACK_DEBIAN_PACKAGE_DEPENDS "")

# =============================================================================
# Installation Rules (Component-based to exclude library development files)
# =============================================================================
# Use component-based installation to only include Firmware component
# This excludes the "Development" and "Unspecified" components from modbus library
set(CPACK_COMPONENTS_ALL Firmware)
set(CPACK_COMPONENT_UNSPECIFIED_HIDDEN TRUE)
set(CPACK_COMPONENT_UNSPECIFIED_DISABLED TRUE)
set(CPACK_COMPONENT_DEVELOPMENT_HIDDEN TRUE)
set(CPACK_COMPONENT_DEVELOPMENT_DISABLED TRUE)

# Install firmware binaries to root of package
install(FILES
    "${CMAKE_BINARY_DIR}/application/jerry_app.elf"
    DESTINATION .
    COMPONENT Firmware
    OPTIONAL
)

install(FILES
    "${CMAKE_BINARY_DIR}/application/bsp/stm/stm32h563/jerry_secure_app.elf"
    DESTINATION .
    COMPONENT Firmware
    OPTIONAL
)

# Install flashing script to root of package
install(FILES
    "${CMAKE_SOURCE_DIR}/tools/flash_nucleo.py"
    DESTINATION .
    COMPONENT Firmware
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Install register map documentation to root of package
install(FILES
    "${CMAKE_BINARY_DIR}/jerry_device_register_map.txt"
    DESTINATION .
    COMPONENT Firmware
    OPTIONAL
)

# Install public-facing README
install(FILES
    "${CMAKE_SOURCE_DIR}/docs/FIRMWARE_README.md"
    DESTINATION .
    COMPONENT Firmware
    RENAME README.md
)

# =============================================================================
# Pre-Package Git Status Check Script
# =============================================================================
# Create a script that checks git status at package time (not configure time)
# This ensures the check happens when cpack is actually run
set(GIT_CHECK_SCRIPT "${CMAKE_BINARY_DIR}/check_git_status.cmake")
file(WRITE ${GIT_CHECK_SCRIPT} "
# Check git status at package time
find_program(GIT_EXECUTABLE git)
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND \${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY \"${CMAKE_SOURCE_DIR}\"
        OUTPUT_VARIABLE GIT_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_STATUS)
        message(FATAL_ERROR \"
================================================================================
ERROR: Cannot create package - Git repository has uncommitted changes!
================================================================================
Please commit or stash all changes before packaging.

Uncommitted files:
\${GIT_STATUS}
================================================================================
\")
    endif()
else()
    message(FATAL_ERROR \"Git not found. Cannot verify repository status.\")
endif()
")

# Set the pre-install script to check git status
set(CPACK_PRE_BUILD_SCRIPTS ${GIT_CHECK_SCRIPT})

# =============================================================================
# Source Package Configuration (optional)
# =============================================================================
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-src")
set(CPACK_SOURCE_IGNORE_FILES
    "/build/"
    "/\\\\.git/"
    "/\\\\.vscode/"
    "\\\\.gitignore"
    "\\\\.gitattributes"
    "/dependencies/.*/(FreeRTOS|CMSIS|lwip)/"
    "__pycache__"
    "\\\\.pyc$"
    "\\\\.elf$"
    "\\\\.map$"
    "\\\\.o$"
    "\\\\.a$"
)

# Include CPack module
include(CPack)

# =============================================================================
# Custom Package Target with Git Check
# =============================================================================
# Create a custom 'package' target that first checks git status
if(GIT_EXECUTABLE)
    add_custom_target(package_firmware
        COMMAND ${CMAKE_COMMAND} -P ${GIT_CHECK_SCRIPT}
        COMMAND ${CMAKE_CPACK_COMMAND} -G ZIP
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Creating firmware package (with git status check)..."
        VERBATIM
    )
endif()

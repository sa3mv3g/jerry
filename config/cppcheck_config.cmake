# ==============================================================================
# CPPCHECK CONFIGURATION
# ==============================================================================
# This file configures cppcheck static analysis for the Jerry project.
# It defines suppressions, include paths, and output settings.
# ==============================================================================

# ------------------------------------------------------------------------------
# Output Configuration
# ------------------------------------------------------------------------------
set(CPPCHECK_REPORT_FILENAME_NOEXT "cppcheck_report")

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------
option(CPPCHECK_USE_ADDONS "Enable cppcheck MISRA and naming addons" OFF)

# ------------------------------------------------------------------------------
# Base Arguments
# ------------------------------------------------------------------------------
set(CPPCHECK_BASE_ARGS
    --enable=all
    --std=c11
    --language=c
    --inline-suppr
)

# ------------------------------------------------------------------------------
# Include Paths
# ------------------------------------------------------------------------------
set(CPPCHECK_INCLUDES
    --include=${CMAKE_SOURCE_DIR}/application/inc/FreeRTOSConfig.h
)

# ------------------------------------------------------------------------------
# Project Configuration
# ------------------------------------------------------------------------------
set(CPPCHECK_PROJECT
    --project=${CMAKE_BINARY_DIR}/compile_commands.json
)

# ------------------------------------------------------------------------------
# Excluded Directories
# Directories completely excluded from analysis
# ------------------------------------------------------------------------------
set(CPPCHECK_EXCLUDED_DIRS
    # Test directory
    -i "${CMAKE_SOURCE_DIR}/test/"
    # Third-party LwIP stack (use upstream fixes)
    -i "${CMAKE_SOURCE_DIR}/application/dependencies/lwip/"
    # BSP/HAL code (vendor provided)
    -i "${CMAKE_SOURCE_DIR}/application/bsp/"
)

# ------------------------------------------------------------------------------
# Global Suppressions
# Warnings suppressed across all files
# ------------------------------------------------------------------------------
set(CPPCHECK_GLOBAL_SUPPRESSIONS
    # System headers not available during analysis
    --suppress=missingIncludeSystem
    # MISRA 19.2: Union usage required for protocol type-punning
    --suppress=misra-c2012-19.2
)

# ------------------------------------------------------------------------------
# Path-Specific Suppressions
# Warnings suppressed for specific file patterns
# ------------------------------------------------------------------------------
set(CPPCHECK_PATH_SUPPRESSIONS
    # Third-party dependencies (suppress all warnings)
    --suppress=*:*/application/dependencies/lwip/*
    --suppress=*:*/application/bsp/*
)

# ------------------------------------------------------------------------------
# Unused Function Suppressions
# Library/API functions intentionally unused in this project
# These are public APIs meant for external use
# ------------------------------------------------------------------------------
set(CPPCHECK_UNUSED_FUNCTION_SUPPRESSIONS
    # PHY driver API
    --suppress=unusedFunction:**/lan8742.c
    # LwIP OS abstraction layer
    --suppress=unusedFunction:**/sys_arch.c
    # Modbus library public API
    --suppress=unusedFunction:**/modbus_*.c
)

# ------------------------------------------------------------------------------
# Unused Macro Suppressions
# Configuration macros used by external libraries or for future use
# ------------------------------------------------------------------------------
set(CPPCHECK_UNUSED_MACRO_SUPPRESSIONS
    # FreeRTOS configuration options
    --suppress=misra-c2012-2.5:**/FreeRTOSConfig.h
    # LwIP configuration options
    --suppress=misra-c2012-2.5:**/lwipopts.h
    # Modbus library configuration
    --suppress=misra-c2012-2.5:**/modbus_config.h
)

# ------------------------------------------------------------------------------
# Combine All CLI Arguments
# ------------------------------------------------------------------------------
set(CPPCHECK_CLI_ARGS
    ${CPPCHECK_BASE_ARGS}
    ${CPPCHECK_INCLUDES}
    ${CPPCHECK_PROJECT}
    ${CPPCHECK_EXCLUDED_DIRS}
    ${CPPCHECK_GLOBAL_SUPPRESSIONS}
    ${CPPCHECK_PATH_SUPPRESSIONS}
    ${CPPCHECK_UNUSED_FUNCTION_SUPPRESSIONS}
    ${CPPCHECK_UNUSED_MACRO_SUPPRESSIONS}
)

# ------------------------------------------------------------------------------
# MISRA Addon Configuration (Optional)
# ------------------------------------------------------------------------------
if(CPPCHECK_USE_ADDONS)
    list(APPEND CPPCHECK_CLI_ARGS
        --addon=${CMAKE_SOURCE_DIR}/config/misra.json
    )
    message(STATUS "Cppcheck: MISRA addon enabled")
endif()

# ------------------------------------------------------------------------------
# Output Format Configuration
# Release: XML format for CI/CD integration
# Debug: Text format for developer readability
# ------------------------------------------------------------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND CPPCHECK_CLI_ARGS
        --xml
        --output-file=${CMAKE_BINARY_DIR}/${CPPCHECK_REPORT_FILENAME_NOEXT}.xml
    )
    message(STATUS "Cppcheck: XML output enabled (Release build)")
else()
    list(APPEND CPPCHECK_CLI_ARGS
        --output-file=${CMAKE_BINARY_DIR}/${CPPCHECK_REPORT_FILENAME_NOEXT}.txt
    )
    message(STATUS "Cppcheck: Text output enabled (Debug build)")
endif()

# ------------------------------------------------------------------------------
# Debug: Print final configuration
# ------------------------------------------------------------------------------
if(CMAKE_VERBOSE_MAKEFILE)
    message(STATUS "Cppcheck CLI arguments:")
    foreach(arg ${CPPCHECK_CLI_ARGS})
        message(STATUS "  ${arg}")
    endforeach()
endif()

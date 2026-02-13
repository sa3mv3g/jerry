# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.
#
# Modbus Code Generation CMake Configuration
#
# This module provides functions to generate Modbus register C code from JSON configuration.
#
# Note: Callback implementations are maintained manually in the application source
# directory (application/src/jerry_device_callbacks.c) to allow custom hardware logic.
#

# Find Python interpreter
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Set paths
set(MODBUS_CODEGEN_SCRIPT "${CMAKE_SOURCE_DIR}/tools/modbus_codegen/modbus_codegen.py")
set(MODBUS_CODEGEN_TEMPLATES_DIR "${CMAKE_SOURCE_DIR}/tools/modbus_codegen/templates")
set(MODBUS_CODEGEN_SCHEMA "${CMAKE_SOURCE_DIR}/tools/modbus_codegen/schema/modbus_registers.schema.json")

#[=============================================================================[
  modbus_generate_code

  Generate Modbus register C code from JSON configuration file.

  Usage:
    modbus_generate_code(
      CONFIG_FILE <path-to-json>
      OUTPUT_DIR <output-directory>
      [GENERATED_FILES <output-variable>]
    )

  Arguments:
    CONFIG_FILE     - Path to the JSON configuration file
    OUTPUT_DIR      - Directory where generated files will be placed
    GENERATED_FILES - Optional variable to receive list of generated files

  Example:
    modbus_generate_code(
      CONFIG_FILE "${CMAKE_SOURCE_DIR}/config/jerry_registers.json"
      OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/generated"
      GENERATED_FILES MODBUS_GENERATED_SOURCES
    )
#]=============================================================================]
function(modbus_generate_code)
    # Parse arguments
    set(options "")
    set(oneValueArgs CONFIG_FILE OUTPUT_DIR GENERATED_FILES)
    set(multiValueArgs "")
    cmake_parse_arguments(MODBUS_GEN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Validate required arguments
    if(NOT MODBUS_GEN_CONFIG_FILE)
        message(FATAL_ERROR "modbus_generate_code: CONFIG_FILE is required")
    endif()

    if(NOT MODBUS_GEN_OUTPUT_DIR)
        message(FATAL_ERROR "modbus_generate_code: OUTPUT_DIR is required")
    endif()

    # Check if config file exists
    if(NOT EXISTS "${MODBUS_GEN_CONFIG_FILE}")
        message(FATAL_ERROR "modbus_generate_code: Config file not found: ${MODBUS_GEN_CONFIG_FILE}")
    endif()

    # Check if codegen script exists
    if(NOT EXISTS "${MODBUS_CODEGEN_SCRIPT}")
        message(FATAL_ERROR "modbus_generate_code: Codegen script not found: ${MODBUS_CODEGEN_SCRIPT}")
    endif()

    # Get device name from config file to determine output filenames
    file(READ "${MODBUS_GEN_CONFIG_FILE}" CONFIG_CONTENT)
    string(JSON DEVICE_NAME GET "${CONFIG_CONTENT}" "device" "name")
    string(TOLOWER "${DEVICE_NAME}" DEVICE_NAME_LOWER)

    # Define expected output files
    set(GENERATED_HEADER "${MODBUS_GEN_OUTPUT_DIR}/${DEVICE_NAME_LOWER}_registers.h")
    set(GENERATED_SOURCE "${MODBUS_GEN_OUTPUT_DIR}/${DEVICE_NAME_LOWER}_registers.c")

    set(ALL_GENERATED_FILES
        ${GENERATED_HEADER}
        ${GENERATED_SOURCE}
    )

    # Get template files for dependency tracking
    file(GLOB TEMPLATE_FILES "${MODBUS_CODEGEN_TEMPLATES_DIR}/*.j2")

    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "  Modbus Code Generation Configuration")
    message(STATUS "========================================")
    message(STATUS "  Config file:  ${MODBUS_GEN_CONFIG_FILE}")
    message(STATUS "  Output dir:   ${MODBUS_GEN_OUTPUT_DIR}")
    message(STATUS "  Device name:  ${DEVICE_NAME}")
    message(STATUS "  Python:       ${Python3_EXECUTABLE}")
    message(STATUS "")
    message(STATUS "  Generated files:")
    message(STATUS "    - ${DEVICE_NAME_LOWER}_registers.h")
    message(STATUS "    - ${DEVICE_NAME_LOWER}_registers.c")
    message(STATUS "========================================")
    message(STATUS "")

    # Create output directory if it doesn't exist
    file(MAKE_DIRECTORY "${MODBUS_GEN_OUTPUT_DIR}")

    # Add custom command to generate the files
    add_custom_command(
        OUTPUT ${ALL_GENERATED_FILES}
        COMMAND ${Python3_EXECUTABLE} "${MODBUS_CODEGEN_SCRIPT}"
                "${MODBUS_GEN_CONFIG_FILE}"
                --output-dir "${MODBUS_GEN_OUTPUT_DIR}"
        DEPENDS
            ${MODBUS_GEN_CONFIG_FILE}
            ${MODBUS_CODEGEN_SCRIPT}
            ${MODBUS_CODEGEN_SCHEMA}
            ${TEMPLATE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating Modbus register code from ${MODBUS_GEN_CONFIG_FILE}"
        VERBATIM
    )

    # Create a custom target for the generation
    add_custom_target(modbus_codegen
        DEPENDS ${ALL_GENERATED_FILES}
        COMMENT "Modbus code generation target"
    )

    # Return generated files list if requested
    if(MODBUS_GEN_GENERATED_FILES)
        set(${MODBUS_GEN_GENERATED_FILES} ${ALL_GENERATED_FILES} PARENT_SCOPE)
    endif()

    message(STATUS "Modbus code generation configured successfully")

endfunction()

#[=============================================================================[
  modbus_add_generated_sources

  Helper function to add generated Modbus sources to a target.

  Usage:
    modbus_add_generated_sources(
      TARGET <target-name>
      GENERATED_FILES <list-of-files>
    )
#]=============================================================================]
function(modbus_add_generated_sources)
    set(options "")
    set(oneValueArgs TARGET)
    set(multiValueArgs GENERATED_FILES)
    cmake_parse_arguments(MODBUS_ADD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT MODBUS_ADD_TARGET)
        message(FATAL_ERROR "modbus_add_generated_sources: TARGET is required")
    endif()

    if(NOT MODBUS_ADD_GENERATED_FILES)
        message(FATAL_ERROR "modbus_add_generated_sources: GENERATED_FILES is required")
    endif()

    # Add dependency on codegen target
    add_dependencies(${MODBUS_ADD_TARGET} modbus_codegen)

    # Add generated sources to target
    target_sources(${MODBUS_ADD_TARGET} PRIVATE ${MODBUS_ADD_GENERATED_FILES})

    message(STATUS "Added Modbus generated sources to target: ${MODBUS_ADD_TARGET}")

endfunction()

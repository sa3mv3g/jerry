SET(CPPCHECK_REPORT_FILENAME_NOEXT cppcheck_report)

# 1. Find the cppcheck executable in the PATH
find_program(CPPCHECK_EXE NAMES cppcheck)

if(CPPCHECK_EXE)
    get_filename_component(CPPCHECK_BIN_DIR "${CPPCHECK_EXE}" DIRECTORY)
    get_filename_component(CPPCHECK_INSTALL_ROOT "${CPPCHECK_BIN_DIR}" DIRECTORY)
    set(CPPCHECK_ADDONS_DIR "${CPPCHECK_INSTALL_ROOT}/cppcheck/addons" CACHE PATH "Path to Cppcheck addons")
    message(STATUS "Cppcheck: Found executable at ${CPPCHECK_EXE}")
    message(STATUS "Cppcheck: Derived addon path: ${CPPCHECK_ADDONS_DIR}")
else()
    message(FATAL_ERROR "Cppcheck executable not found in PATH!")
endif()

# Option to enable MISRA and naming addons (requires cppcheck addons to be installed)
option(CPPCHECK_USE_ADDONS "Enable cppcheck MISRA and naming addons" OFF)

# Find Python executable for MISRA addon
# On Windows, 'py' launcher is preferred; on Unix, 'python3' or 'python'
if(WIN32)
    find_program(PYTHON_EXECUTABLE NAMES py python3 python)
else()
    find_program(PYTHON_EXECUTABLE NAMES python3 python)
endif()

if(PYTHON_EXECUTABLE)
    message(STATUS "Cppcheck: Found Python at ${PYTHON_EXECUTABLE}")
else()
    message(WARNING "Cppcheck: Python not found - MISRA addon will not work")
    set(PYTHON_EXECUTABLE "python" CACHE FILEPATH "Python executable for cppcheck addons")
endif()

SET(CPPCHECK_CLI_ARGS
--enable=all
--suppress=missingIncludeSystem
--std=c11
--language=c
--platform=native
--inline-suppr
# File filters - only analyze our application code and custom dependencies
--file-filter=*/application/src/*
--file-filter=*/application/inc/*
--file-filter=*/application/dependencies/modbus/*
--file-filter=*/application/dependencies/adc_filter/*
--file-filter=*/application/dependencies/lwip/port/*
--file-filter=*/application/dependencies/lwip/config/*
# Suppress all warnings from third-party dependencies (headers still get processed)
--suppress=*:*/application/dependencies/FreeRTOS-Kernel/*
--suppress=*:*/application/dependencies/CMSIS_6/*
--suppress=*:*/application/dependencies/CMSIS-DSP/*
--suppress=*:*/application/dependencies/lwip/stm32-mw-lwip/*
--suppress=*:*/application/bsp/stm/stm32h563/Drivers/*
# Include FreeRTOS config for proper macro expansion
--include=${CMAKE_SOURCE_DIR}/application/inc/FreeRTOSConfig.h
--project=${CMAKE_BINARY_DIR}/compile_commands.json
--checkers-report=${CMAKE_BINARY_DIR}/${CPPCHECK_REPORT_FILENAME_NOEXT}_checker_report.txt
--inconclusive
)
# Generate a portable misra.json in the build directory
configure_file(
    "${CMAKE_SOURCE_DIR}/config/misra.json.in"
    "${CMAKE_BINARY_DIR}/misra.json"
    @ONLY
)

# Add MISRA addon if enabled
if(CPPCHECK_USE_ADDONS)
    message(STATUS "Cppcheck: MISRA addon ENABLED using ${CMAKE_BINARY_DIR}/misra.json")
    list(APPEND CPPCHECK_CLI_ARGS
        --addon=${CMAKE_BINARY_DIR}/misra.json
    )
else()
    message(STATUS "Cppcheck: MISRA addon DISABLED")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND CPPCHECK_CLI_ARGS
        --xml
        --output-file=${CMAKE_BINARY_DIR}/${CPPCHECK_REPORT_FILENAME_NOEXT}.xml
    )
else()
    list(APPEND CPPCHECK_CLI_ARGS
        --output-file=${CMAKE_BINARY_DIR}/${CPPCHECK_REPORT_FILENAME_NOEXT}.txt
    )
endif()

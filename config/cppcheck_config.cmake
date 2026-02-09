SET(CPPCHECK_REPORT_FILENAME_NOEXT cppcheck_report)

# 1. Find the cppcheck executable in the PATH
find_program(CPPCHECK_EXE NAMES cppcheck)

if(CPPCHECK_EXE)
    # 2. Get the directory where the exe is located (usually the 'bin' folder)
    get_filename_component(CPPCHECK_BIN_DIR "${CPPCHECK_EXE}" DIRECTORY)
    
    # 3. Go up one level to the root installation folder
    get_filename_component(CPPCHECK_INSTALL_ROOT "${CPPCHECK_BIN_DIR}" DIRECTORY)
    
    # 4. Define the addon path relative to the root
    set(CPPCHECK_ADDONS_DIR "${CPPCHECK_INSTALL_ROOT}/cppcheck/addons" CACHE PATH "Path to Cppcheck addons")
    
    message(STATUS "Cppcheck: Found executable at ${CPPCHECK_EXE}")
    message(STATUS "Cppcheck: Derived addon path: ${CPPCHECK_ADDONS_DIR}")
else()
    message(FATAL_ERROR "Cppcheck executable not found in PATH!")
endif()

# Option to enable MISRA and naming addons (requires cppcheck addons to be installed)
option(CPPCHECK_USE_ADDONS "Enable cppcheck MISRA and naming addons" OFF)

SET(CPPCHECK_CLI_ARGS
--enable=all
--suppress=missingIncludeSystem
--suppress=*:*/application/dependencies/lwip/*
--suppress=*:*/application/dependencies/CMSIS_6/*
--suppress=*:*/application/dependencies/CMSIS-DSP/*
--suppress=*:*/application/bsp/*
--suppress=*:*/FreeRTOS-Kernel/*
--std=c11
--language=c
--inline-suppr
-i "${CMAKE_SOURCE_DIR}/test/"
-i "${CMAKE_SOURCE_DIR}/application/bsp/"
-i "${CMAKE_SOURCE_DIR}/application/CMSIS_6/"
-i "${CMAKE_SOURCE_DIR}/application/CMSIS-DSP/"
-i ${CMAKE_SOURCE_DIR}/application/dependencies/FreeRTOS-Kernel/
--include=${CMAKE_SOURCE_DIR}/application/inc/FreeRTOSConfig.h
--include=${CMAKE_SOURCE_DIR}/application
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
    message(STATUS "Cppcheck: MISRA addon ENABLED using ${CMAKE_SOURCE_DIR}/config/misra.json")
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

SET(CPPCHECK_REPORT_FILENAME_NOEXT cppcheck_report)

# Option to enable MISRA and naming addons (requires cppcheck addons to be installed)
option(CPPCHECK_USE_ADDONS "Enable cppcheck MISRA and naming addons" OFF)

SET(CPPCHECK_CLI_ARGS
--enable=all
--suppress=missingIncludeSystem
# Suppress warnings from third-party dependencies (lwIP) but NOT our custom modbus library
--suppress=*:*/application/dependencies/lwip/*
--suppress=*:*/application/bsp/*
--std=c11
--language=c
--inline-suppr
-i "${CMAKE_SOURCE_DIR}/test/"
# Exclude third-party dependencies (lwIP) but NOT our custom modbus library
-i "${CMAKE_SOURCE_DIR}/application/dependencies/lwip/"
-i "${CMAKE_SOURCE_DIR}/application/bsp/"
--include=${CMAKE_SOURCE_DIR}/application/inc/FreeRTOSConfig.h
--project=${CMAKE_BINARY_DIR}/compile_commands.json
)

# Add MISRA addon if enabled
if(CPPCHECK_USE_ADDONS)
    list(APPEND CPPCHECK_CLI_ARGS
        --addon=${CMAKE_SOURCE_DIR}/config/misra.json
    )
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

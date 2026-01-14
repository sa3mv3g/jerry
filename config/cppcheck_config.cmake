SET(CPPCHECK_REPORT_FILENAME_NOEXT cppcheck_report)

SET(CPPCHECK_CLI_ARGS 
--enable=all
--suppress=missingIncludeSystem
--suppress=*:*/application/dependencies/*
--suppress=*:*/application/bsp/*
--std=c11
--language=c
--inline-suppr
--addon=${CMAKE_SOURCE_DIR}/config/misra.json
--addon=naming.py
-i "${CMAKE_SOURCE_DIR}/test/"
-i "${CMAKE_SOURCE_DIR}/application/dependencies/"
-i "${CMAKE_SOURCE_DIR}/application/bsp/"
--include=${CMAKE_SOURCE_DIR}/application/inc/FreeRTOSConfig.h
--project=${CMAKE_BINARY_DIR}/compile_commands.json
)

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

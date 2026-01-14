SET(CPPCHECK_REPORT_FILENAME_NOEXT cppcheck_report)

SET(CPPCHECK_CLI_ARGS 
--enable=all
--std=c11
--language=c
--inline-suppr
--addon=misra
--addon=naming.py
-i ./test/
-i ./application/dependencies/
-i ./application/bsp/
--project=build/compile_commands.json
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND CPPCHECK_CLI_ARGS 
        --xml
        --output-file=build/${CPPCHECK_REPORT_FILENAME_NOEXT}.xml
    )
else()

    list(APPEND CPPCHECK_CLI_ARGS
        --output-file=build/${CPPCHECK_REPORT_FILENAME_NOEXT}.txt
    )
endif()

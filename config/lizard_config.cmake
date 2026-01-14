SET(LIZARD_REPORT_FILENAME_NOEXT lizrad_report)

SET(LIZARD_CLI_PARAMS 
--length 100
--CCN 15
--arguments 5
--modified
--languages cpp
--exclude "./application/bsp/*"
--exclude "./application/dependencies/*" 
--exclude "./tools/*" 
--exclude "./test/*"
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND LIZARD_CLI_PARAMS 
        --xml
        --output_file build/${LIZARD_REPORT_FILENAME_NOEXT}.xml
    )
else()

    list(APPEND LIZARD_CLI_PARAMS
        --html
        --output_file build/${LIZARD_REPORT_FILENAME_NOEXT}.html
    )
endif()

if(VENDOR STREQUAL "stm" OR "$ENV{VENDOR}" STREQUAL "stm")
    add_subdirectory(stm)
else()
    message(FATAL_ERROR "Unsupported Vendor: ${VENDOR}")
endif()

SET(CMSIS_PORT_SRC ${CMSIS_PORT_SRC} PARENT_SCOPE)
SET(CMSIS_PORT_INC ${CMSIS_PORT_INC} PARENT_SCOPE)
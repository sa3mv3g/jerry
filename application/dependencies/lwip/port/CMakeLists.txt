# only CMSIS port is needed

add_subdirectory(cmsis_lwip_port)

SET(CMSIS_PORT_SRC ${CMSIS_PORT_SRC} PARENT_SCOPE)
SET(CMSIS_PORT_INC ${CMSIS_PORT_INC} PARENT_SCOPE)
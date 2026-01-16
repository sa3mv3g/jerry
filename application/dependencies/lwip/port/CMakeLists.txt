# only CMSIS port is needed

add_subdirectory(cmsis_lwip_port)

SET(LWIP_PORT_SRC ${LWIP_PORT_SRC} PARENT_SCOPE)
SET(LWIP_PORT_INC ${LWIP_PORT_INC} PARENT_SCOPE)

# STM32H5 direct HAL port (no CMSIS driver dependency)
add_subdirectory(stm32h5)

SET(LWIP_PORT_SRC ${LWIP_PORT_SRC} PARENT_SCOPE)
SET(LWIP_PORT_INC ${LWIP_PORT_INC} PARENT_SCOPE)

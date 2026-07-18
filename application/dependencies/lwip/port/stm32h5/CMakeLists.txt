# STM32H5 LwIP Port - Direct HAL implementation

# Port sources
SET(LWIP_PORT_SRC
    ${LWIP_PORT_SRC}
    "${CMAKE_CURRENT_SOURCE_DIR}/ethernetif.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lan8742.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sys_arch.c"
    PARENT_SCOPE
)

# Port includes (include arch subdirectory for cc.h and sys_arch.h)
SET(LWIP_PORT_INC
    ${LWIP_PORT_INC}
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/arch"
    "${CMAKE_SOURCE_DIR}/application/bsp"
    "${CMAKE_SOURCE_DIR}/application/bsp/stm/stm32h563/Drivers/CMSIS/DSP/Include"
    PARENT_SCOPE
)


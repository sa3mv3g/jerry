# Chip-specific LwIP port files
if(CHIPS_CFG STREQUAL "stm32h563")
    # STM32H5 direct HAL port (no CMSIS driver dependency)
    add_subdirectory(stm32h5)
elseif(CHIPS_CFG STREQUAL "stm32f429")
    # STM32F4 port - LwIP is included directly in the stm32f429_bsp library
    # from the BSP's Middlewares/Third_Party/LwIP folder
    # No port sources needed here as they are handled by the BSP CMakeLists.txt
    return()
endif()

SET(LWIP_PORT_SRC ${LWIP_PORT_SRC} PARENT_SCOPE)
SET(LWIP_PORT_INC ${LWIP_PORT_INC} PARENT_SCOPE)

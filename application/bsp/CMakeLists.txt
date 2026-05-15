
if(VENDOR STREQUAL "stm" OR "$ENV{VENDOR}" STREQUAL "stm")
    set(SRC ${SRC}
        ${BSP_DIR}/stm/Source/bsp.c
        ${BSP_DIR}/stm/Source/Driver_GPIO.c
        ${BSP_DIR}/stm/Source/Driver_USART.c
        ${BSP_DIR}/stm/Source/Driver_I2C.c
        ${BSP_DIR}/stm/Source/Driver_SPI.c
    )

    add_subdirectory(${BSP_DIR}/stm/stm32h563)

    add_library(bsp STATIC ${SRC})

    target_include_directories(bsp PUBLIC ${BSP_DIR}/../dependencies/CMSIS_6/CMSIS/Driver/Include)

    target_link_libraries(bsp PUBLIC stm32h563_bsp)
else()
    message(FATAL_ERROR "Unsupported Vendor: ${VENDOR}")
endif()

add_library(bsp_interface INTERFACE)
target_link_libraries(bsp_interface INTERFACE bsp)
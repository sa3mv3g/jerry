if(VENDOR STREQUAL "stm" OR "$ENV{VENDOR}" STREQUAL "stm")

    set(APP_SOURCES ${APP_SOURCES}
                    ${BSP_DIR}/stm/bsp.c
                    PARENT_SCOPE
    )

    add_subdirectory(${BSP_DIR}/stm/stm32h563)

    set(BSP_LIBRARY_NAME "stm32h563_bsp")
else()
    message(FATAL_ERROR "Unsupported Vendor: ${VENDOR}")
endif()

# This allows all other libraries to just link to "bsp_interface"
# the intention is that, irrespective of underneath bsp lib,
# app will only include bsp_interface. bsp_interface is now a common
# interface.

add_library(bsp_interface INTERFACE)
target_link_libraries(bsp_interface INTERFACE ${BSP_LIBRARY_NAME})
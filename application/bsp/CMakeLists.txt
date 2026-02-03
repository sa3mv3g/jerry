if(VENDOR STREQUAL "stm32h563" OR "$ENV{VENDOR}" STREQUAL "stm32h563")
    set(APP_SOURCES ${APP_SOURCES} ${BSP_DIR}/stm32h563/bsp.c PARENT_SCOPE)
    add_subdirectory(${BSP_DIR}/stm32h563)
    set(BSP_LIBRARY_NAME "stm32h563_bsp")
else()
    message(FATAL_ERROR "Unsupported Vendor: ${VENDOR}")
endif()

# This allows all other libraries to just link to "bsp_interface"
add_library(bsp_interface INTERFACE)
target_link_libraries(bsp_interface INTERFACE ${BSP_LIBRARY_NAME})
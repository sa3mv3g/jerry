set(VENDOR "stm" CACHE STRING "Microcontroller Vendor")

if(VENDOR STREQUAL "stm")
    set(BSP_DIR "${CMAKE_SOURCE_DIR}/application/bsp/stm")
    set(CMAKE_TOOLCHAIN_FILE "${BSP_DIR}/toolchain.cmake")
else()
    message(FATAL_ERROR "Unsupported Vendor: ${VENDOR}")
endif()

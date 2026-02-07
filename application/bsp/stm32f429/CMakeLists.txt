cmake_minimum_required(VERSION 3.22)

# Define the BSP library - this will be used by stm32cubemx/CMakeLists.txt
add_library(stm32f429_bsp STATIC)

# Set CMAKE_PROJECT_NAME so stm32cubemx/CMakeLists.txt can add sources to our target
set(CMAKE_PROJECT_NAME stm32f429_bsp)

# Include the STM32CubeMX generated CMakeLists.txt
# This adds sources, includes, and definitions to stm32f429_bsp
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/stm32cubemx/CMakeLists.txt)

# ==========================================================================
# Additional configuration for FreeRTOS integration
# ==========================================================================

# Additional sources not in stm32cubemx
target_sources(stm32f429_bsp PRIVATE
    Core/Src/bsp_init.c
    # sys_arch.c from stm32h5 port (uses native FreeRTOS, not CMSIS-OS)
    ${CMAKE_SOURCE_DIR}/application/dependencies/lwip/port/stm32h5/sys_arch.c
)

# Additional include directories for FreeRTOS and project integration
target_include_directories(stm32f429_bsp PUBLIC
    # Chip-specific bsp_chip.h location
    ${CMAKE_CURRENT_SOURCE_DIR}
    # LwIP config (lwipopts.h)
    ${CMAKE_SOURCE_DIR}/application/dependencies/lwip/config
    # FreeRTOS includes
    ${CMAKE_SOURCE_DIR}/application/dependencies/FreeRTOS-Kernel/include
    ${CMAKE_SOURCE_DIR}/application/dependencies/FreeRTOS-Kernel/portable/GCC/ARM_CM4F
    ${CMAKE_SOURCE_DIR}/application/inc
)

# Additional compile definitions for FreeRTOS LwIP integration
target_compile_definitions(stm32f429_bsp PUBLIC
    NO_SYS=0
    USE_FREERTOS=1
)

target_compile_options(stm32f429_bsp PRIVATE
    -Wall -Wextra -Werror -g -gdwarf-4 -Wno-unused-parameter -Wno-old-style-declaration -Wno-pedantic
)

# Link to FreeRTOS (required for LwIP sys_arch.c)
target_link_libraries(stm32f429_bsp PUBLIC
    freertos_kernel
)

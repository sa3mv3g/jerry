# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.

# Application CMakeLists.txt

# Sources
file(GLOB_RECURSE APP_SOURCES "src/*.c" "src/*.s")
file(GLOB_RECURSE APP_HEADERS "inc/*.h")

# 3rd Party Libs would be added here
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE inc)
target_compile_definitions(freertos_config INTERFACE projCOVERAGE_TEST=0)

set(FREERTOS_PORT "GCC_ARM_CM33_NTZ_NONSECURE" CACHE STRING "FreeRTOS Port")
add_subdirectory(dependencies/FreeRTOS-Kernel)

# Add BSP
add_subdirectory(${BSP_DIR}/stm32h563)

add_executable(jerry_app
    ${APP_SOURCES}
    ${APP_HEADERS}
)

target_include_directories(jerry_app PRIVATE
    inc
    CMSIS_5
)

target_link_libraries(jerry_app PRIVATE
    stm32h563_bsp
    freertos_kernel
)

# Compiler options are inherited from toolchain but we can add more specific ones
target_compile_options(jerry_app PRIVATE
    -Wall -Wextra -Werror
)

# Linker options
target_link_options(jerry_app PRIVATE
    -Wl,-Map=${CMAKE_BINARY_DIR}/jerry_app.map
)

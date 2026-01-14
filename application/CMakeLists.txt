# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.

# Application CMakeLists.txt

# Sources
file(GLOB_RECURSE APP_SOURCES "src/*.c" "src/*.s")
file(GLOB_RECURSE APP_HEADERS "inc/*.h")
set(APP_HEADERS ${APP_HEADERS} bsp/bsp.h)

# Clone CMSIS_5 if not present
set(CMSIS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/CMSIS_5")
if(NOT EXISTS "${CMSIS_DIR}")
    message(STATUS "Cloning CMSIS_5...")
    execute_process(
        COMMAND git clone -b 5.9.0 https://github.com/ARM-software/CMSIS_5.git ${CMSIS_DIR}
        RESULT_VARIABLE git_result
    )
    if(NOT git_result EQUAL 0)
        message(FATAL_ERROR "Failed to clone CMSIS_5")
    endif()
endif()

include(cmsis.cmake)

# 3rd Party Libs would be added here
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE inc)
target_compile_definitions(freertos_config INTERFACE projCOVERAGE_TEST=0)

set(FREERTOS_PORT "GCC_ARM_CM33_NTZ_NONSECURE" CACHE STRING "FreeRTOS Port")
add_subdirectory(dependencies/FreeRTOS-Kernel)

# Add BSP
add_subdirectory(${BSP_DIR})

add_executable(jerry_app
    ${APP_SOURCES}
    ${APP_HEADERS}
)

target_include_directories(jerry_app PRIVATE
    inc
    bsp
)

target_link_libraries(jerry_app 
    PRIVATE 
        "-Wl,--whole-archive"
        stm32h563_bsp
        "-Wl,--no-whole-archive"
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

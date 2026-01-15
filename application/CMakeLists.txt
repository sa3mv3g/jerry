# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.

# Application CMakeLists.txt
include(FetchContent)

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

# Define the base path for your dependencies
set(LWIP_DEPS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lwip")

# 1. Download LwIP Core into application/dependencies/lwip/src
FetchContent_Declare(
    lwip_core
    GIT_REPOSITORY https://git.savannah.nongnu.org/git/lwip.git
    GIT_TAG        STABLE-2_1_3_RELEASE
    SOURCE_DIR     "${LWIP_DEPS_PATH}/lwip-core"
    SOURCE_SUBDIR  "EXTRACT_ONLY"
)

# 2. Download LwIP Contrib into application/dependencies/lwip/contrib
FetchContent_Declare(
    lwip_contrib
    GIT_REPOSITORY https://git.savannah.nongnu.org/git/lwip/lwip-contrib.git
    GIT_TAG        STABLE-2_1_0_RELEASE
    SOURCE_DIR     "${LWIP_DEPS_PATH}/lwip-contrib"
    SOURCE_SUBDIR  "EXTRACT_ONLY"
)

# Populate the content
FetchContent_MakeAvailable(lwip_core lwip_contrib)

include(cmsis.cmake)

# 3rd Party Libs would be added here
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE inc)
target_compile_definitions(freertos_config INTERFACE projCOVERAGE_TEST=0)

set(FREERTOS_PORT "GCC_ARM_CM33_NTZ_NONSECURE" CACHE STRING "FreeRTOS Port")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/FreeRTOS-Kernel)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lwip)
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

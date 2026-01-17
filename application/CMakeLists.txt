# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.

# Application CMakeLists.txt
include(FetchContent)

# Sources
file(GLOB_RECURSE APP_SOURCES "src/*.c" "src/*.s")
file(GLOB_RECURSE APP_HEADERS "inc/*.h")
set(APP_HEADERS ${APP_HEADERS} bsp/bsp.h)

# Define the base path for your dependencies
set(DEPS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/")
set(LWIP_DEPS_PATH "${DEPS_PATH}/lwip")

# All cmake FetchContent_Declare's target should be of  
# /FCD_.*/ to make name unique.

# 1. Download LwIP Core into application/dependencies/lwip/src
FetchContent_Declare(
    FCD_lwip_core
    GIT_REPOSITORY https://git.savannah.nongnu.org/git/lwip.git
    GIT_TAG        STABLE-2_1_3_RELEASE
    SOURCE_DIR     "${LWIP_DEPS_PATH}/lwip-core"
    SOURCE_SUBDIR  "EXTRACT_ONLY"
)

# 2. Download LwIP Contrib into application/dependencies/lwip/contrib
FetchContent_Declare(
    FCD_lwip_contrib
    GIT_REPOSITORY https://git.savannah.nongnu.org/git/lwip/lwip-contrib.git
    GIT_TAG        STABLE-2_1_0_RELEASE
    SOURCE_DIR     "${LWIP_DEPS_PATH}/lwip-contrib"
    SOURCE_SUBDIR  "EXTRACT_ONLY"
)

# 3. Download CMSIS_5
FetchContent_Declare(
    FCD_CMSIS_5
    GIT_REPOSITORY https://github.com/ARM-software/CMSIS_5.git
    GIT_TAG        5.9.0
    SOURCE_DIR     "${DEPS_PATH}/cmsis/CMSIS_5"
    SOURCE_SUBDIR  "EXTRACT_ONLY"
)

# 4. Download FreeRTOS-Kernel
FetchContent_Declare(
    FCD_freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG        V11.2.0
    SOURCE_DIR     "${DEPS_PATH}/FreeRTOS-Kernel"
    SOURCE_SUBDIR  "EXTRACT_ONLY"
)

# Populate the content
FetchContent_MakeAvailable(FCD_lwip_core FCD_CMSIS_5 FCD_freertos_kernel)

# 3rd Party Libs would be added here
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE inc)
target_compile_definitions(freertos_config INTERFACE projCOVERAGE_TEST=0)

set(FREERTOS_PORT "GCC_ARM_CM33_NTZ_NONSECURE" CACHE STRING "FreeRTOS Port")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/FreeRTOS-Kernel)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/cmsis)
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
        "-Wl,--no-whole-archive"
        cmsis_ported
        "-Wl,--no-whole-archive"
        lwip_stack
)

# Ensure jerry_secure_app is built first so import lib exists
add_dependencies(jerry_app jerry_secure_app)

# Compiler options are inherited from toolchain but we can add more specific ones
target_compile_options(jerry_app PRIVATE
    -Wall -Wextra -Werror -g -gdwarf-4
)

# Linker options
target_link_options(jerry_app PRIVATE
    -T "${CMAKE_SOURCE_DIR}/application/bsp/stm/stm32h563/NonSecure/STM32H563xx_FLASH_ns.ld"
    -Wl,-Map=${CMAKE_BINARY_DIR}/jerry_app.map
    "${CMAKE_BINARY_DIR}/libsecure_nsclib.a"
)

# Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
# All rights reserved.

# Application CMakeLists.txt
include(FetchContent)

# Include Modbus code generation configuration
include(${CMAKE_SOURCE_DIR}/config/modbus_codegen.cmake)

# Sources
file(GLOB_RECURSE APP_SOURCES "src/*.c" "src/*.s")
file(GLOB_RECURSE APP_HEADERS "inc/*.h")
set(APP_HEADERS ${APP_HEADERS} bsp/bsp.h)

# Define the base path for your dependencies
set(DEPS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/")
set(LWIP_DEPS_PATH "${DEPS_PATH}/lwip")

# All cmake FetchContent_Declare's target should be of
# /FCD_.*/ to make name unique.

# ==========================================================================
# FetchContent Progress Messages
# ==========================================================================
message(STATUS "")
message(STATUS "=== Fetching External Dependencies ===")

# 1. Download Official STM32 LwIP Middleware
message(STATUS "[1/2] Declaring STM32 LwIP Middleware (stm32-mw-lwip v2.1.3_20241213)...")
FetchContent_Declare(
    FCD_stm32_mw_lwip
    GIT_REPOSITORY https://github.com/STMicroelectronics/stm32-mw-lwip.git
    GIT_TAG        v2.1.3_20241213
    SOURCE_DIR     "${LWIP_DEPS_PATH}/stm32-mw-lwip"
    GIT_PROGRESS   TRUE
)

# 2. Download FreeRTOS-Kernel
message(STATUS "[2/2] Declaring FreeRTOS-Kernel (V11.2.0)...")
FetchContent_Declare(
    FCD_freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG        V11.2.0
    SOURCE_DIR     "${DEPS_PATH}/FreeRTOS-Kernel"
    SOURCE_SUBDIR  "EXTRACT_ONLY"
    GIT_PROGRESS   TRUE
)

# Populate the content at configure time
message(STATUS "")
message(STATUS "Fetching dependencies (this may take a while on first run)...")
message(STATUS "  - STM32 LwIP Middleware: ${LWIP_DEPS_PATH}/stm32-mw-lwip")
message(STATUS "  - FreeRTOS-Kernel: ${DEPS_PATH}/FreeRTOS-Kernel")
message(STATUS "")
FetchContent_MakeAvailable(FCD_stm32_mw_lwip FCD_freertos_kernel)
message(STATUS "")
message(STATUS "=== Dependencies fetched successfully ===")
message(STATUS "")

# ==========================================================================
# Configure Libraries
# ==========================================================================
message(STATUS "=== Configuring Libraries ===")

# 3rd Party Libs would be added here
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE inc)
target_compile_definitions(freertos_config INTERFACE projCOVERAGE_TEST=0)

set(FREERTOS_PORT "GCC_ARM_CM33_NTZ_NONSECURE" CACHE STRING "FreeRTOS Port")

message(STATUS "[1/4] Adding FreeRTOS-Kernel...")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/FreeRTOS-Kernel)

message(STATUS "[2/4] Adding LwIP stack...")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lwip)

message(STATUS "[3/4] Adding Modbus stack...")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/modbus)

message(STATUS "[4/4] Adding BSP (${BSP_DIR})...")
add_subdirectory(${BSP_DIR})

message(STATUS "=== Libraries configured ===")
message(STATUS "")

# ==========================================================================
# Modbus Code Generation
# ==========================================================================
# Generate Modbus register code from JSON configuration
modbus_generate_code(
    CONFIG_FILE "${CMAKE_SOURCE_DIR}/config/jerry_registers.json"
    OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/generated"
    GENERATED_FILES MODBUS_GENERATED_SOURCES
)

add_executable(jerry_app
    ${APP_SOURCES}
    ${APP_HEADERS}
)

target_include_directories(jerry_app PRIVATE
    inc
    bsp
    src/generated
)

# Add Modbus generated sources to the application
modbus_add_generated_sources(
    TARGET jerry_app
    GENERATED_FILES ${MODBUS_GENERATED_SOURCES}
)

target_link_libraries(jerry_app
    PRIVATE
        "-Wl,--whole-archive"
        lwip_stack
        stm32h563_bsp
        modbus_stack
        "-Wl,--no-whole-archive"
        freertos_kernel
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

add_custom_command(TARGET jerry_app POST_BUILD
    COMMAND ${CMAKE_OBJDUMP} -d $<TARGET_FILE:jerry_app> > $<TARGET_FILE_DIR:jerry_app>/jerry_app.asm
    COMMENT "Generating disassembly for jerry_app"
)

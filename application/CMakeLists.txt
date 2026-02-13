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
# CMSIS-DSP Configuration (must be set BEFORE FetchContent_MakeAvailable)
# ==========================================================================
# Target processor - STM32H5 uses Cortex-M33
set(ARM_CPU "cortex-m33" CACHE STRING "ARM CPU type")

# Set CMSIS Core path (will be set after CMSIS_6 is fetched)
set(CMSISCORE "${DEPS_PATH}/CMSIS_6/CMSIS/Core/Include" CACHE PATH "Path to CMSIS Core headers")

# Enable all DSP functions
set(BASICMATH ON CACHE BOOL "Include Basic Math Functions")
set(COMPLEXMATH ON CACHE BOOL "Include Complex Math Functions")
set(CONTROLLER ON CACHE BOOL "Include Controller Functions")
set(FASTMATH ON CACHE BOOL "Include Fast Math Functions")
set(FILTERING ON CACHE BOOL "Include Filtering Functions")
set(MATRIX ON CACHE BOOL "Include Matrix Functions")
set(STATISTICS ON CACHE BOOL "Include Statistics Functions")
set(SUPPORT ON CACHE BOOL "Include Support Functions")
set(TRANSFORM ON CACHE BOOL "Include Transform Functions")
set(SVM ON CACHE BOOL "Include SVM Functions")
set(BAYES ON CACHE BOOL "Include Bayes Functions")
set(DISTANCE ON CACHE BOOL "Include Distance Functions")
set(INTERPOLATION ON CACHE BOOL "Include Interpolation Functions")
set(QUATERNIONMATH ON CACHE BOOL "Include Quaternion Math Functions")
set(WINDOW ON CACHE BOOL "Include Window Functions")

# Disable Neon (not available on Cortex-M33)
set(NEON OFF CACHE BOOL "Neon acceleration")
set(NEONEXPERIMENTAL OFF CACHE BOOL "Neon experimental")

# Disable MVE (not available on Cortex-M33)
set(MVEI OFF CACHE BOOL "MVE Integer")
set(MVEF OFF CACHE BOOL "MVE Float")
set(HELIUM OFF CACHE BOOL "Helium")

# Enable loop unrolling for performance
set(LOOPUNROLL ON CACHE BOOL "Loop unrolling")

# Disable ROUNDING (use default truncation)
set(ROUNDING OFF CACHE BOOL "Rounding")

# Disable matrix check for performance
set(MATRIXCHECK OFF CACHE BOOL "Matrix dimension check")

# Float16 support
set(FLOAT16 OFF CACHE BOOL "Float16 support")

# ==========================================================================
# FetchContent Progress Messages
# ==========================================================================
message(STATUS "")
message(STATUS "=== Fetching External Dependencies ===")

# 1. Download Official STM32 LwIP Middleware
message(STATUS "[1/4] Declaring STM32 LwIP Middleware (stm32-mw-lwip v2.1.3_20241213)...")
FetchContent_Declare(
    FCD_stm32_mw_lwip
    GIT_REPOSITORY https://github.com/STMicroelectronics/stm32-mw-lwip.git
    GIT_TAG        v2.1.3_20241213
    SOURCE_DIR     "${LWIP_DEPS_PATH}/stm32-mw-lwip"
    GIT_PROGRESS   TRUE
)

# 2. Download FreeRTOS-Kernel
message(STATUS "[2/4] Declaring FreeRTOS-Kernel (V11.2.0)...")
FetchContent_Declare(
    FCD_freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG        V11.2.0
    SOURCE_DIR     "${DEPS_PATH}/FreeRTOS-Kernel"
    SOURCE_SUBDIR  "EXTRACT_ONLY"
    GIT_PROGRESS   TRUE
)

# 3. Download CMSIS_6 for core headers (must be before CMSIS-DSP)
message(STATUS "[3/4] Declaring CMSIS_6 (v6.1.0)...")
FetchContent_Declare(
    FCD_cmsis_6
    GIT_REPOSITORY https://github.com/ARM-software/CMSIS_6.git
    GIT_TAG        v6.1.0
    SOURCE_DIR     "${DEPS_PATH}/CMSIS_6"
    SOURCE_SUBDIR  "EXTRACT_ONLY"
    GIT_PROGRESS   TRUE
)

# 4. Download CMSIS-DSP from official ARM GitHub repository
message(STATUS "[4/4] Declaring CMSIS-DSP (v1.16.2)...")
FetchContent_Declare(
    FCD_cmsis_dsp
    GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
    GIT_TAG        v1.16.2
    SOURCE_DIR     "${DEPS_PATH}/CMSIS-DSP"
    GIT_PROGRESS   TRUE
)

# Populate the content at configure time
message(STATUS "")
message(STATUS "Fetching dependencies (this may take a while on first run)...")
message(STATUS "  - STM32 LwIP Middleware: ${LWIP_DEPS_PATH}/stm32-mw-lwip")
message(STATUS "  - FreeRTOS-Kernel: ${DEPS_PATH}/FreeRTOS-Kernel")
message(STATUS "  - CMSIS_6: ${DEPS_PATH}/CMSIS_6")
message(STATUS "  - CMSIS-DSP: ${DEPS_PATH}/CMSIS-DSP")
message(STATUS "")
FetchContent_MakeAvailable(FCD_stm32_mw_lwip FCD_freertos_kernel FCD_cmsis_6 FCD_cmsis_dsp)
message(STATUS "")
message(STATUS "=== Dependencies fetched successfully ===")
message(STATUS "")

# Export CMSIS paths for use by other libraries
set(CMSIS_DSP_PATH "${DEPS_PATH}/CMSIS-DSP" CACHE PATH "Path to CMSIS-DSP library")
set(CMSIS_CORE_PATH "${DEPS_PATH}/CMSIS_6/CMSIS/Core/Include" CACHE PATH "Path to CMSIS Core headers")

# ==========================================================================
# Configure CMSIS-DSP target (created by FetchContent)
# ==========================================================================
if(TARGET CMSISDSP)
    # Add compile definitions for Cortex-M33
    target_compile_definitions(CMSISDSP
        PUBLIC
            ARM_MATH_CM33       # Cortex-M33 core
            ARM_MATH_LOOPUNROLL # Enable loop unrolling
    )

    # Add CMSIS Core include path
    target_include_directories(CMSISDSP
        PUBLIC
            ${CMSIS_CORE_PATH}
    )

    message(STATUS "[CMSIS-DSP] Library configured for Cortex-M33")
else()
    message(WARNING "[CMSIS-DSP] CMSISDSP target not found!")
endif()

# ==========================================================================
# Configure Libraries
# ==========================================================================
message(STATUS "=== Configuring Libraries ===")

# 3rd Party Libs would be added here
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE inc)
target_compile_definitions(freertos_config INTERFACE projCOVERAGE_TEST=0)

set(FREERTOS_PORT "GCC_ARM_CM33_NTZ_NONSECURE" CACHE STRING "FreeRTOS Port")

message(STATUS "[1/5] Adding FreeRTOS-Kernel...")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/FreeRTOS-Kernel)

message(STATUS "[2/5] Adding LwIP stack...")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lwip)

message(STATUS "[3/5] Adding Modbus stack...")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/modbus)

message(STATUS "[4/5] Adding ADC Filter library...")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/adc_filter)

message(STATUS "[5/5] Adding BSP (${BSP_DIR})...")
add_subdirectory(${BSP_DIR})

message(STATUS "=== Libraries configured ===")
message(STATUS "")

# ==========================================================================
# Modbus Code Generation
# ==========================================================================
# Generate Modbus register code from JSON configuration
# Note: Callbacks are maintained manually in application/src/jerry_device_callbacks.c
# to allow custom hardware logic (e.g., I2C relay control)
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
        adc_filter
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

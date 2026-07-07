# Copyright (c) 2026
# All rights reserved.

# Application CMakeLists.txt
include(FetchContent)

# ==========================================================================
# Optional Features
# ==========================================================================
option(ENABLE_I2C_DEVICE_SCAN "Enable I2C device address scanning at startup" OFF)

# ==========================================================================
# Application Version
# ==========================================================================
# These cache variables are set via build.py --version-* or cmake -D.
# The build number (Unix timestamp) is generated fresh on every cmake --build
# call by tools/gen_version.py via the generate_app_version custom target.
set(APP_VERSION_MAJOR "1" CACHE STRING "Application version major number")
set(APP_VERSION_MINOR "0" CACHE STRING "Application version minor number")
set(APP_VERSION_PATCH "0" CACHE STRING "Application version patch number")

# Resolve git commit hash at configure time.
# The hash is fixed per configure run — a new commit requires reconfigure.
# Appends '+' if the repository has uncommitted changes at configure time.
find_program(GIT_EXECUTABLE_APP git)
if(GIT_EXECUTABLE_APP)
    execute_process(
        COMMAND ${GIT_EXECUTABLE_APP} rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE APP_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _git_hash_result
    )
    if(NOT _git_hash_result EQUAL 0)
        set(APP_GIT_HASH "unknown")
    else()
        execute_process(
            COMMAND ${GIT_EXECUTABLE_APP} status --porcelain
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE _git_dirty
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_git_dirty)
            set(APP_GIT_HASH "${APP_GIT_HASH}+")
        endif()
    endif()
else()
    set(APP_GIT_HASH "unknown")
endif()
message(STATUS "[Version] Git hash: ${APP_GIT_HASH}")

# Include Modbus code generation configuration
include(${CMAKE_SOURCE_DIR}/config/modbus_codegen.cmake)

# Sources
file(GLOB_RECURSE APP_SOURCES "src/*.c" "src/*.s")
file(GLOB_RECURSE APP_HEADERS "inc/*.h")
set(APP_HEADERS ${APP_HEADERS} bsp/bsp.h)
set(APP_INCLUDE_DIRECTORIES "")

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
message(STATUS "[4/5] Declaring CMSIS-DSP (v1.16.2)...")
FetchContent_Declare(
    FCD_cmsis_dsp
    GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
    GIT_TAG        v1.16.2
    SOURCE_DIR     "${DEPS_PATH}/CMSIS-DSP"
    GIT_PROGRESS   TRUE
)

# 5. Download embedded-log
message(STATUS "[5/7] Declaring embedded-log (master)...")
FetchContent_Declare(
    FCD_embedded_log
    GIT_REPOSITORY https://github.com/to9/embedded-log.git
    GIT_TAG        master
    SOURCE_DIR     "${DEPS_PATH}/embedded-log"
    GIT_PROGRESS   TRUE
)

# 6. Download SEGGER RTT
message(STATUS "[6/7] Declaring SEGGER RTT (V8.58.0)...")
FetchContent_Declare(
    FCD_segger_rtt
    GIT_REPOSITORY https://github.com/SEGGERMicro/RTT.git
    GIT_TAG        V8.58.0
    SOURCE_DIR     "${DEPS_PATH}/segger_rtt"
    GIT_PROGRESS   TRUE
)

# 7. Download wolfSSL (Secure firmware only — X.509 + ECDSA-P256 for FOTA verification)
# wolfSSL uses user_settings.h for configuration (set WOLFSSL_USER_SETTINGS).
# WOLFSSL_STATIC_MEMORY + WOLFSSL_NO_MALLOC = fully static, no heap.
# Software crypto only for now; hardware accelerators (STM32H5 HASH/PKA) can be
# enabled later by adding WOLFSSL_STM32H5 + WOLFSSL_STM32_HASH + WOLFSSL_STM32_PKA
# to user_settings.h and adding the HAL HASH/PKA drivers to the project.
set(BSP_STM32H563_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bsp/stm/stm32h563")
message(STATUS "[7/7] Declaring wolfSSL (v5.7.4-stable, Secure FOTA crypto)...")
set(WOLFSSL_USER_SETTINGS ON CACHE BOOL "" FORCE)
set(WOLFSSL_USER_SETTINGS_PATH
    "${BSP_STM32H563_DIR}/Secure/Core/Inc/user_settings.h"
    CACHE STRING "wolfSSL user_settings.h path" FORCE)
# WOLFSSL_SINGLE_THREADED: prevents wolfSSL CMake from adding -lpthreads.
# The Secure firmware is single-threaded (SINGLE_THREADED in user_settings.h).
# This CMake variable must be set BEFORE FetchContent_MakeAvailable.
set(WOLFSSL_SINGLE_THREADED ON CACHE BOOL "wolfSSL single-threaded (no pthreads)" FORCE)
FetchContent_Declare(
    FCD_wolfssl
    GIT_REPOSITORY https://github.com/wolfSSL/wolfssl.git
    GIT_TAG        v5.7.4-stable
    GIT_SHALLOW    TRUE
)

# Populate each dependency individually, checking if already present first.
message(STATUS "")
message(STATUS "Fetching dependencies (cached on subsequent runs)...")
message(STATUS "")

FetchContent_GetProperties(FCD_stm32_mw_lwip)
if(NOT fcd_stm32_mw_lwip_POPULATED)
    message(STATUS "  Downloading STM32 LwIP Middleware...")
    FetchContent_MakeAvailable(FCD_stm32_mw_lwip)
else()
    message(STATUS "  STM32 LwIP Middleware: already cached")
endif()

FetchContent_GetProperties(FCD_freertos_kernel)
if(NOT fcd_freertos_kernel_POPULATED)
    message(STATUS "  Downloading FreeRTOS-Kernel...")
    FetchContent_MakeAvailable(FCD_freertos_kernel)
else()
    message(STATUS "  FreeRTOS-Kernel: already cached")
endif()

FetchContent_GetProperties(FCD_cmsis_6)
if(NOT fcd_cmsis_6_POPULATED)
    message(STATUS "  Downloading CMSIS_6...")
    FetchContent_MakeAvailable(FCD_cmsis_6)
else()
    message(STATUS "  CMSIS_6: already cached")
endif()

FetchContent_GetProperties(FCD_cmsis_dsp)
if(NOT fcd_cmsis_dsp_POPULATED)
    message(STATUS "  Downloading CMSIS-DSP...")
    FetchContent_MakeAvailable(FCD_cmsis_dsp)
else()
    message(STATUS "  CMSIS-DSP: already cached")
endif()

FetchContent_GetProperties(FCD_embedded_log)
if(NOT fcd_embedded_log_POPULATED)
    message(STATUS "  Downloading embedded-log...")
    FetchContent_Populate(FCD_embedded_log)
    # Remove default log_cfg.h so our application/inc/log_cfg.h is used
    file(REMOVE "${fcd_embedded_log_SOURCE_DIR}/src/log_cfg.h")
else()
    message(STATUS "  embedded-log: already cached")
endif()

FetchContent_GetProperties(FCD_segger_rtt)
if(NOT fcd_segger_rtt_POPULATED)
    message(STATUS "  Downloading SEGGER RTT...")
    FetchContent_Populate(FCD_segger_rtt)
else()
    message(STATUS "  SEGGER RTT: already cached")
endif()

FetchContent_GetProperties(FCD_wolfssl)
if(NOT fcd_wolfssl_POPULATED)
    message(STATUS "  Downloading wolfSSL v5.7.4-stable...")
    FetchContent_MakeAvailable(FCD_wolfssl)
else()
    message(STATUS "  wolfSSL: already cached")
endif()

# Export wolfSSL include path as a cache variable so subdirectories (BSP) can use it.
# fcd_wolfssl_SOURCE_DIR is only available in this scope after FetchContent_MakeAvailable/GetProperties.
set(WOLFSSL_INCLUDE_DIR "${fcd_wolfssl_SOURCE_DIR}"
    CACHE PATH "wolfSSL include directory (root — wolfssl/ headers are under root)" FORCE)

# ==========================================================================
# Configure wolfSSL target
# All wolfssl target configuration lives here — the target is created by
# FetchContent_MakeAvailable above, so this is the natural place to configure it.
# ==========================================================================
if(TARGET wolfssl)
    # Cortex-M33 ISA flags — required so the linker can resolve calls from
    # secure_nsc.c (compiled with -mcmse) into wolfSSL.
    # -Wno-pedantic: wolfSSL sources become empty TUs when features are disabled
    # (e.g. NO_SHA, NO_AES) — -Wpedantic treats empty TUs as errors.
    target_compile_options(wolfssl PRIVATE
        -mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16
        -Wno-pedantic
    )
    # wolfSSL settings.h does: #include "user_settings.h" when WOLFSSL_USER_SETTINGS is defined.
    # Add Secure/Core/Inc so the wolfssl build can find our user_settings.h.
    target_include_directories(wolfssl PRIVATE
        "${BSP_STM32H563_DIR}/Secure/Core/Inc"
    )
endif()

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

message(STATUS "[1] Adding FreeRTOS-Kernel...")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/FreeRTOS-Kernel)

message(STATUS "[1.5] Adding embedded-log library...")
add_library(embedded_log STATIC "${fcd_embedded_log_SOURCE_DIR}/src/log.c")
target_include_directories(embedded_log PUBLIC "${fcd_embedded_log_SOURCE_DIR}/src")
# Allow overriding log_cfg.h from application/inc
target_include_directories(embedded_log BEFORE PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")

message(STATUS "[1.6] Adding SEGGER RTT library...")
add_library(segger_rtt STATIC 
    "${fcd_segger_rtt_SOURCE_DIR}/RTT/SEGGER_RTT.c"
    "${fcd_segger_rtt_SOURCE_DIR}/RTT/SEGGER_RTT_printf.c"
)
target_include_directories(segger_rtt PUBLIC "${fcd_segger_rtt_SOURCE_DIR}/RTT")
# Allow overriding SEGGER_RTT_Conf.h
target_include_directories(segger_rtt BEFORE PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")

target_link_libraries(embedded_log PUBLIC segger_rtt)

# Propagate includes to BSP and FreeRTOS which use log macros
target_link_libraries(freertos_kernel PUBLIC embedded_log)

message(STATUS "[2] Adding LwIP stack...")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lwip)
target_link_libraries(lwip_stack PUBLIC embedded_log)

message(STATUS "[3] Adding Modbus stack...")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/modbus)
target_link_libraries(modbus_stack PUBLIC embedded_log)

message(STATUS "[4] Adding ADC Filter library...")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/adc_filter)

message(STATUS "[5] Adding BSP (${BSP_DIR})...")
add_subdirectory(${BSP_DIR})

message(STATUS "[6] Adding I2C LCD library...")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lcd_i2c)

# Propagate includes to BSP which uses log macros
target_link_libraries(stm32h563_bsp PUBLIC embedded_log)
target_link_libraries(jerry_secure_app PUBLIC embedded_log)

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
    "${CMAKE_BINARY_DIR}/generated"
    ${APP_INCLUDE_DIRECTORIES}
)

# Add Modbus generated sources to the application
modbus_add_generated_sources(
    TARGET jerry_app
    GENERATED_FILES ${MODBUS_GENERATED_SOURCES}
)

# ==========================================================================
# Optional Feature Configuration
# ==========================================================================
if(ENABLE_I2C_DEVICE_SCAN)
    target_compile_definitions(jerry_app PRIVATE ENABLE_I2C_DEVICE_SCAN=1)
    message(STATUS "[Optional Feature] I2C Device Scan: ENABLED")
    message(STATUS "  - I2C bus will be scanned at startup")
    message(STATUS "  - All detected device addresses will be printed to console")
else()
    message(STATUS "[Optional Feature] I2C Device Scan: DISABLED")
    message(STATUS "  - To enable: cmake -DENABLE_I2C_DEVICE_SCAN=ON ..")
endif()

target_link_libraries(jerry_app
    PRIVATE
        "-Wl,--whole-archive"
        lwip_stack
        stm32h563_bsp
        modbus_stack
        "-Wl,--no-whole-archive"
        freertos_kernel
        adc_filter
        lcd_i2c
        embedded_log
)

# ==========================================================================
# Build-time version header generation
# ==========================================================================
# This custom target ALWAYS runs on every cmake --build call, regenerating
# app_version.h with a fresh Unix timestamp as the build number.
# jerry_device_registers.c includes app_version.h and will recompile when
# the timestamp changes, ensuring the version info is always up-to-date.
add_custom_target(generate_app_version ALL
    COMMAND ${UV_RUN_CMD} python "${CMAKE_SOURCE_DIR}/tools/gen_version.py"
            --output "${CMAKE_BINARY_DIR}/generated/app_version.h"
            --major "${APP_VERSION_MAJOR}"
            --minor "${APP_VERSION_MINOR}"
            --patch "${APP_VERSION_PATCH}"
            --git-hash "${APP_GIT_HASH}"
    COMMENT "Generating app_version.h (${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH} ${APP_GIT_HASH})"
    VERBATIM
)

# Ensure jerry_secure_app is built first so import lib exists
add_dependencies(jerry_app jerry_secure_app generate_app_version)

# Compiler options are inherited from toolchain but we can add more specific ones
target_compile_options(jerry_app PRIVATE
    -Wall -Wextra -Werror -Wfatal-errors
    $<$<CONFIG:Debug>:-gdwarf-4>
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

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET jerry_app POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --strip-all $<TARGET_FILE:jerry_app>
        COMMENT "Stripping all symbols from jerry_app (Release build)"
    )
endif()

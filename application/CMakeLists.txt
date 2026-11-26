# Copyright (c) 2026
# All rights reserved.

# Application CMakeLists.txt
include(FetchContent)
include(${CMAKE_SOURCE_DIR}/config/modbus_codegen.cmake)
include(${CMAKE_SOURCE_DIR}/config/threadx_config.cmake)
include(${CMAKE_SOURCE_DIR}/config/cmsis_dsp_config.cmake)

# ==========================================================================
# Optional Features
# ==========================================================================
option(CMAKE_ENABLE_I2C_DEVICE_SCAN "Enable I2C device address scanning at startup" OFF)
option(CMAKE_ENABLE_SYSLOG "Enable syslog remote logging via UDP" ON)
option(CMAKE_ENABLE_DHCP "Enable DHCP client" ON)
option(CMAKE_ENABLE_SNTP "Enable SNTP time synchronization" ON)
option(CMAKE_ENABLE_MDNS "Enable mDNS responder" ON)

if(CMAKE_ENABLE_DHCP)
    add_compile_definitions(CMAKE_ENABLE_DHCP=1)
    message(STATUS "[Optional Feature] DHCP: ENABLED")
    message(STATUS "  - IP address will be assigned automatically via DHCP")
else()
    add_compile_definitions(CMAKE_ENABLE_DHCP=0)
    message(STATUS "[Optional Feature] DHCP: DISABLED")
    message(STATUS "  - To enable: cmake -DCMAKE_ENABLE_DHCP=ON ..")
endif()

if(CMAKE_ENABLE_SNTP)
    add_compile_definitions(CMAKE_ENABLE_SNTP=1)
    message(STATUS "[Optional Feature] SNTP: ENABLED")
    message(STATUS "  - RTC time will be synchronized via NTP")
else()
    add_compile_definitions(CMAKE_ENABLE_SNTP=0)
    message(STATUS "[Optional Feature] SNTP: DISABLED")
    message(STATUS "  - To enable: cmake -DCMAKE_ENABLE_SNTP=ON ..")
endif()

if(CMAKE_ENABLE_MDNS)
    add_compile_definitions(CMAKE_ENABLE_MDNS=1)
    message(STATUS "[Optional Feature] mDNS: ENABLED")
    message(STATUS "  - Device will be discoverable on the local network")
else()
    add_compile_definitions(CMAKE_ENABLE_MDNS=0)
    message(STATUS "[Optional Feature] mDNS: DISABLED")
    message(STATUS "  - To enable: cmake -DCMAKE_ENABLE_MDNS=ON ..")
endif()

# ==========================================================================
# Application Version
# ==========================================================================

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
# FetchContent Progress Messages
# ==========================================================================
message(STATUS "")
message(STATUS "=== Fetching External Dependencies ===")

FetchContent_Declare(
    FCD_stm32_mw_lwip
    GIT_REPOSITORY https://github.com/STMicroelectronics/stm32-mw-lwip.git
    GIT_TAG        v2.1.3_20241213
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    FCD_threadx_kernel
    GIT_REPOSITORY https://github.com/eclipse-threadx/threadx.git
    GIT_TAG        v6.5.2.202603_rel
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    FCD_cmsis_6
    GIT_REPOSITORY https://github.com/ARM-software/CMSIS_6.git
    GIT_TAG        v6.1.0
    SOURCE_SUBDIR  "EXTRACT_ONLY"
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    FCD_cmsis_dsp
    GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
    GIT_TAG        v1.16.2
    GIT_PROGRESS   TRUE
)

FetchContent_Declare(
    FCD_segger_rtt
    GIT_REPOSITORY https://github.com/SEGGERMicro/RTT.git
    GIT_TAG        V8.58.0
    GIT_PROGRESS   TRUE
)

message("fetching lwip")
FetchContent_MakeAvailable(FCD_stm32_mw_lwip)
message("fetching threadx")
FetchContent_MakeAvailable(FCD_threadx_kernel)
message("fetching cmsis")
FetchContent_MakeAvailable(FCD_cmsis_6)
FetchContent_GetProperties(FCD_cmsis_6 SOURCE_DIR CMSIS_CORE_PATH)

message("fetching cmsis_dsp")
set(CMSISCORE ${CMSIS_CORE_PATH}/CMSIS/Core CACHE STRING "" FORCE)
FetchContent_MakeAvailable(FCD_cmsis_dsp)
FetchContent_GetProperties(FCD_cmsis_dsp SOURCE_DIR CMSIS_DSP_PATH)

message("fetching segger rtt")
FetchContent_MakeAvailable(FCD_segger_rtt)
FetchContent_GetProperties(FCD_segger_rtt SOURCE_DIR SEGGER_RTT_SRC_PATH)

message(STATUS "")
message(STATUS "=== Dependencies fetched successfully ===")
message(STATUS "")

# segger rtt does not comes with CMAKE. Need to add manually

add_library(segger_rtt STATIC 
    "${SEGGER_RTT_SRC_PATH}/RTT/SEGGER_RTT.c"
    "${SEGGER_RTT_SRC_PATH}/RTT/SEGGER_RTT_printf.c"
)
target_include_directories(segger_rtt PUBLIC "${SEGGER_RTT_SRC_PATH}/RTT")
target_include_directories(segger_rtt BEFORE PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc")


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

add_executable(jerry_app ${APP_SOURCES} ${APP_HEADERS})

target_include_directories(jerry_app PRIVATE
    inc
    bsp
    src/generated
    "${CMAKE_BINARY_DIR}/generated"
    ${APP_INCLUDE_DIRECTORIES}
    ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lcd_i2c/inc
)

# Add Modbus generated sources to the application
modbus_add_generated_sources(
    TARGET jerry_app
    GENERATED_FILES ${MODBUS_GENERATED_SOURCES}
)

# ==========================================================================
# Optional Feature Configuration
# ==========================================================================
if(CMAKE_ENABLE_I2C_DEVICE_SCAN)
    target_compile_definitions(jerry_app PRIVATE CMAKE_ENABLE_I2C_DEVICE_SCAN=1)
    message(STATUS "[Optional Feature] I2C Device Scan: ENABLED")
    message(STATUS "  - I2C bus will be scanned at startup")
    message(STATUS "  - All detected device addresses will be printed to console")
else()
    message(STATUS "[Optional Feature] I2C Device Scan: DISABLED")
    message(STATUS "  - To enable: cmake -DCMAKE_ENABLE_I2C_DEVICE_SCAN=ON ..")
endif()

if(CMAKE_ENABLE_SYSLOG)
    target_compile_definitions(jerry_app PRIVATE CMAKE_ENABLE_SYSLOG=1)
    message(STATUS "[Optional Feature] Syslog: ENABLED")
    message(STATUS "  - Logs will be sent via UDP to remote syslog server")
else()
    target_compile_definitions(jerry_app PRIVATE CMAKE_ENABLE_SYSLOG=0)
    message(STATUS "[Optional Feature] Syslog: DISABLED")
    message(STATUS "  - To enable: cmake -DCMAKE_ENABLE_SYSLOG=ON ..")
endif()

target_link_libraries(jerry_app
    PRIVATE
        "-Wl,--whole-archive"
        lwip_stack
        stm32h563_bsp
        modbus_stack
        "-Wl,--no-whole-archive"
        adc_filter
        lcd_i2c
        segger_rtt
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
            --major "${PROJECT_VERSION_MAJOR}"
            --minor "${PROJECT_VERSION_MINOR}"
            --patch "${PROJECT_VERSION_PATCH}"
            --git-hash "${APP_GIT_HASH}"
    COMMENT "Generating app_version.h (${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} ${APP_GIT_HASH})"
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

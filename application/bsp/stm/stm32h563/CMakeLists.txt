cmake_minimum_required(VERSION 3.22)

# Define the BSP library
add_library(stm32h563_bsp STATIC)

# Sources
target_sources(stm32h563_bsp PRIVATE
    NonSecure/Core/Startup/startup_stm32h563xx.s
    NonSecure/Core/Src/system_stm32h5xx_ns.c
    NonSecure/Core/Src/stm32h5xx_hal_msp.c
    NonSecure/Core/Src/stm32h5xx_it.c
    NonSecure/Core/Src/syscalls.c
    NonSecure/Core/Src/sysmem.c
    NonSecure/Core/Src/main.c
    NonSecure/Core/Src/stm32h5xx_hal_timebase_tim.c
)

# HAL Drivers
file(GLOB HAL_SOURCES "Drivers/STM32H5xx_HAL_Driver/Src/*.c")
list(FILTER HAL_SOURCES EXCLUDE REGEX ".*_template\\.c$")

file(GLOB BSP_SOURCES "Drivers/BSP/STM32H5xx_Nucleo/*.c")
target_sources(stm32h563_bsp PRIVATE
    ${HAL_SOURCES}
    ${BSP_SOURCES}
)

# Include Directories
target_include_directories(stm32h563_bsp PUBLIC
    Drivers/STM32H5xx_HAL_Driver/Inc
    Drivers/STM32H5xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32H5xx/Include
    Drivers/CMSIS/Include
    Drivers/BSP/STM32H5xx_Nucleo
    NonSecure/Core/Inc
    Secure_nsclib
)
target_compile_options(stm32h563_bsp PRIVATE
    -Wall -Wextra -Werror -g -gdwarf-4 -Wno-unused-parameter -Wno-old-style-declaration
)
# Compiler Definitions
target_compile_definitions(stm32h563_bsp PUBLIC
    STM32H563xx
    USE_HAL_DRIVER
)

# Define the Secure Application
add_executable(jerry_secure_app)

file(GLOB SECURE_SOURCES "Secure/Core/Src/*.c" "Secure/Core/Startup/*.s")

target_sources(jerry_secure_app PRIVATE
    ${SECURE_SOURCES}
    ${HAL_SOURCES}
)

target_include_directories(jerry_secure_app PUBLIC
    Drivers/STM32H5xx_HAL_Driver/Inc
    Drivers/STM32H5xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32H5xx/Include
    Drivers/CMSIS/Include
    Drivers/BSP/STM32H5xx_Nucleo
    Secure/Core/Inc
    Secure_nsclib
)

target_compile_options(jerry_secure_app PRIVATE
    -Wall -Wextra -Werror -g -gdwarf-4 -Wno-unused-parameter -Wno-old-style-declaration
    -mcmse
)

target_compile_definitions(jerry_secure_app PUBLIC
    STM32H563xx
    USE_HAL_DRIVER
)

target_link_options(jerry_secure_app PRIVATE
    -T "${CMAKE_CURRENT_SOURCE_DIR}/Secure/STM32H563xx_FLASH_s.ld"
    -Wl,--out-implib=${CMAKE_BINARY_DIR}/libsecure_nsclib.a
    -Wl,--cmse-implib
    -Wl,-Map=${CMAKE_BINARY_DIR}/jerry_secure_app.map
)

add_custom_command(TARGET jerry_secure_app POST_BUILD
    COMMAND ${CMAKE_OBJDUMP} -d $<TARGET_FILE:jerry_secure_app> > $<TARGET_FILE_DIR:jerry_secure_app>/jerry_secure_app.asm
    COMMENT "Generating disassembly for jerry_secure_app"
)

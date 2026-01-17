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

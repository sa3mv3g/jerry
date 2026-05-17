cmake_minimum_required(VERSION 3.22)

# Define the BSP library
add_library(stm32h563_bsp STATIC)

set(MX_Defines_Syms
	USE_HAL_DRIVER
	STM32H563xx
	USE_FULL_LL_DRIVER
	USE_NUCLEO_64
    FLASH_LINES_128B
    $<$<CONFIG:Debug>:DEBUG>
)
# STM32CubeMX generated include paths
set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Secure_nsclib
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/CMSIS/Device/ST/STM32H5xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/BSP/STM32H5xx_Nucleo
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/CMSIS/Include
)
# STM32CubeMX generated application sources
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/Core/Src/stm32h5xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/Core/Src/stm32h5xx_hal_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/Core/Src/stm32h5xx_hal_timebase_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/Core/Startup/startup_stm32h563xx.s
)

# STM32 HAL/LL Drivers
set(STM32_Drivers_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/Core/Src/system_stm32h5xx_ns.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_gtzc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_tim_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_adc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_adc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_crc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_eth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_eth_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_i2c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_i2c_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_iwdg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_uart_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_hcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_usb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_usart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_usart_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/NonSecure/../Drivers/BSP/STM32H5xx_Nucleo/stm32h5xx_nucleo.c
)

# Sources
target_sources(stm32h563_bsp PRIVATE
    ${STM32_Drivers_Src}
    ${MX_Application_Src}
    NonSecure/Core/Src/system_stm32h5xx_ns.c
    Middlewares/ST/EEPROM_Emul/Core/eeprom_emul.c
    Middlewares/ST/EEPROM_Emul/Porting/STM32H5/flash_interface.c
)

# Include Directories
target_include_directories(stm32h563_bsp PUBLIC
    ${MX_Include_Dirs}
    Middlewares/ST/EEPROM_Emul
    Middlewares/ST/EEPROM_Emul/Core
    Middlewares/ST/EEPROM_Emul/Porting/STM32H5
    ../../../../application/bsp
)
target_compile_options(stm32h563_bsp PRIVATE
    -Wall -Wextra -Werror -g -gdwarf-4 -Wno-unused-parameter -Wno-old-style-declaration
)
# Compiler Definitions
target_compile_definitions(stm32h563_bsp PUBLIC
    ${MX_Defines_Syms}
    # ETH DMA descriptor counts - increased from default 4 to prevent RBU/TBU errors
    # RBU (Receive Buffer Unavailable) occurs when DMA runs out of descriptors
    # Higher count provides more buffering for bursty traffic
    # These definitions override values in stm32h5xx_hal_conf.h
    ETH_RX_DESC_CNT=8U
    ETH_TX_DESC_CNT=8U
)

#[=============================================================================[
Manual workflow:
    copy contents of the Secure/mx-generated.cmake
    and edit it
#]=============================================================================]


# Define the Secure Application
set(MX_Defines_Syms
	USE_HAL_DRIVER
	STM32H563xx
    $<$<CONFIG:Debug>:DEBUG>
)
# STM32CubeMX generated include paths
set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Secure_nsclib
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/CMSIS/Device/ST/STM32H5xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/CMSIS/Include
)
# STM32CubeMX generated application sources
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/Core/Src/stm32h5xx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/Core/Src/stm32h5xx_hal_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/Core/Src/secure_nsc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/Core/Startup/startup_stm32h563xx.s
)

# STM32 HAL/LL Drivers
set(STM32_Drivers_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/Core/Src/system_stm32h5xx_s.c
    # ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_utils.c
    # ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_dma_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_rcc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_flash_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_pwr_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_exti.c
    # ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_ll_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_gtzc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Secure/../Drivers/STM32H5xx_HAL_Driver/Src/stm32h5xx_hal_icache.c
)


add_executable(jerry_secure_app)

target_sources(jerry_secure_app PRIVATE
    ${MX_Application_Src}
    ${STM32_Drivers_Src}
)

target_include_directories(jerry_secure_app PUBLIC
    ${MX_Include_Dirs}
    Drivers/BSP/STM32H5xx_Nucleo
)

target_compile_options(jerry_secure_app PRIVATE
    -Wall -Wextra -Werror -g -gdwarf-4 -Wno-unused-parameter -Wno-old-style-declaration
    -mcmse
)

target_compile_definitions(jerry_secure_app PUBLIC
    ${MX_Defines_Syms}
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

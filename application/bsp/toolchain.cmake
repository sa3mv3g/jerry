if(CHIPS_CFG STREQUAL "stm32h563" OR "$ENV{CHIPS_CFG}" STREQUAL "stm32h563")

    set(CMAKE_TOOLCHAIN_FILE "${BSP_DIR}/stm32h563/toolchain.cmake")

    # Set MCU Flags for STM32H563 (Cortex-M33)
    set(STM32_MCU_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")

    # Set Linker Script
    # Path relative to CMAKE_SOURCE_DIR (gcc-arm-none-eabi.cmake prepends CMAKE_SOURCE_DIR)
    set(STM32_LINKER_SCRIPT "application/bsp/stm32h563/NonSecure/STM32H563xx_FLASH_ns.ld")

    # Include the chip toolchain file
    include("${CMAKE_CURRENT_LIST_DIR}/stm32h563/gcc-arm-none-eabi.cmake")

elseif(CHIPS_CFG STREQUAL "stm32f429" OR "$ENV{CHIPS_CFG}" STREQUAL "stm32f429")

    set(CMAKE_TOOLCHAIN_FILE "${BSP_DIR}/stm32f429/cmake/gcc-arm-none-eabi.cmake")

    # Set MCU Flags for STM32F429 (Cortex-M4)
    set(STM32_MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")

    # Set Linker Script
    set(STM32_LINKER_SCRIPT "application/bsp/stm32f429/STM32F429XX_FLASH.ld")

    # Include the chip toolchain file from cmake subfolder
    include("${CMAKE_CURRENT_LIST_DIR}/stm32f429/cmake/gcc-arm-none-eabi.cmake")

else()
    message(FATAL_ERROR "Unsupported chip: ${CHIPS_CFG}. Supported chips: stm32h563, stm32f429")
endif()

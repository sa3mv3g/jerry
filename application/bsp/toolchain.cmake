if(VENDOR STREQUAL "stm" OR "$ENV{VENDOR}" STREQUAL "stm")

    set(CMAKE_TOOLCHAIN_FILE "${BSP_DIR}/stm/toolchain.cmake")

    # Set MCU Flags for STM32H563 (Cortex-M33)
    set(STM32_MCU_FLAGS "-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16")

    # Set Linker Script
    # Path relative to CMAKE_SOURCE_DIR (gcc-arm-none-eabi.cmake prepends CMAKE_SOURCE_DIR)
    set(STM32_LINKER_SCRIPT "application/bsp/stm/stm32h563/NonSecure/STM32H563xx_FLASH_ns.ld")

    # Include the vendor toolchain file
    include("${CMAKE_CURRENT_LIST_DIR}/stm/stm32h563/gcc-arm-none-eabi.cmake")

else()
    message(FATAL_ERROR "Unsupported Vendor: ${VENDOR}")
endif()

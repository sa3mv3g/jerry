# Copyright (c) 2026
# All rights reserved.

# CMSIS-Driver Library for STM32H563
add_library(cmsis_driver_stm32h563
    Source/Driver_GPIO.c
    Source/Driver_USART.c
    Source/Driver_I2C.c
    Source/Driver_SPI.c
)

target_include_directories(cmsis_driver_stm32h563
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# Link against HAL library
target_link_libraries(cmsis_driver_stm32h563
    PUBLIC
        stm32h563_hal
)

# Export include path for consumers
set(CMSIS_DRIVER_STM32H563_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Include" CACHE PATH "Path to CMSIS-Driver STM32H563 headers")
cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Compiler options
set(STM32_MCU_FLAGS  "-mcpu=cortex-m33 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mcmse " )

#Linker options
set(STM32_LINKER_SCRIPT STM32H563xx_FLASH_s.ld)
set(STM32_LINKER_OPTION  -Wl,--cmse-implib\ -Wl,--out-implib="${CMAKE_SOURCE_DIR}/../Secure_nsclib/secure_nsclib.o")

# Include toolchain file
include("../gcc-arm-none-eabi.cmake")

# Set the project name
set(CMAKE_PROJECT_NAME stm32h563_S)
project(${CMAKE_PROJECT_NAME})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Include mx-generated 
include("mx-generated.cmake")


# In order to use CMake for cross-compiling
set(CMAKE_CXX_COMPILER_FORCED "true") 

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # EDATA_ENABLED: switches EEPROM emulation to use Bank 1 high-cycle data area.
    # In Secure build (CMSE=3), FLASH_EDATA_BASE resolves to FLASH_EDATA_BASE_S = 0x0D000000
    # (Secure alias, stable across SWAP_BANK changes).
    # Do NOT define SECURE_FEATURES — that would force 0x09000000 (NS alias, swaps after FOTA).
    EDATA_ENABLED
)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # EEPROM emulation middleware headers (Secure build only)
    # Located under Secure/Core/EEPROM_Emul to avoid CubeMX regeneration overwriting it
    # (CubeMX only regenerates Middlewares/ST/ under the STM32H563 root)
    ${CMAKE_CURRENT_SOURCE_DIR}/core/EEPROM_Emul/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/EEPROM_Emul/Porting/STM32H5
    # eeprom_emul_conf.h is co-located in EEPROM_Emul/Core/ (copied from application/bsp/)
    # No separate BSP path needed
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    core/Src/main.c
    # EEPROM emulation middleware (Secure build — uses FLASH_EDATA_BASE_S = 0x0D000000)
    core/EEPROM_Emul/Core/eeprom_emul.c
    core/EEPROM_Emul/Porting/STM32H5/flash_interface.c
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined libraries
)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()










# CMakeLists.txt for ADC Filter Library
#
# This library provides digital filtering for ADC inputs using CMSIS-DSP
# biquad cascade filters.
#
# Dependencies:
#   - CMSISDSP library (built from application/dependencies/cmsis_dsp)
#
# Usage:
#   add_subdirectory(dependencies/adc_filter)
#   target_link_libraries(your_target PRIVATE adc_filter)

cmake_minimum_required(VERSION 3.16)

# Library name
set(LIB_NAME adc_filter)

# ==========================================================================
# ADC Filter Library Sources
# ==========================================================================
set(ADC_FILTER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/adc_filter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/adc_filter_coefficients.c
)

# Header files (for IDE integration)
set(ADC_FILTER_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/adc_filter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inc/adc_filter_coefficients.h
)

# ==========================================================================
# Create Static Library
# ==========================================================================
add_library(${LIB_NAME} STATIC
    ${ADC_FILTER_SOURCES}
    ${ADC_FILTER_HEADERS}
)

# Include directories
target_include_directories(${LIB_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

# Compiler options
target_compile_options(${LIB_NAME}
    PRIVATE
        -Wall
        -Wextra
        -Wno-pedantic
        -Wno-unused-parameter
)

# ==========================================================================
# Link against CMSIS-DSP library
# ==========================================================================
# The CMSISDSP target is created by dependencies/cmsis_dsp/CMakeLists.txt
target_link_libraries(${LIB_NAME}
    PUBLIC
        CMSISDSP
)

message(STATUS "[adc_filter] ADC Filter library configured (using CMSIS-DSP)")

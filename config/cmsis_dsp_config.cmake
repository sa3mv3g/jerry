
# ==========================================================================
# CMSIS-DSP Configuration (must be set BEFORE FetchContent_MakeAvailable)
# ==========================================================================

# Target processor - STM32H5 uses Cortex-M33
set(ARM_CPU "cortex-m33" CACHE STRING "ARM CPU type")

# Set CMSIS Core path (will be set after CMSIS_6 is fetched)
set(CMSISCORE "${DEPS_PATH}/CMSIS_6/CMSIS/Core/Include" CACHE PATH "Path to CMSIS Core headers")

# Enable all DSP functions
set(BASICMATH OFF CACHE BOOL "Include Basic Math Functions")
set(COMPLEXMATH OFF CACHE BOOL "Include Complex Math Functions")
set(CONTROLLER OFF CACHE BOOL "Include Controller Functions")
set(FASTMATH OFF CACHE BOOL "Include Fast Math Functions")
set(FILTERING ON CACHE BOOL "Include Filtering Functions")
set(MATRIX OFF CACHE BOOL "Include Matrix Functions")
set(STATISTICS OFF CACHE BOOL "Include Statistics Functions")
set(SUPPORT OFF CACHE BOOL "Include Support Functions")
set(TRANSFORM OFF CACHE BOOL "Include Transform Functions")
set(SVM OFF CACHE BOOL "Include SVM Functions")
set(BAYES OFF CACHE BOOL "Include Bayes Functions")
set(DISTANCE OFF CACHE BOOL "Include Distance Functions")
set(INTERPOLATION OFF CACHE BOOL "Include Interpolation Functions")
set(QUATERNIONMATH OFF CACHE BOOL "Include Quaternion Math Functions")
set(WINDOW OFF CACHE BOOL "Include Window Functions")

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

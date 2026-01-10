/*
 * Copyright (c) 2026 Advance Instrumentation 'n' Control Systems
 * All rights reserved.
 */

# Application CMakeLists.txt

# Sources
file(GLOB_RECURSE APP_SOURCES "src/*.c" "src/*.s")
file(GLOB_RECURSE APP_HEADERS "inc/*.h")

# 3rd Party Libs would be added here
# add_subdirectory(dependencies/FreeRTOS)

add_executable(jerry_app
    ${APP_SOURCES}
    ${APP_HEADERS}
)

target_include_directories(jerry_app PRIVATE
    inc
    CMSIS_5
    ${BSP_DIR}
)

# Compiler options are inherited from toolchain but we can add more specific ones
target_compile_options(jerry_app PRIVATE
    -Wall -Wextra -Werror
)

# Linker options
target_link_options(jerry_app PRIVATE
    -Wl,-Map=${CMAKE_BINARY_DIR}/jerry_app.map
)

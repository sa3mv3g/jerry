add_library(cmsis INTERFACE)

target_include_directories(cmsis INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/CMSIS_5/CMSIS/Core/Include
)

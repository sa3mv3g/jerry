SET(CMSIS_PORT_SRC "")
SET(CMSIS_PORT_INC "")

add_library(cmsis_ported STATIC)

# port subdirectory should complete ${CMSIS_PORT_SRC} and ${CMSIS_PORT_INC}
add_subdirectory(port)

target_sources(cmsis_ported PRIVATE ${CMSIS_PORT_SRC})

# PUBLIC allows any target linking to 'cmsis' to see these headers.
target_include_directories(cmsis_ported PUBLIC 
    "CMSIS_5/CMSIS/Driver/Include"
    ${CMSIS_PORT_INC}
)

target_compile_options(cmsis_ported PRIVATE
    -Wall -Wextra -Werror -g -gdwarf-4 
)

# Link library 'bsp_interface' PRIVATELY
# This means 'cmsis' uses the BSP internally, but targets linking 
# to 'cmsis' won't automatically link to 'bsp_interface'.
target_link_libraries(cmsis_ported PRIVATE bsp_interface)

# Optional: Add standard CMSIS definitions if required
target_compile_definitions(cmsis_ported PUBLIC 
    CMSIS_DRV_ETH_INTERFACE=1
)

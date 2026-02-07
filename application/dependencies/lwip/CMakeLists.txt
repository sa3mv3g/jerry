cmake_minimum_required(VERSION 3.16)
project(lwip_stack C)

# For stm32f429, LwIP is already included in the BSP folder
# Skip building the lwip_stack library from the fetched dependency
if(CHIPS_CFG STREQUAL "stm32f429")
    message(STATUS "  -> Skipping lwip_stack build for stm32f429 (using BSP-provided LwIP)")
    # Create an INTERFACE library so that targets can still link to lwip_stack
    # The actual LwIP sources are compiled as part of stm32f429_bsp
    add_library(lwip_stack INTERFACE)
    return()
endif()

SET(LWIP_PORT_SRC "")
SET(LWIP_PORT_INC "")

# Define Path for the cloned STM32 lwIP middleware
set(STM32_LWIP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stm32-mw-lwip")

# Create the Library
add_library(lwip_stack STATIC)

# include port subdir (custom port files based on vendor)
add_subdirectory(port)

# Include Directories
target_include_directories(lwip_stack PUBLIC
    "${LWIP_PORT_INC}"
    "${STM32_LWIP_DIR}/src/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/"
)

# Chip-specific include directories
if(CHIPS_CFG STREQUAL "stm32h563")
    target_include_directories(lwip_stack PRIVATE
        "${CMAKE_SOURCE_DIR}/application/inc"
        "${CMAKE_SOURCE_DIR}/application/bsp/stm32h563/NonSecure/Core/Inc"
    )
elseif(CHIPS_CFG STREQUAL "stm32f429")
    target_include_directories(lwip_stack PRIVATE
        "${CMAKE_SOURCE_DIR}/application/inc"
        "${CMAKE_SOURCE_DIR}/application/bsp/stm32f429/Core/Inc"
    )
endif()

# STM32 LwIP Middleware Source Files
# Core files
target_sources(lwip_stack PRIVATE
    # Core
    "${STM32_LWIP_DIR}/src/core/init.c"
    "${STM32_LWIP_DIR}/src/core/def.c"
    "${STM32_LWIP_DIR}/src/core/dns.c"
    "${STM32_LWIP_DIR}/src/core/inet_chksum.c"
    "${STM32_LWIP_DIR}/src/core/ip.c"
    "${STM32_LWIP_DIR}/src/core/mem.c"
    "${STM32_LWIP_DIR}/src/core/memp.c"
    "${STM32_LWIP_DIR}/src/core/netif.c"
    "${STM32_LWIP_DIR}/src/core/pbuf.c"
    "${STM32_LWIP_DIR}/src/core/raw.c"
    "${STM32_LWIP_DIR}/src/core/stats.c"
    "${STM32_LWIP_DIR}/src/core/sys.c"
    "${STM32_LWIP_DIR}/src/core/tcp.c"
    "${STM32_LWIP_DIR}/src/core/tcp_in.c"
    "${STM32_LWIP_DIR}/src/core/tcp_out.c"
    "${STM32_LWIP_DIR}/src/core/timeouts.c"
    "${STM32_LWIP_DIR}/src/core/udp.c"
    # IPv4
    "${STM32_LWIP_DIR}/src/core/ipv4/etharp.c"
    "${STM32_LWIP_DIR}/src/core/ipv4/dhcp.c"
    "${STM32_LWIP_DIR}/src/core/ipv4/autoip.c"
    "${STM32_LWIP_DIR}/src/core/ipv4/icmp.c"
    "${STM32_LWIP_DIR}/src/core/ipv4/ip4.c"
    "${STM32_LWIP_DIR}/src/core/ipv4/ip4_addr.c"
    "${STM32_LWIP_DIR}/src/core/ipv4/ip4_frag.c"
    "${STM32_LWIP_DIR}/src/core/ipv4/igmp.c"
    # Netif
    "${STM32_LWIP_DIR}/src/netif/ethernet.c"
    # API
    "${STM32_LWIP_DIR}/src/api/tcpip.c"
    "${STM32_LWIP_DIR}/src/api/api_lib.c"
    "${STM32_LWIP_DIR}/src/api/api_msg.c"
    "${STM32_LWIP_DIR}/src/api/err.c"
    "${STM32_LWIP_DIR}/src/api/netbuf.c"
    "${STM32_LWIP_DIR}/src/api/sockets.c"
    "${STM32_LWIP_DIR}/src/api/netdb.c"
    "${STM32_LWIP_DIR}/src/api/netifapi.c"
    # Port sources (ethernetif.c, lan8742.c, sys_arch.c)
    "${LWIP_PORT_SRC}"
)

target_compile_options(lwip_stack PRIVATE
    -Wall -Wextra -Werror -g -gdwarf-4
    -Wno-unused-parameter  # CubeMX-generated code has unused parameters
    -Wno-pedantic          # CubeMX-generated code has extra semicolons
)

# Link to your FreeRTOS target and the BSP (chip-specific)
if(CHIPS_CFG STREQUAL "stm32h563")
    target_link_libraries(lwip_stack PRIVATE
        freertos_kernel
        stm32h563_bsp
    )
elseif(CHIPS_CFG STREQUAL "stm32f429")
    target_link_libraries(lwip_stack PRIVATE
        freertos_kernel
        stm32f429_bsp
    )
endif()

# Necessary Definitions
target_compile_definitions(lwip_stack PUBLIC
    NO_SYS=0
    USE_FREERTOS=1
)

cmake_minimum_required(VERSION 3.16)
project(lwip_stack C)

SET(LWIP_PORT_SRC "")
SET(LWIP_PORT_INC "")

# Define Paths for the cloned repositories
set(LWIP_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lwip-core")      # The cloned core
set(LWIP_CONTRIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lwip-contrib") # The cloned contrib

# Create the Library
add_library(lwip_stack STATIC)

# include port subdir 
add_subdirectory(port)

# Include Directories
target_include_directories(lwip_stack PUBLIC
    "${LWIP_PORT_INC}"
)

# LwIP Core Source Files (from the cloned repo)
target_sources(lwip_stack PRIVATE
    "${LWIP_CORE_DIR}/src/core/init.c"
    "${LWIP_CORE_DIR}/src/core/def.c"
    "${LWIP_CORE_DIR}/src/core/dns.c"
    "${LWIP_CORE_DIR}/src/core/inet_chksum.c"
    "${LWIP_CORE_DIR}/src/core/ip.c"
    "${LWIP_CORE_DIR}/src/core/mem.c"
    "${LWIP_CORE_DIR}/src/core/memp.c"
    "${LWIP_CORE_DIR}/src/core/netif.c"
    "${LWIP_CORE_DIR}/src/core/pbuf.c"
    "${LWIP_CORE_DIR}/src/core/raw.c"
    "${LWIP_CORE_DIR}/src/core/stats.c"
    "${LWIP_CORE_DIR}/src/core/sys.c"
    "${LWIP_CORE_DIR}/src/core/tcp.c"
    "${LWIP_CORE_DIR}/src/core/tcp_in.c"
    "${LWIP_CORE_DIR}/src/core/tcp_out.c"
    "${LWIP_CORE_DIR}/src/core/timeouts.c"
    "${LWIP_CORE_DIR}/src/core/udp.c"
    "${LWIP_CORE_DIR}/src/core/ipv4/etharp.c"
    "${LWIP_CORE_DIR}/src/core/ipv4/icmp.c"
    "${LWIP_CORE_DIR}/src/core/ipv4/ip4.c"
    "${LWIP_CORE_DIR}/src/core/ipv4/ip4_addr.c"
    "${LWIP_CORE_DIR}/src/core/ipv4/ip4_frag.c"
    "${LWIP_CORE_DIR}/src/api/tcpip.c"
    "${LWIP_CORE_DIR}/src/api/api_lib.c"
    "${LWIP_CORE_DIR}/src/api/api_msg.c"
    "${LWIP_CORE_DIR}/src/api/err.c"
    "${LWIP_CORE_DIR}/src/api/netbuf.c"
    "${LWIP_CORE_DIR}/src/api/sockets.c"
    "${LWIP_CORE_DIR}/src/api/netdb.c"
    "${LWIP_PORT_SRC}"
)

# Link to your FreeRTOS target and the CMSIS-Drivers (for Ethernet MAC/PHY)
target_link_libraries(lwip_stack PRIVATE 
    FreeRTOS_Kernel 
    cmsis
    bsp_interface
)

# Necessary Definitions
target_compile_definitions(lwip_stack PUBLIC 
    NO_SYS=0 
    USE_FREERTOS=1
)
cmake_minimum_required(VERSION 3.16)
project(lwip_stack C)

# 1. Define Paths for the cloned repositories
set(LWIP_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lwip-core")      # The cloned core
set(LWIP_CONTRIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lwip-contrib") # The cloned contrib

# 2. Create the Library
add_library(lwip_stack STATIC)

# 3. Include Directories
target_include_directories(lwip_stack PUBLIC
    "inc"                                 # Your custom lwipopts.h and headers
    "${LWIP_CORE_DIR}/src/include"        # LwIP Core Headers
    "${LWIP_CONTRIB_DIR}/ports/freertos/include" # FreeRTOS Port Headers
    "${CMAKE_CURRENT_SOURCE_DIR}/lwip-contrib/ports/freertos/include"
)

# 4. Your Custom Port Source (The CMSIS-Driver bridge)
target_sources(lwip_stack PRIVATE
    "src/ethernetif.c"                    # Your bridge between LwIP and CMSIS-Drivers
)

# 5. LwIP Core Source Files (from the cloned repo)
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
)

# 6. The Official FreeRTOS Port (from the cloned contrib)
target_sources(lwip_stack PRIVATE
    "${LWIP_CONTRIB_DIR}/ports/freertos/sys_arch.c"
)

# 7. Dependencies
# Link to your FreeRTOS target and the CMSIS-Drivers (for Ethernet MAC/PHY)
target_link_libraries(lwip_stack PUBLIC 
    FreeRTOS_Kernel 
    # Add your CMSIS-Driver target name here, e.g., CMSIS_ETH_Driver
)

# 8. Necessary Definitions
target_compile_definitions(lwip_stack PUBLIC 
    NO_SYS=0 
    USE_FREERTOS=1
)
cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Get the BSP root directory (this file is in cmake/stm32cubemx/, so go up 2 levels)
get_filename_component(BSP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# STM32CubeMX generated symbols (macros)
set(MX_Defines_Syms
	USE_HAL_DRIVER
	STM32F429xx
    $<$<CONFIG:Debug>:DEBUG>
)

# STM32CubeMX generated include paths
# Note: compat/posix and compat/stdc are excluded - they shadow system headers like errno.h
# Note: BSP's system/arch is excluded - it uses CMSIS-RTOS, we use FreeRTOS directly via stm32h5 port
set(MX_Include_Dirs
    ${BSP_ROOT}/LWIP/App
    ${BSP_ROOT}/LWIP/Target
    # LwIP arch headers (cc.h, sys_arch.h) from stm32h5 port - works with FreeRTOS directly
    # Note: This directory also has ethernetif.h but BSP's LWIP/Target comes first in include order
    ${CMAKE_SOURCE_DIR}/application/dependencies/lwip/port/stm32h5
    ${BSP_ROOT}/USB_HOST/App
    ${BSP_ROOT}/USB_HOST/Target
    ${BSP_ROOT}/Core/Inc
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/include
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${BSP_ROOT}/Drivers/BSP/Components/lan8742
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/include/netif/ppp
    ${BSP_ROOT}/Middlewares/ST/STM32_USB_Host_Library/Core/Inc
    ${BSP_ROOT}/Middlewares/ST/STM32_USB_Host_Library/Class/MSC/Inc
    ${BSP_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/include/lwip
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/include/lwip/apps
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/include/lwip/priv
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/include/lwip/prot
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/include/netif
    ${BSP_ROOT}/Drivers/CMSIS/Include
    # FreeRTOS includes (required by lwipopts.h which includes FreeRTOS.h)
    ${CMAKE_SOURCE_DIR}/application/dependencies/FreeRTOS-Kernel/include
    ${CMAKE_SOURCE_DIR}/application/dependencies/FreeRTOS-Kernel/portable/GCC/ARM_CM4F
    ${CMAKE_SOURCE_DIR}/application/inc
)

# STM32CubeMX generated application sources
# Note: main.c is excluded - application provides its own main
set(MX_Application_Src
    ${BSP_ROOT}/USB_HOST/Target/usbh_conf.c
    ${BSP_ROOT}/USB_HOST/Target/usbh_platform.c
    ${BSP_ROOT}/USB_HOST/App/usb_host.c
    ${BSP_ROOT}/LWIP/Target/ethernetif.c
    ${BSP_ROOT}/LWIP/App/lwip.c
    ${BSP_ROOT}/Core/Src/stm32f4xx_it.c
    ${BSP_ROOT}/Core/Src/stm32f4xx_hal_msp.c
    ${BSP_ROOT}/Core/Src/syscalls.c
    ${BSP_ROOT}/Core/Src/sysmem.c
    ${BSP_ROOT}/startup_stm32f429xx.s
)

# STM32 HAL/LL Drivers
set(STM32_Drivers_Src
    ${BSP_ROOT}/Core/Src/system_stm32f4xx.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_eth.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_hcd.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c
    ${BSP_ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    ${BSP_ROOT}/Drivers/BSP/Components/lan8742/lan8742.c
)

# Drivers Midllewares


set(LwIP_Src
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/auth.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/ccp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/chap_ms.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/chap-md5.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/chap-new.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/demand.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/eap.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/eui64.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/fsm.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/ipcp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/ipv6cp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/lcp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/magic.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/mppe.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/multilink.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/ppp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/pppapi.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/pppcrypt.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/pppoe.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/pppol2tp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/pppos.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/upap.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/utils.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/vj.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/bridgeif.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/bridgeif_fdb.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ethernet.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/lowpan6.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/lowpan6_ble.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/lowpan6_common.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/slipif.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/zepif.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/netif/ppp/ecp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/api/api_lib.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/api/api_msg.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/api/err.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/api/if_api.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/api/netbuf.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/api/netdb.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/api/netifapi.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/api/sockets.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/api/tcpip.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/altcp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/altcp_alloc.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/altcp_tcp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/def.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/dns.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/inet_chksum.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/init.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ip.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/mem.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/memp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/netif.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/pbuf.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/raw.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/stats.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/sys.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/tcp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/tcp_in.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/tcp_out.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/timeouts.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/udp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv4/autoip.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv4/dhcp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv4/etharp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv4/icmp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv4/igmp.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv4/ip4.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv4/ip4_addr.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv4/ip4_frag.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv6/dhcp6.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv6/ethip6.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv6/icmp6.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv6/inet6.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv6/ip6.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv6/ip6_addr.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv6/ip6_frag.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv6/mld6.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/core/ipv6/nd6.c
    ${BSP_ROOT}/Middlewares/Third_Party/LwIP/src/apps/mqtt/mqtt.c
)
set(USB_Host_Library_Src
    ${BSP_ROOT}/Middlewares/ST/STM32_USB_Host_Library/Core/Src/usbh_core.c
    ${BSP_ROOT}/Middlewares/ST/STM32_USB_Host_Library/Core/Src/usbh_ctlreq.c
    ${BSP_ROOT}/Middlewares/ST/STM32_USB_Host_Library/Core/Src/usbh_ioreq.c
    ${BSP_ROOT}/Middlewares/ST/STM32_USB_Host_Library/Core/Src/usbh_pipes.c
    ${BSP_ROOT}/Middlewares/ST/STM32_USB_Host_Library/Class/MSC/Src/usbh_msc.c
    ${BSP_ROOT}/Middlewares/ST/STM32_USB_Host_Library/Class/MSC/Src/usbh_msc_bot.c
    ${BSP_ROOT}/Middlewares/ST/STM32_USB_Host_Library/Class/MSC/Src/usbh_msc_scsi.c
)

# Link directories setup
set(MX_LINK_DIRS

)
# Project static libraries
set(MX_LINK_LIBS
    STM32_Drivers
    ${TOOLCHAIN_LINK_LIBRARIES}
    LwIP
	USB_Host_Library

)
# Interface library for includes and symbols
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

# Create STM32_Drivers static library
add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE ${STM32_Drivers_Src})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)


# Create LwIP static library
add_library(LwIP OBJECT)
target_sources(LwIP PRIVATE ${LwIP_Src})
target_link_libraries(LwIP PUBLIC stm32cubemx)

# Create USB_Host_Library static library
add_library(USB_Host_Library OBJECT)
target_sources(USB_Host_Library PRIVATE ${USB_Host_Library_Src})
target_link_libraries(USB_Host_Library PUBLIC stm32cubemx)

# Add STM32CubeMX generated application sources to the project
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src})

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

# Add libraries to the project
target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ${MX_LINK_LIBS})

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()

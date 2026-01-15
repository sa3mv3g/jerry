include(FetchContent)

FetchContent_Declare(
    FCD_CMSIS_LWIP_PORT
    GIT_REPOSITORY https://github.com/Open-CMSIS-Pack/lwIP.git
    GIT_TAG        v2.3.0
    SOURCE_DIR     "${CMAKE_CURRENT_LIST_DIR}/Open_CMSIS_Pack_Lwip"
    SOURCE_SUBDIR  "EXTRACT_ONLY"
)

FetchContent_MakeAvailable(FCD_CMSIS_LWIP_PORT)

SET(LWIP_PORT_SRC   "${LWIP_PORT_SRC}"
                    "${CMAKE_CURRENT_LIST_DIR}/Open_CMSIS_Pack_Lwip/ports/freertos/sys_arch.c"
                    "${CMAKE_CURRENT_LIST_DIR}/Open_CMSIS_Pack_Lwip/ports/cmsis-driver/netif/ethernetif.c"
                    "${CMAKE_CURRENT_LIST_DIR}/Open_CMSIS_Pack_Lwip/ports/cmsis-driver/sio/sio.c"
)

SET(LWIP_PORT_INC   "${LWIP_PORT_INC}"
                    "${CMAKE_CURRENT_LIST_DIR}/Open_CMSIS_Pack_Lwip/ports/cmsis-driver/netif"
                    "${CMAKE_CURRENT_LIST_DIR}/Open_CMSIS_Pack_Lwip/ports/cmsis-driver/config"
                    "${CMAKE_CURRENT_LIST_DIR}/Open_CMSIS_Pack_Lwip/ports/freertos/include/arch"
                    "${CMAKE_CURRENT_LIST_DIR}/Open_CMSIS_Pack_Lwip/rte/config"
                    "${CMAKE_CURRENT_LIST_DIR}/Open_CMSIS_Pack_Lwip/rte/include"
)
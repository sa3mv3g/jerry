#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

/* ------------------------------------------------
   Fix for sys_arch.c missing FreeRTOS headers
   ------------------------------------------------ */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


/* ------------------------------------------------
   2. Operating System & Core
   ------------------------------------------------ */
#define NO_SYS                          0  /* We are using FreeRTOS */
#define SYS_LIGHTWEIGHT_PROT            1
#define LWIP_NETCONN                    1  /* Required for many apps */
#define LWIP_SOCKET                     1  /* Required for POSIX sockets */
/* Stop LwIP from defining struct timeval, as <sys/time.h> already does it */
#define LWIP_TIMEVAL_PRIVATE            0
#define LWIP_ERRNO_STDINCLUDE           1

/* ------------------------------------------------
   3. Memory Options (Tailor to STM32H5 RAM)
   ------------------------------------------------ */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (16 * 1024) /* 16KB Heap */
#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_UDP_PCB                4
#define MEMP_NUM_TCP_PCB                5
#define MEMP_NUM_TCP_PCB_LISTEN         8
#define MEMP_NUM_SYS_TIMEOUT            10

/* ------------------------------------------------
   4. IP Version Support
   ------------------------------------------------ */
#define LWIP_IPV4                       1  /* FORCE IPv4 ON */
#define LWIP_IPV6                       0  /* FORCE IPv6 OFF */

/* ------------------------------------------------
   5. Network Interfaces
   ------------------------------------------------ */
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_ICMP                       1
#define LWIP_RAW                        1
#define LWIP_DHCP                       1
#define LWIP_DNS                        1

/* ------------------------------------------------
   6. Checksum Offload (STM32H5 Hardware)
   ------------------------------------------------ */
/* If your ethernetif.c driver handles checksums, set these to 0.
   If you aren't sure, set to 1 (Software Checksum) to be safe. */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_TCP                1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_TCP              1

/* ------------------------------------------------
   7. FreeRTOS Specifics
   ------------------------------------------------ */
#define TCPIP_THREAD_NAME               "tcpip_thread"
#define TCPIP_THREAD_STACKSIZE          1024
#define TCPIP_THREAD_PRIO               3
#define TCPIP_MBOX_SIZE                 8

#define DEFAULT_THREAD_STACKSIZE        1024
#define DEFAULT_THREAD_PRIO             3
#define DEFAULT_RAW_RECVMBOX_SIZE       8
#define DEFAULT_UDP_RECVMBOX_SIZE       8
#define DEFAULT_TCP_RECVMBOX_SIZE       8
#define DEFAULT_ACCEPTMBOX_SIZE         8

#define LWIP_NETIF_LINK_CALLBACK        1

#define LWIP_STATS                      1
#define LWIP_STATS_DISPLAY              1
#define LINK_STATS                      1

#endif /* LWIP_LWIPOPTS_H */
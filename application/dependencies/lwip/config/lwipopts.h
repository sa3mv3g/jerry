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
#define LWIP_SO_RCVTIMEO                1  /* Enable receive timeout for sockets */
/* Use LWIP_COMPAT_SOCKETS=2 to provide actual functions instead of macros.
   This avoids conflicts with code that uses 'send', 'recv', 'select', etc.
   as variable or function pointer names. */
#define LWIP_COMPAT_SOCKETS             2
/* Stop LwIP from defining struct timeval, as <sys/time.h> already does it */
#define LWIP_TIMEVAL_PRIVATE            0
#define LWIP_ERRNO_STDINCLUDE           1

/* ------------------------------------------------
   3. Memory Options (Tailor to STM32H5 RAM)
   ------------------------------------------------ */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (4 * 1024) /* 4KB Heap - actual usage ~120 bytes max */
#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_UDP_PCB                4
#define MEMP_NUM_TCP_PCB                10
#define MEMP_NUM_TCP_PCB_LISTEN         2   /* Only need 1-2 listening sockets */
#define MEMP_NUM_NETCONN                10  /* Number of netconn structures */
#define MEMP_NUM_SYS_TIMEOUT            10
#define MEMP_NUM_TCP_SEG                32  /* TCP segments for queuing */

/* ------------------------------------------------
   3b. TCP Tuning for Embedded Systems
   ------------------------------------------------ */
#define TCP_MSS                         1460 /* Maximum segment size */
#define TCP_WND                         (2 * TCP_MSS) /* Receive window size - reduced to limit memory */
#define TCP_SND_BUF                     (2 * TCP_MSS) /* Send buffer size - reduced to prevent heap exhaustion */
#define TCP_SND_QUEUELEN                8   /* Number of segments in send queue (must be > TCP_SNDQUEUELOWAT) */
#define TCP_QUEUE_OOSEQ                 0   /* Disable out-of-order segment queuing to save memory */
#define LWIP_TCP_SACK_OUT               0   /* Disable SACK to save memory */

/* Reduce TIME_WAIT duration for faster connection recycling */
#define TCP_MSL                         1000 /* 1 second MSL (default is 60000ms) */

/* Enable send timeout to prevent blocking forever */
#define LWIP_SO_SNDTIMEO                1

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

/* ------------------------------------------------
   8. Statistics (for debugging memory issues)
   ------------------------------------------------ */
#define LWIP_STATS                      1
#define LWIP_STATS_DISPLAY              1
#define LINK_STATS                      1
#define MEM_STATS                       1  /* Heap memory statistics */
#define MEMP_STATS                      1  /* Memory pool statistics */
#define SYS_STATS                       1  /* System statistics */
#define TCP_STATS                       1  /* TCP statistics */
#define IP_STATS                        1  /* IP statistics */

#endif /* LWIP_LWIPOPTS_H */

#ifndef IP_SPECS_H
#define IP_SPECS_H

/* Set ENABLE_DHCP to 1 for dynamic IP (DHCP), 0 for static IP                  */
/* ENABLE_DHCP is now injected via CMake target_compile_definitions             */

#if !CMAKE_ENABLE_DHCP
#define STATIC_IP_ADDR0      192
#define STATIC_IP_ADDR1      168
#define STATIC_IP_ADDR2      0
#define STATIC_IP_ADDR3_BASE 200 /* Base value, DEVADDR (0-15) is added */

#define STATIC_NETMASK0 255
#define STATIC_NETMASK1 255
#define STATIC_NETMASK2 255
#define STATIC_NETMASK3 0

#define STATIC_GW_ADDR0 192
#define STATIC_GW_ADDR1 168
#define STATIC_GW_ADDR2 0
#define STATIC_GW_ADDR3 1
#endif /* !USE_DHCP */

#define HOST_IP_ADDR0 192
#define HOST_IP_ADDR1 168
#define HOST_IP_ADDR2 0
#define HOST_IP_ADDR3 100

#define SNTP_IP_ADDR0 HOST_IP_ADDR0
#define SNTP_IP_ADDR1 HOST_IP_ADDR1
#define SNTP_IP_ADDR2 HOST_IP_ADDR2
#define SNTP_IP_ADDR3 HOST_IP_ADDR3

#define SYSLOG_SERVER_IP_ADDR0 HOST_IP_ADDR0
#define SYSLOG_SERVER_IP_ADDR1 HOST_IP_ADDR1
#define SYSLOG_SERVER_IP_ADDR2 HOST_IP_ADDR2
#define SYSLOG_SERVER_IP_ADDR3 HOST_IP_ADDR3

#endif  // IP_SPECS_H
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// Operating system integration - NO_SYS means no operating system
#define NO_SYS                  1
#define LWIP_NETCONN            0   // Disable Netconn API (requires OS)
#define LWIP_SOCKET             0   // Disable Socket API (requires OS)

// System architecture - use the Pico SDK's lwIP port
#define SYS_LIGHTWEIGHT_PROT    1   // Enable lightweight protection

// For NO_SYS mode, define sys_prot_t if not already defined
#ifndef sys_prot_t
typedef int sys_prot_t;
#endif

// Basic lwIP configuration for Pico W
#define LWIP_IPV4 1
#define LWIP_IPV6 0

// Memory management
#define MEM_SIZE (16*1024)
#define MEMP_NUM_PBUF 16
#define MEMP_NUM_UDP_PCB 8
#define MEMP_NUM_TCP_PCB 0      // Disable TCP
#define MEMP_NUM_TCP_PCB_LISTEN 0
#define MEMP_NUM_TCP_SEG 0
#define MEMP_NUM_SYS_TIMEOUT 8
#define MEMP_NUM_NETBUF         0   // Not needed for NO_SYS
#define MEMP_NUM_NETCONN        0   // Not needed for NO_SYS  
#define MEMP_NUM_TCPIP_MSG_API  0   // Not needed for NO_SYS
#define MEMP_NUM_TCPIP_MSG_INPKT 0  // Not needed for NO_SYS

// Network interfaces
#define LWIP_NETIF_API 0        // Disable for NO_SYS
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_HOSTNAME 1

// UDP
#define LWIP_UDP 1
#define LWIP_UDPLITE 0

// TCP (not needed for MQTT-SN)
#define LWIP_TCP 0

// ICMP
#define LWIP_ICMP 1
#define ICMP_TTL 255

// DHCP
#define LWIP_DHCP 1

// DNS
#define LWIP_DNS 1

// Statistics
#define LWIP_STATS 0
#define LWIP_STATS_DISPLAY 0

// Debug
#define LWIP_DEBUG 0

// Checksums
#define CHECKSUM_GEN_IP 1
#define CHECKSUM_GEN_UDP 1
#define CHECKSUM_CHECK_IP 1
#define CHECKSUM_CHECK_UDP 1

// Random number generation
#define LWIP_RAND() ((u32_t)rand())

#endif // LWIPOPTS_H

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// System architecture
#define LWIP_ARCH_SYS_ARCH_H "arch/sys_arch.h"

// Basic lwIP configuration for Pico W
#define LWIP_IPV4 1
#define LWIP_IPV6 0

// Memory management
#define MEM_SIZE (1600)
#define MEMP_NUM_PBUF 16
#define MEMP_NUM_UDP_PCB 4
#define MEMP_NUM_TCP_PCB 4
#define MEMP_NUM_TCP_PCB_LISTEN 8
#define MEMP_NUM_TCP_SEG 16
#define MEMP_NUM_SYS_TIMEOUT 8

// Network interfaces
#define LWIP_NETIF_API 1
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_HOSTNAME 1

// UDP
#define LWIP_UDP 1
#define LWIP_UDPLITE 0

// TCP (not needed for MQTT-SN but included for completeness)
#define LWIP_TCP 0

// ICMP
#define LWIP_ICMP 1
#define ICMP_TTL 255

// DHCP
#define LWIP_DHCP 1

// DNS
#define LWIP_DNS 1

// Socket API
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0

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

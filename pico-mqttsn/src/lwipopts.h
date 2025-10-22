#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* No RTOS, use lwIP internal timers/background */
#define NO_SYS                         1

/* Enable BSD sockets API (you use <lwip/sockets.h>) */
#define LWIP_SOCKET                    0
#define LWIP_NETCONN                   0
#define LWIP_TIMEVAL_PRIVATE           0

/* Use libc malloc/free to keep it simple */
#define MEM_LIBC_MALLOC                1
#define MEMP_MEM_MALLOC                1

/* Core protocol toggles */
#define LWIP_IPV4                      1
#define LWIP_IPV6                      0
#define LWIP_TCP                       1
#define LWIP_UDP                       1
#define LWIP_DNS                       1
#define LWIP_DHCP                      1

/* Reasonable TCP tuning for Pico W */
#define TCP_MSS                        1460
#define TCP_WND                        (4 * TCP_MSS)
#define TCP_SND_BUF                    (4 * TCP_MSS)

/* Random function needed by some lwIP features */
#include <stdlib.h>
#define LWIP_RAND()                    ((u32_t)rand())

/* Checksums handled by SDK glue; don’t double-check */
#define CHECKSUM_CHECK_IP              0
#define CHECKSUM_CHECK_UDP             0
#define CHECKSUM_CHECK_TCP             0
#define CHECKSUM_GEN_IP                0
#define CHECKSUM_GEN_UDP               0
#define CHECKSUM_GEN_TCP               0

/* Keep packet debugging quiet by default */
#define LWIP_DEBUG                     0

#endif /* LWIPOPTS_H */

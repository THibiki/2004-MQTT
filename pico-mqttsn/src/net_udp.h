#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "lwip/udp.h"

typedef struct {
  ip_addr_t ip;
  uint16_t  port;   // host order
} udp_endpoint_t;

typedef struct {
  struct udp_pcb *pcb;
  // simple single-buffer receive slot for demo
  volatile int    rx_len;
  uint8_t         rx_buf[1536];
  uint32_t        last_rx_ms;
} udp_socket_t;

bool net_udp_resolve(udp_endpoint_t *ep, const char *ip_str, uint16_t port);
bool net_udp_open(udp_socket_t *s, uint16_t local_port /* 0 = ephemeral */);
bool net_udp_sendto(udp_socket_t *s, const udp_endpoint_t *to, const void *buf, uint16_t len);
/* poll-style receive with timeout (ms); returns length or -1 on timeout */
int  net_udp_recv(udp_socket_t *s, void *buf, uint16_t maxlen, uint32_t timeout_ms);

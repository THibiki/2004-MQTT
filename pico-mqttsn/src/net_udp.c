#include "net_udp.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/inet.h"
#include <string.h>
#include "lwip/ip_addr.h"

static void rx_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                  const ip_addr_t *addr, u16_t port) {
  (void)pcb; (void)addr; (void)port;
  udp_socket_t *s = (udp_socket_t*)arg;
  if (!p) return;
  // flatten into single buffer (demo). For production, handle chains.
  u16_t copy = p->tot_len;
  if (copy > sizeof(s->rx_buf)) copy = sizeof(s->rx_buf);
  pbuf_copy_partial(p, s->rx_buf, copy, 0);
  s->rx_len = (int)copy;
  s->last_rx_ms = to_ms_since_boot(get_absolute_time());
  pbuf_free(p);
}

bool net_udp_resolve(udp_endpoint_t *ep, const char *ip_str, uint16_t port){
  if (!ipaddr_aton(ip_str, &ep->ip)) return false;
  ep->port = port;
  return true;
}

bool net_udp_open(udp_socket_t *s, uint16_t local_port){
  s->pcb = udp_new_ip_type(IPADDR_TYPE_V4);
  if (!s->pcb) return false;
  s->rx_len = 0;
  if (udp_bind(s->pcb, IP_ANY_TYPE, local_port) != ERR_OK) {
    udp_remove(s->pcb);
    s->pcb = NULL;
    return false;
  }
  udp_recv(s->pcb, rx_cb, s);
  return true;
}

bool net_udp_sendto(udp_socket_t *s, const udp_endpoint_t *to, const void *buf, uint16_t len){
  if (!s->pcb) return false;
  struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
  if (!p) return false;
  memcpy(p->payload, buf, len);
  err_t e = udp_sendto(s->pcb, p, &to->ip, to->port);
  pbuf_free(p);
  return e == ERR_OK;
}

int net_udp_recv(udp_socket_t *s, void *buf, uint16_t maxlen, uint32_t timeout_ms){
  uint64_t deadline = to_ms_since_boot(get_absolute_time()) + timeout_ms;
  while (to_ms_since_boot(get_absolute_time()) < deadline) {
    if (s->rx_len > 0) {
      int n = s->rx_len;
      if ((uint16_t)n > maxlen) n = (int)maxlen;
      memcpy(buf, s->rx_buf, n);
      s->rx_len = 0; // consume
      return n;
    }
    cyw43_arch_poll();
    sleep_ms(1);
  }
  return -1; // timeout
}

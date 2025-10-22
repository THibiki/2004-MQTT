#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "net_udp.h"

typedef struct {
  udp_endpoint_t gw;          // gateway ip:port
  udp_socket_t   sock;        // lwIP UDP PCB wrapper
  char           client_id[24];
  uint16_t       keepalive_s;
  uint16_t       next_msg_id;
  // simple one-topic demo cache
  uint16_t       last_topic_id;
} mqttsn_client_t;

bool mqttsn_init(mqttsn_client_t *c, const char *gw_ip, uint16_t gw_port,
                 const char *client_id, uint16_t keepalive_s);

bool mqttsn_connect(mqttsn_client_t *c, bool clean_session);
bool mqttsn_register(mqttsn_client_t *c, const char *topic, uint16_t *out_tid);
bool mqttsn_publish_qos0(mqttsn_client_t *c, uint16_t topic_id,
                         const void *payload, uint16_t len);

void mqttsn_tick(mqttsn_client_t *c, uint32_t ms); // keepalive later

#include <string.h>
#include "mqttsn.h"
#include "mqttsn_client.h"

static uint16_t next_id(mqttsn_client_t *c){ 
  c->next_msg_id = (c->next_msg_id==0xFFFF)?1:(c->next_msg_id+1);
  return c->next_msg_id;
}

bool mqttsn_init(mqttsn_client_t *c, const char *gw_ip, uint16_t gw_port,
                 const char *client_id, uint16_t keepalive_s){
  memset(c, 0, sizeof(*c));
  strncpy(c->client_id, client_id, sizeof(c->client_id)-1);
  c->keepalive_s = keepalive_s;
  if(!net_udp_resolve(&c->gw, gw_ip, gw_port)) return false;
  if(!net_udp_open(&c->sock, 0)) return false;
  return true;
}

static bool send_pkt(mqttsn_client_t *c, const uint8_t *body, uint16_t blen){
  // MQTT-SN short format: 1-byte LEN then BODY (LEN includes length byte)
  uint8_t buf[256];
  if (blen + 1 > sizeof(buf)) return false;
  buf[0] = (uint8_t)(blen + 1);
  memcpy(buf+1, body, blen);
  return net_udp_sendto(&c->sock, &c->gw, buf, blen+1);
}

// Blocking receive with small timeout (demo)
static int recv_pkt(mqttsn_client_t *c, uint8_t *out, uint16_t max, uint32_t timeout_ms){
  return net_udp_recv(&c->sock, out, max, timeout_ms); // returns length or -1
}

bool mqttsn_connect(mqttsn_client_t *c, bool clean_session){
  uint8_t body[64];
  uint16_t i=0;
  body[i++] = MQTTSN_CONNECT;
  body[i++] = clean_session ? 0x02 : 0x00; // Flags (CleanSession bit)
  body[i++] = MQTTSN_PROTO_ID;
  wr16(body+i, c->keepalive_s); i+=2;
  const size_t idlen = strnlen(c->client_id, sizeof(c->client_id));
  memcpy(body+i, c->client_id, idlen); i+= (uint16_t)idlen;

  if(!send_pkt(c, body, i)) return false;

  uint8_t pkt[64];
  int n = recv_pkt(c, pkt, sizeof(pkt), 2000);
  if(n < 3) return false;
  if(pkt[1] != MQTTSN_CONNACK) return false;
  return pkt[2] == MQTTSN_RC_ACCEPTED;
}

bool mqttsn_register(mqttsn_client_t *c, const char *topic, uint16_t *out_tid){
  uint8_t body[128]; uint16_t i=0;
  uint16_t mid = next_id(c);
  body[i++] = MQTTSN_REGISTER;
  body[i++] = 0x00; body[i++] = 0x00;     // TopicId (0 for request)
  wr16(body+i, mid); i+=2;                // MsgId
  const size_t tlen = strlen(topic);
  memcpy(body+i, topic, tlen); i += (uint16_t)tlen;

  if(!send_pkt(c, body, i)) return false;

  uint8_t pkt[128];
  int n = recv_pkt(c, pkt, sizeof(pkt), 2000);
  if(n < 7 || pkt[1] != MQTTSN_REGACK) return false;
  uint16_t tid = be16(&pkt[2]);
  uint16_t rmid = be16(&pkt[4]);
  uint8_t rc = pkt[6];
  if(rc != 0x00 || rmid != mid) return false;
  c->last_topic_id = tid;
  if(out_tid) *out_tid = tid;
  return true;
}

bool mqttsn_publish_qos0(mqttsn_client_t *c, uint16_t topic_id,
                         const void *payload, uint16_t len){
  // Flags: short topic id
  uint8_t head[1+1+2]; // type, flags, topicId
  head[0] = MQTTSN_PUBLISH;
  head[1] = MQTTSN_FLAG_TOPIC_ID; // QoS0 + short topic id
  wr16(&head[2], topic_id);

  // assemble [head]+[payload] into single buffer
  uint8_t buf[256];
  if (len + sizeof(head) + 1 > sizeof(buf)) return false;
  buf[0] = (uint8_t)(len + sizeof(head) + 1);
  memcpy(buf+1, head, sizeof(head));
  memcpy(buf+1+sizeof(head), payload, len);
  return net_udp_sendto(&c->sock, &c->gw, buf, buf[0]);
}

void mqttsn_tick(mqttsn_client_t *c, uint32_t ms){
  (void)c; (void)ms; // (keepalive/QoS timers later)
}

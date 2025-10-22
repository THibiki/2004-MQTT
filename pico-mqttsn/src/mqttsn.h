#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// MQTT-SN types
enum {
  MQTTSN_ADVERTISE=0x00, MQTTSN_SEARCHGW=0x01, MQTTSN_GWINFO=0x02,
  MQTTSN_CONNECT=0x04,   MQTTSN_CONNACK=0x05,
  MQTTSN_REGISTER=0x0A,  MQTTSN_REGACK=0x0B,
  MQTTSN_PUBLISH=0x0C,   MQTTSN_PUBACK=0x0D,
  MQTTSN_PUBREC=0x0F,    MQTTSN_PUBREL=0x10, MQTTSN_PUBCOMP=0x11,
  MQTTSN_SUBSCRIBE=0x12, MQTTSN_SUBACK=0x13,
  MQTTSN_PINGREQ=0x16,   MQTTSN_PINGRESP=0x17,
  MQTTSN_DISCONNECT=0x18
};

#define MQTTSN_PROTO_ID 0x01
#define MQTTSN_RC_ACCEPTED 0x00
#define MQTTSN_FLAG_DUP   0x80
#define MQTTSN_FLAG_QOS1  0x20
#define MQTTSN_FLAG_QOS2  0x40
#define MQTTSN_FLAG_RETAIN 0x10
#define MQTTSN_FLAG_TOPIC_ID 0x02 /* 1 = short topic id */

// Helpers
static inline uint16_t be16(const uint8_t *p){ return (uint16_t)p[0]<<8 | p[1]; }
static inline void wr16(uint8_t *p, uint16_t v){ p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }

/* Minimal MQTT-SN message definitions used by the gateway scaffold.
 * This header intentionally avoids Pico/lwIP-specific includes and
 * contains only protocol constants and packet structs needed by the
 * gateway.
 */
#ifndef GATEWAY_MQTT_SN_H
#define GATEWAY_MQTT_SN_H

#include <stdint.h>

// MQTT-SN Message Types
#define MQTT_SN_CONNECT      0x04
#define MQTT_SN_CONNACK      0x05
#define MQTT_SN_REGISTER     0x0A
#define MQTT_SN_REGACK      0x0B
#define MQTT_SN_PUBLISH      0x0C
#define MQTT_SN_PUBACK       0x0D
#define MQTT_SN_SUBSCRIBE    0x12
#define MQTT_SN_SUBACK       0x13

// QoS Levels
#define MQTT_SN_QOS_0        0x00
#define MQTT_SN_QOS_1        0x01

// Return Codes
#define MQTT_SN_ACCEPTED     0x00

// Use portable packing: MSVC uses pragmas, others use __attribute__((packed))
#if defined(_MSC_VER)
#pragma pack(push,1)
#define MQTT_SN_PACKED
#else
#define MQTT_SN_PACKED __attribute__((packed))
#endif

// Basic packet structures (packed as on-wire formats)
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint8_t flags;
    /* variable content follows depending on msg_type */
} MQTT_SN_PACKED mqtt_sn_generic_t;

// REGISTER (client -> gateway): length, msg_type, topic_id(2), msg_id(2), topic_name[]
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint16_t topic_id;
    uint16_t msg_id;
    /* topic_name follows */
} MQTT_SN_PACKED mqtt_sn_register_t;

// REGACK (gateway -> client): length, msg_type, topic_id(2), msg_id(2), return_code
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint16_t topic_id;
    uint16_t msg_id;
    uint8_t return_code;
} MQTT_SN_PACKED mqtt_sn_regack_t;

// CONNECT: length, msg_type, flags, protocol_id, duration, client_id[]
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint8_t flags;
    uint8_t protocol_id;
    uint16_t duration;
    /* client_id follows */
} MQTT_SN_PACKED mqtt_sn_connect_t;

// CONNACK: length, msg_type, return_code
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint8_t return_code;
} MQTT_SN_PACKED mqtt_sn_connack_t;

// PUBLISH (common fields) : length, msg_type, flags, topic_id(2), [msg_id(2) if qos>0], data[]
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint8_t flags;
    uint16_t topic_id;
    /* optional msg_id and payload follow */
} MQTT_SN_PACKED mqtt_sn_publish_hdr_t;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#endif // GATEWAY_MQTT_SN_H

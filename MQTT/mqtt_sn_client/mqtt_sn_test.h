#ifndef MQTT_SN_TEST_H
#define MQTT_SN_TEST_H

#include "pico/stdlib.h"

// MQTT-SN Protocol Constants
#define MQTT_SN_PROTOCOL_ID 0x01
#define MQTT_SN_VERSION 0x01

// MQTT-SN Message Types
#define MQTT_SN_ADVERTISE    0x00
#define MQTT_SN_SEARCHGW     0x01
#define MQTT_SN_GWINFO       0x02
#define MQTT_SN_CONNECT      0x04
#define MQTT_SN_CONNACK      0x05
#define MQTT_SN_WILLTOPICREQ 0x06
#define MQTT_SN_WILLTOPIC    0x07
#define MQTT_SN_WILLMSGREQ   0x08
#define MQTT_SN_WILLMSG      0x09
#define MQTT_SN_REGISTER     0x0A
#define MQTT_SN_REGACK       0x0B
#define MQTT_SN_PUBLISH      0x0C
#define MQTT_SN_PUBACK       0x0D
#define MQTT_SN_PUBCOMP      0x0E
#define MQTT_SN_PUBREC       0x0F
#define MQTT_SN_PUBREL       0x10
#define MQTT_SN_SUBSCRIBE    0x12
#define MQTT_SN_SUBACK       0x13
#define MQTT_SN_UNSUBSCRIBE  0x14
#define MQTT_SN_UNSUBACK     0x15
#define MQTT_SN_PINGREQ      0x16
#define MQTT_SN_PINGRESP     0x17
#define MQTT_SN_DISCONNECT   0x18
#define MQTT_SN_WILLTOPICUPD 0x1A
#define MQTT_SN_WILLTOPICRESP 0x1B
#define MQTT_SN_WILLMSGUPD   0x1C
#define MQTT_SN_WILLMSGRESP  0x1D

// QoS Levels
#define MQTT_SN_QOS_0        0x00
#define MQTT_SN_QOS_1        0x01
#define MQTT_SN_QOS_2        0x02
#define MQTT_SN_QOS_M1       0x03  // -1 (fire and forget)

// Topic ID Types
#define MQTT_SN_TOPIC_NORMAL 0x00
#define MQTT_SN_TOPIC_PREDEF 0x01
#define MQTT_SN_TOPIC_SHORT  0x02

// Return Codes
#define MQTT_SN_ACCEPTED     0x00
#define MQTT_SN_REJECTED_CONGESTION 0x01
#define MQTT_SN_REJECTED_INVALID_TOPIC_ID 0x02
#define MQTT_SN_REJECTED_NOT_SUPPORTED 0x03

// Client Configuration
#define MQTT_SN_CLIENT_ID_MAX_LEN 23
#define MQTT_SN_TOPIC_NAME_MAX_LEN 64
#define MQTT_SN_MESSAGE_MAX_LEN 255
#define MQTT_SN_DEFAULT_KEEPALIVE 60
#define MQTT_SN_DEFAULT_PORT 1883

// MQTT-SN Message Structure
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint8_t data[];
} __attribute__((packed)) mqtt_sn_message_t;

// CONNECT Message Structure
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint8_t flags;
    uint8_t protocol_id;
    uint16_t duration;
    char client_id[];
} __attribute__((packed)) mqtt_sn_connect_t;

// CONNACK Message Structure
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint8_t return_code;
} __attribute__((packed)) mqtt_sn_connack_t;

// REGISTER Message Structure
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint16_t topic_id;
    uint16_t msg_id;
    char topic_name[];
} __attribute__((packed)) mqtt_sn_register_t;

// REGACK Message Structure
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint16_t topic_id;
    uint16_t msg_id;
    uint8_t return_code;
} __attribute__((packed)) mqtt_sn_regack_t;

// PUBLISH Message Structure
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint8_t flags;
    uint16_t topic_id;
    uint16_t msg_id;
    uint8_t data[];
} __attribute__((packed)) mqtt_sn_publish_t;

// PUBACK Message Structure
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint16_t topic_id;
    uint16_t msg_id;
    uint8_t return_code;
} __attribute__((packed)) mqtt_sn_puback_t;

// SUBSCRIBE Message Structure
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint8_t flags;
    uint16_t msg_id;
    union {
        uint16_t topic_id;
        char topic_name[];
    };
} __attribute__((packed)) mqtt_sn_subscribe_t;

// SUBACK Message Structure
typedef struct {
    uint8_t length;
    uint8_t msg_type;
    uint8_t flags;
    uint16_t topic_id;
    uint16_t msg_id;
    uint8_t return_code;
} __attribute__((packed)) mqtt_sn_suback_t;

// Client State
typedef enum {
    MQTT_SN_STATE_DISCONNECTED,
    MQTT_SN_STATE_CONNECTING,
    MQTT_SN_STATE_CONNECTED,
    MQTT_SN_STATE_REGISTERING,
    MQTT_SN_STATE_READY
} mqtt_sn_state_t;

// MQTT-SN Client Structure (simplified for testing)
typedef struct {
    // Client Info
    char client_id[MQTT_SN_CLIENT_ID_MAX_LEN + 1];
    uint16_t keepalive;
    mqtt_sn_state_t state;
    
    // Message IDs
    uint16_t next_msg_id;
    
    // Topic Management
    uint16_t next_topic_id;
} mqtt_sn_client_t;

// Function Declarations (message construction only for testing)
int mqtt_sn_build_connect(mqtt_sn_client_t *client, uint8_t *buffer, uint16_t buffer_len);
int mqtt_sn_build_register(mqtt_sn_client_t *client, const char *topic_name, 
                          uint8_t *buffer, uint16_t buffer_len);
int mqtt_sn_build_publish(mqtt_sn_client_t *client, uint16_t topic_id, 
                         const uint8_t *data, uint16_t data_len, uint8_t qos,
                         uint8_t *buffer, uint16_t buffer_len);
int mqtt_sn_build_subscribe(mqtt_sn_client_t *client, const char *topic_name, 
                           uint8_t qos, uint8_t *buffer, uint16_t buffer_len);

// Message parsing functions
int mqtt_sn_parse_message(const uint8_t *data, uint16_t data_len, mqtt_sn_message_t *msg);

#endif // MQTT_SN_TEST_H

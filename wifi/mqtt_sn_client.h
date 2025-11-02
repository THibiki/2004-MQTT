#ifndef MQTT_SN_CLIENT_H
#define MQTT_SN_CLIENT_H

#include "pico/stdlib.h"

// MQTT-SN return codes
typedef enum {
    MQTTSN_OK = 0,
    MQTTSN_ERROR = -1,
    MQTTSN_TIMEOUT = -2,
    MQTTSN_NOT_CONNECTED = -3
} mqttsn_result_t;

// MQTT-SN QoS levels
typedef enum {
    QOS_0 = 0,  // At most once
    QOS_1 = 1,  // At least once
    QOS_2 = 2   // Exactly once
} mqttsn_qos_t;

// MQTT-SN functions
int mqttsn_init(const char *gateway_host, uint16_t gateway_port);
int mqttsn_connect(const char *client_id, uint16_t keep_alive);
int mqttsn_disconnect(void);
int mqttsn_subscribe(const char *topic, mqttsn_qos_t qos);
int mqttsn_publish(const char *topic, const uint8_t *data, size_t len, mqttsn_qos_t qos);
int mqttsn_poll(void);
bool mqttsn_is_connected(void);

// Message callback type
typedef void (*mqttsn_message_callback_t)(const char *topic, const uint8_t *data, size_t len);

// Set message callback
void mqttsn_set_message_callback(mqttsn_message_callback_t callback);

#endif // MQTT_SN_CLIENT_H
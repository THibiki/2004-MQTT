#ifndef MQTT_SN_CLIENT_H
#define MQTT_SN_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> 

// Return codes
#define MQTTSN_OK 0
#define MQTTSN_ERROR -1
#define MQTTSN_TIMEOUT -2

// Initialize MQTT-SN client
int mqttsn_init(const char *gateway_ip, uint16_t gateway_port);

// Connect to MQTT-SN gateway
int mqttsn_connect(const char *client_id, uint16_t keep_alive_sec);

// Publish to a topic (QoS 0, 1, or 2)
int mqttsn_publish(const char *topic, const uint8_t *payload, size_t len, uint8_t qos);

// Subscribe to a topic
int mqttsn_subscribe(const char *topic, uint8_t qos);

// Process incoming packets (call this regularly in your main loop)
void mqttsn_poll(void);

// Disconnect from gateway
void mqttsn_disconnect(void);

// Check if connected
bool mqttsn_is_connected(void);

#endif // MQTT_SN_CLIENT_H

#ifndef GATEWAY_MQTT_PAHO_H
#define GATEWAY_MQTT_PAHO_H

#include <stdint.h>

// Initialize Paho MQTT client and connect to broker
int mqtt_paho_init(const char *broker_uri, const char *client_id);

// Clean up client
void mqtt_paho_cleanup(void);

// Publish a message to the broker (blocking)
int mqtt_paho_publish(const char *topic, const void *payload, int len, int qos);

// Subscribe to a topic; the message callback provided to the wrapper will be invoked
int mqtt_paho_subscribe(const char *topic, int qos);

// Application can set a callback invoked when an MQTT message arrives:
// void cb(const char *topic, const void *payload, int len, void *user_ctx)
void mqtt_paho_set_message_callback(void (*cb)(const char*, const void*, int, void*), void *user_ctx);

#endif // GATEWAY_MQTT_PAHO_H

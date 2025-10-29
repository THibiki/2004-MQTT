#include "mqtt_paho.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include "MQTTClient.h"
#else
#include "MQTTClient.h"
#endif

static MQTTClient g_client = NULL;
static void (*g_msg_cb)(const char*, const void*, int, void*) = NULL;
static void *g_msg_ctx = NULL;

// Paho message arrived callback
static int message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    if (g_msg_cb) {
        g_msg_cb(topicName, message->payload, message->payloadlen, g_msg_ctx);
    }
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

int mqtt_paho_init(const char *broker_uri, const char *client_id) {
    int rc;
    if (!broker_uri || !client_id) return -1;

    rc = MQTTClient_create(&g_client, broker_uri, client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "MQTTClient_create failed: %d\n", rc);
        return -1;
    }

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    rc = MQTTClient_connect(g_client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "MQTTClient_connect failed: %d\n", rc);
        MQTTClient_destroy(&g_client);
        g_client = NULL;
        return -1;
    }

    MQTTClient_setCallbacks(g_client, NULL, NULL, message_arrived, NULL);
    return 0;
}

void mqtt_paho_cleanup(void) {
    if (g_client) {
        MQTTClient_disconnect(g_client, 1000);
        MQTTClient_destroy(&g_client);
        g_client = NULL;
    }
}

int mqtt_paho_publish(const char *topic, const void *payload, int len, int qos) {
    if (!g_client || !topic) return -1;
    int rc = MQTTClient_publish(g_client, topic, len, (void*)payload, qos, 0, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "MQTTClient_publish failed: %d\n", rc);
        return -1;
    }
    return 0;
}

int mqtt_paho_subscribe(const char *topic, int qos) {
    if (!g_client || !topic) return -1;
    int rc = MQTTClient_subscribe(g_client, topic, qos);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "MQTTClient_subscribe failed: %d\n", rc);
        return -1;
    }
    return 0;
}

void mqtt_paho_set_message_callback(void (*cb)(const char*, const void*, int, void*), void *user_ctx) {
    g_msg_cb = cb;
    g_msg_ctx = user_ctx;
}

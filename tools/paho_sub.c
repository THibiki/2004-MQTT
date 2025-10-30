#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"

#define ADDRESS     "tcp://localhost:1883"
#define CLIENTID    "PahoSub"
#define TOPIC       "test"
#define QOS         1
#define TIMEOUT     10000L

int main(int argc, char* argv[]) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

    if (MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to create MQTT client\n");
        return 1;
    }

    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to connect to %s\n", ADDRESS);
        MQTTClient_destroy(&client);
        return 1;
    }

    if (MQTTClient_subscribe(client, TOPIC, QOS) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to subscribe to topic %s\n", TOPIC);
    }

    printf("Subscribed to topic '%s'. Waiting for messages...\n", TOPIC);

    while (1) {
        char* topicName = NULL;
        MQTTClient_message* msg = NULL;
        int rc = MQTTClient_receive(client, &topicName, NULL, &msg, TIMEOUT);
        if (rc == MQTTCLIENT_SUCCESS && msg != NULL) {
            printf("Received on %s: %.*s\n", topicName, msg->payloadlen, (char*)msg->payload);
            MQTTClient_freeMessage(&msg);
            MQTTClient_free(topicName);
        }
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return 0;
}

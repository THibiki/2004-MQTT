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
    int rc;

    if ((rc = MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to create client, return code %d\n", rc);
        return 1;
    }

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to connect to %s, return code %d\n", ADDRESS, rc);
        MQTTClient_destroy(&client);
        return 1;
    }

    if ((rc = MQTTClient_subscribe(client, TOPIC, QOS)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to subscribe to topic '%s', return code %d\n", TOPIC, rc);
        MQTTClient_disconnect(client, 10000);
        MQTTClient_destroy(&client);
        return 1;
    }

    printf("Subscribed to topic '%s'. Waiting for messages...\n", TOPIC);

    while (1) {
        MQTTClient_message* msg = NULL;
        char* topicName = NULL;
        int rc = MQTTClient_receive(client, &topicName, NULL, &msg, TIMEOUT);
        if (rc == MQTTCLIENT_SUCCESS && msg != NULL) {
            printf("Received on %s: %.*s\n", topicName, msg->payloadlen, (char*)msg->payload);
            MQTTClient_freeMessage(&msg);
            MQTTClient_free(topicName);
        } else if (rc == MQTTCLIENT_TOPICNAME_TRUNCATED) {
            printf("Topic name truncated\n");
        }
        // Continue waiting for messages
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return 0;
}
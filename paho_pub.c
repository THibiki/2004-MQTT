#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "MQTTClient.h"

#define ADDRESS     "tcp://localhost:1883"
#define CLIENTID    "PahoPub"
#define TOPIC       "test"
#define QOS         1
#define TIMEOUT     10000L

int main(int argc, char* argv[]) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    const char *payload = (argc > 1) ? argv[1] : "Hello from Paho C";
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

    pubmsg.payload = (void*)payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    if ((rc = MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to publish message, return code %d\n", rc);
    } else {
        printf("Published message, waiting for delivery token %d...\n", token);
        MQTTClient_waitForCompletion(client, token, TIMEOUT);
        printf("Message delivered to topic '%s': %s\n", TOPIC, payload);
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return 0;
}
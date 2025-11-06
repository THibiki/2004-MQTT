// mqttsn_client_example.c
// Lightweight MQTT-SN client example harness that uses mqttsn_adapter.
// This file does not require the Paho library to compile. If Paho is
// detected at build time (CMake defines HAVE_PAHO), the example will
// include the Paho headers and show where to call them.

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "mqttsn_adapter.h"

#ifdef HAVE_PAHO
#include "MQTTSNPacket.h"
#include "MQTTSNConnect.h"
#include "MQTTSNPublish.h"
#include "MQTTSNSubscribe.h"
#include "MQTTSNSearch.h"
#endif

// Configure these for your gateway (edit as needed)
#ifndef MQTTSN_GATEWAY_IP
#define MQTTSN_GATEWAY_IP "192.168.4.1"
#endif
#ifndef MQTTSN_GATEWAY_PORT
#define MQTTSN_GATEWAY_PORT 1885
#endif

static bool mqttsn_initialized = false;

int mqttsn_demo_init(uint16_t local_port){
    int rc = mqttsn_transport_open(local_port);
    if (rc != 0){
        printf("[MQTTSN] Failed to open transport (err=%d)\n", rc);
        return rc;
    }

#ifdef HAVE_PAHO
    // Build CONNECT packet using Paho helpers
    unsigned char buf[256];
    MQTTSNPacket_connectData options = MQTTSNPacket_connectData_initializer;
    options.clientID.cstring = "pico_w_client";
    options.duration = 30; // keepalive
    options.cleansession = 1;

    int len = MQTTSNSerialize_connect(buf, sizeof(buf), &options);
    if (len <= 0) {
        printf("[MQTTSN] Failed to serialize CONNECT (rc=%d)\n", len);
    } else {
        int s = mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, buf, len);
        if (s != 0) {
            printf("[MQTTSN] CONNECT send failed (err=%d)\n", s);
        } else {
            // Wait for CONNACK
            int r = mqttsn_transport_receive(buf, sizeof(buf), 5000);
            if (r > 0) {
                int connack_rc = -1;
                int d = MQTTSNDeserialize_connack(&connack_rc, buf, r);
                if (d == 1) {
                    if (connack_rc == MQTTSN_RC_ACCEPTED) {
                        printf("[MQTTSN] CONNECT accepted\n");
                    } else {
                        printf("[MQTTSN] CONNECT rejected (code=%d)\n", connack_rc);
                    }
                } else {
                    printf("[MQTTSN] Failed to parse CONNACK\n");
                }
            } else {
                printf("[MQTTSN] CONNACK not received (rc=%d)\n", r);
            }
        }
    }
#else
    printf("[MQTTSN] Paho not available at build time; example will send a demo datagram instead.\n");
#endif

    mqttsn_initialized = true;
    return 0;
}

// Send a small test payload to the gateway and print timing/result.
int mqttsn_demo_send_test(const char *payload){
    if (!mqttsn_initialized){
        printf("[MQTTSN] Not initialized\n");
        return -1;
    }

    size_t len = strlen(payload);
    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    int rc = mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, (const uint8_t*)payload, len);
    uint32_t t1 = to_ms_since_boot(get_absolute_time());

    if (rc == 0){
        printf("[MQTTSN] Sent %zu bytes to %s:%d (send_ms=%lums)\n", len, MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, (unsigned long)(t1 - t0));
        return 0;
    } else {
        printf("[MQTTSN] Send failed (err=%d)\n", rc);
        return rc;
    }
}

// Blocking receive wrapper to demonstrate receive usage (returns bytes or negative error)
int mqttsn_demo_receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms){
    if (!mqttsn_initialized) return -1;
    return mqttsn_transport_receive(buffer, max_len, timeout_ms);
}

#ifdef HAVE_PAHO
// Subscribe to a topic name. Returns topic id (>0) on success, or negative on error.
int mqttsn_demo_subscribe(const char *topicname, unsigned short packetid, unsigned short *out_topicid){
    if (!mqttsn_initialized) return -1;
    unsigned char buf[256];
    MQTTSN_topicid topic;
    topic.type = MQTTSN_TOPIC_TYPE_NORMAL;
    topic.data.long_.name = (char*)topicname;
    topic.data.long_.len = (int)strlen(topicname);

    int len = MQTTSNSerialize_subscribe(buf, sizeof(buf), 0, 0, packetid, &topic);
    if (len <= 0) return -2;
    int s = mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, buf, len);
    if (s != 0) return -3;

    // Wait for SUBACK
    int r = mqttsn_transport_receive(buf, sizeof(buf), 5000);
    if (r <= 0) return -4;

    int qos = -1;
    unsigned short topicid = 0;
    unsigned short rid = 0;
    unsigned char returncode = 0;
    int d = MQTTSNDeserialize_suback(&qos, &topicid, &rid, &returncode, buf, r);
    if (d != 1) return -5;
    if (returncode != 0) {
        // non-zero means rejected
        return -6;
    }
    if (out_topicid) *out_topicid = topicid;
    printf("[MQTTSN] SUBACK received topicid=%u qos=%d\n", topicid, qos);
    return (int)topicid;
}

// Publish payload to a topic name (uses long topic name). qos and packetid ignored for QoS0.
int mqttsn_demo_publish_name(const char *topicname, const uint8_t *payload, int payloadlen){
    if (!mqttsn_initialized) return -1;
    unsigned char buf[512];
    MQTTSN_topicid topic;
    topic.type = MQTTSN_TOPIC_TYPE_NORMAL;
    topic.data.long_.name = (char*)topicname;
    topic.data.long_.len = (int)strlen(topicname);

    int len = MQTTSNSerialize_publish(buf, sizeof(buf), 0, 0, 0, 0, topic, (unsigned char*)payload, payloadlen);
    if (len <= 0) return -2;
    int s = mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, buf, len);
    if (s != 0) return -3;
    return 0;
}
#else
// Fallbacks when Paho isn't available
int mqttsn_demo_subscribe(const char *topicname, unsigned short packetid, unsigned short *out_topicid){
    (void)topicname; (void)packetid; (void)out_topicid;
    printf("[MQTTSN] subscribe: Paho not present\n");
    return -1;
}

int mqttsn_demo_publish_name(const char *topicname, const uint8_t *payload, int payloadlen){
    (void)topicname;
    return mqttsn_demo_send_test((const char*)payload);
}
#endif

// Process a single incoming packet (blocking up to timeout_ms). If a PUBLISH
// is received, print topic and payload. Returns number of bytes processed or
// 0 on timeout, negative on error.
int mqttsn_demo_process_once(uint32_t timeout_ms){
    if (!mqttsn_initialized) return -1;
    unsigned char buf[512];
    int r = mqttsn_transport_receive(buf, sizeof(buf), timeout_ms);
    if (r <= 0) return r; // 0=no data, negative=error

#ifdef HAVE_PAHO
    unsigned char dup;
    int qos;
    unsigned char retained;
    unsigned short packetid;
    MQTTSN_topicid topic;
    unsigned char *payload = NULL;
    int payloadlen = 0;
    int d = MQTTSNDeserialize_publish(&dup, &qos, &retained, &packetid, &topic, &payload, &payloadlen, buf, r);
    if (d == 1){
        // topic may be long name
        if (topic.type == MQTTSN_TOPIC_TYPE_NORMAL){
            printf("[MQTTSN] PUBLISH topic=%.*s payload=%.*s\n", topic.data.long_.len, topic.data.long_.name, payloadlen, payload);
        } else {
            printf("[MQTTSN] PUBLISH topicid=%u payload=%.*s\n", topic.data.id, payloadlen, payload);
        }
    } else {
        printf("[MQTTSN] Received non-PUBLISH or failed to parse (len=%d)\n", r);
    }
#else
    // Without Paho, just print raw bytes received
    printf("[MQTTSN] Raw packet received (%d bytes)\n", r);
#endif
    return r;
}

void mqttsn_demo_close(void){
    if (mqttsn_initialized){
        mqttsn_transport_close();
        mqttsn_initialized = false;
    }
}

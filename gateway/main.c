#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "mqtt_sn.h"
#include "mqtt_paho.h"

#define DEFAULT_UDP_PORT 1884
#define MAX_PACKET_SIZE 1024
#define MAX_TOPICS 256

typedef struct {
    uint16_t topic_id;
    char topic_name[128];
    int in_use;
} topic_entry_t;

static topic_entry_t g_topics[MAX_TOPICS];
static uint16_t g_next_topic_id = 1;
static int g_sock = -1;
static struct sockaddr_in g_last_client_addr;
static socklen_t g_last_client_addrlen = 0;

static void add_topic_mapping(uint16_t topic_id, const char *name) {
    for (int i = 0; i < MAX_TOPICS; i++) {
        if (!g_topics[i].in_use) {
            g_topics[i].in_use = 1;
            g_topics[i].topic_id = topic_id;
            strncpy(g_topics[i].topic_name, name, sizeof(g_topics[i].topic_name)-1);
            g_topics[i].topic_name[sizeof(g_topics[i].topic_name)-1] = '\0';
            return;
        }
    }
}

static const char* find_topic_name(uint16_t topic_id) {
    for (int i = 0; i < MAX_TOPICS; i++) {
        if (g_topics[i].in_use && g_topics[i].topic_id == topic_id) return g_topics[i].topic_name;
    }
    return NULL;
}

static int find_topic_id_by_name(const char *name, uint16_t *out_id) {
    for (int i = 0; i < MAX_TOPICS; i++) {
        if (g_topics[i].in_use && strcmp(g_topics[i].topic_name, name) == 0) {
            *out_id = g_topics[i].topic_id;
            return 0;
        }
    }
    return -1;
}

// Send simple UDP packet to last client address
static void send_udp(const uint8_t *buf, int len) {
    if (g_sock < 0 || g_last_client_addrlen == 0) return;
    sendto(g_sock, (const char*)buf, len, 0, (struct sockaddr*)&g_last_client_addr, g_last_client_addrlen);
}

// Build and send CONNACK (accept always)
static void send_connack() {
    uint8_t buf[3];
    buf[0] = 3; // length
    buf[1] = MQTT_SN_CONNACK;
    buf[2] = MQTT_SN_ACCEPTED;
    send_udp(buf, 3);
    printf("Sent CONNACK\n");
}

// Build and send REGACK
static void send_regack(uint16_t topic_id, uint16_t msg_id) {
    uint8_t buf[7];
    buf[0] = 7; // length
    buf[1] = MQTT_SN_REGACK;
    buf[2] = (topic_id >> 8) & 0xFF;
    buf[3] = topic_id & 0xFF;
    buf[4] = (msg_id >> 8) & 0xFF;
    buf[5] = msg_id & 0xFF;
    buf[6] = MQTT_SN_ACCEPTED;
    send_udp(buf, 7);
    printf("Sent REGACK topic_id=%d msg_id=%d\n", topic_id, msg_id);
}

// Build and send SUBACK
static void send_suback(uint8_t flags, uint16_t topic_id, uint16_t msg_id) {
    uint8_t buf[8];
    buf[0] = 8; // length
    buf[1] = MQTT_SN_SUBACK;
    buf[2] = flags;
    buf[3] = (topic_id >> 8) & 0xFF;
    buf[4] = topic_id & 0xFF;
    buf[5] = (msg_id >> 8) & 0xFF;
    buf[6] = msg_id & 0xFF;
    buf[7] = MQTT_SN_ACCEPTED;
    send_udp(buf, 8);
    printf("Sent SUBACK topic_id=%d msg_id=%d\n", topic_id, msg_id);
}

// Forward MQTT broker message into MQTT-SN PUBLISH to client
static void broker_message_cb(const char *topic, const void *payload, int len, void *ctx) {
    uint16_t tid;
    if (find_topic_id_by_name(topic, &tid) != 0) {
        printf("Broker message for unsubscribed topic: %s\n", topic);
        return;
    }

    // Build PUBLISH (QoS 0)
    int total_len = 5 + len; // length(1)+type(1)+flags(1)+topic_id(2)+payload
    if (total_len > MAX_PACKET_SIZE) return;
    uint8_t buf[MAX_PACKET_SIZE];
    buf[0] = total_len;
    buf[1] = MQTT_SN_PUBLISH;
    buf[2] = MQTT_SN_QOS_0; // flags (qos=0)
    buf[3] = (tid >> 8) & 0xFF;
    buf[4] = tid & 0xFF;
    memcpy(&buf[5], payload, len);
    send_udp(buf, total_len);
    printf("Forwarded broker->client topic=%s (id=%d) len=%d\n", topic, tid, len);
}

int main(int argc, char **argv) {
    const char *broker = "tcp://localhost:1883";
    int udp_port = DEFAULT_UDP_PORT;

    if (argc >= 2) broker = argv[1];
    if (argc >= 3) udp_port = atoi(argv[2]);

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif

    // Initialize Paho
    if (mqtt_paho_init(broker, "mqtt_sn_gateway") != 0) {
        fprintf(stderr, "Failed to initialize MQTT (Paho) client\n");
        return 1;
    }
    mqtt_paho_set_message_callback(broker_message_cb, NULL);

    // Setup UDP socket
    g_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (g_sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(udp_port);

    if (bind(g_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    printf("MQTT-SN gateway listening on UDP port %d, broker=%s\n", udp_port, broker);

    // Main receive loop
    uint8_t buf[MAX_PACKET_SIZE];
    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int r = recvfrom(g_sock, (char*)buf, sizeof(buf), 0, (struct sockaddr*)&client_addr, &client_len);
        if (r <= 0) continue;

        // Save last client address (single-client gateway assumption)
        g_last_client_addr = client_addr;
        g_last_client_addrlen = client_len;

        if (r < 2) continue;
        uint8_t msg_type = buf[1];

        switch (msg_type) {
            case MQTT_SN_CONNECT:
                printf("Received CONNECT from client\n");
                send_connack();
                break;

            case MQTT_SN_REGISTER: {
                if (r < 6) break;
                uint16_t proposed_tid = (buf[2] << 8) | buf[3];
                uint16_t msg_id = (buf[4] << 8) | buf[5];
                
                // Calculate topic name length and ensure null termination
                int topic_name_len = buf[0] - 6;  // Total length minus header
                char topic_name[256];  // Buffer for topic name
                memcpy(topic_name, &buf[6], topic_name_len);
                topic_name[topic_name_len] = '\0';  // Ensure null termination
                
                uint16_t assigned = g_next_topic_id++;
                add_topic_mapping(assigned, topic_name);
                // Send REGACK
                send_regack(assigned, msg_id);
                printf("REGISTER: name=%s -> id=%d\n", topic_name, assigned);
                break;
            }

            case MQTT_SN_PUBLISH: {
                if (r < 5) break;
                uint8_t flags = buf[2];
                uint16_t topic_id = (buf[3] << 8) | buf[4];
                int qos = flags & 0x03;
                int offset = 5;
                uint16_t msg_id = 0;
                if (qos > MQTT_SN_QOS_0) {
                    if (r < 7) break;
                    msg_id = (buf[5] << 8) | buf[6];
                    offset += 2;
                }
                int payload_len = r - offset;
                const uint8_t *payload = &buf[offset];
                const char *topic_name = find_topic_name(topic_id);
                if (!topic_name) {
                    printf("Unknown topic_id %d\n", topic_id);
                    break;
                }
                // Forward to MQTT broker
                mqtt_paho_publish(topic_name, payload, payload_len, qos);
                printf("Forwarded PUBLISH id=%d -> broker topic=%s len=%d\n", topic_id, topic_name, payload_len);
                // For QoS1 send PUBACK back (simple accepted)
                if (qos == MQTT_SN_QOS_1) {
                    uint8_t ack[7];
                    ack[0] = 7; ack[1] = MQTT_SN_PUBACK;
                    ack[2] = (topic_id >> 8) & 0xFF; ack[3] = topic_id & 0xFF;
                    ack[4] = (msg_id >> 8) & 0xFF; ack[5] = msg_id & 0xFF;
                    ack[6] = MQTT_SN_ACCEPTED;
                    send_udp(ack, 7);
                }
                break;
            }

            case MQTT_SN_SUBSCRIBE: {
                if (r < 6) break;
                uint8_t flags = buf[2];
                uint16_t msg_id = (buf[3] << 8) | buf[4];
                const char *topic_name = (const char*)&buf[5];
                // Assign topic id if not existing
                uint16_t tid;
                if (find_topic_id_by_name(topic_name, &tid) != 0) {
                    tid = g_next_topic_id++;
                    add_topic_mapping(tid, topic_name);
                }
                // Subscribe on broker
                mqtt_paho_subscribe(topic_name, flags & 0x03);
                // Send SUBACK
                send_suback(flags, tid, msg_id);
                printf("SUBSCRIBE: name=%s -> id=%d\n", topic_name, tid);
                break;
            }

            default:
                printf("Unhandled MQTT-SN message type: 0x%02x\n", msg_type);
                break;
        }
    }

    mqtt_paho_cleanup();
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

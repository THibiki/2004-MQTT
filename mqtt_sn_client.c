#include "mqtt_sn_client.h"
#include "wifi_driver.h"
#include "block_transfer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// MQTT-SN message types
#define MQTTSN_CONNECT      0x04
#define MQTTSN_CONNACK      0x05
#define MQTTSN_SUBSCRIBE    0x12
#define MQTTSN_SUBACK       0x13
#define MQTTSN_PUBLISH      0x0C
#define MQTTSN_PUBACK       0x0D
#define MQTTSN_DISCONNECT   0x18

// MQTT-SN flags
#define MQTTSN_FLAG_CLEAN_SESSION 0x04
#define MQTTSN_FLAG_TOPIC_NAME    0x00
#define MQTTSN_FLAG_QOS_0         0x00
#define MQTTSN_FLAG_QOS_1         0x20
#define MQTTSN_FLAG_QOS_2         0x40

// Connection state
static bool connected = false;
static char gateway_host[64];
static uint16_t gateway_port;
static uint16_t msg_id = 1;
static mqttsn_message_callback_t message_callback = NULL;

// Helper function to send MQTT-SN packet
static int send_packet(const uint8_t *packet, size_t len) {
    return wifi_udp_send(gateway_host, gateway_port, packet, len);
}

// Helper function to receive MQTT-SN packet
static int receive_packet(uint8_t *buffer, size_t max_len, uint32_t timeout_ms) {
    return wifi_udp_receive(buffer, max_len, timeout_ms);
}

int mqttsn_init(const char *gateway_host_param, uint16_t gateway_port_param) {
    strncpy(gateway_host, gateway_host_param, sizeof(gateway_host) - 1);
    gateway_host[sizeof(gateway_host) - 1] = '\0';
    gateway_port = gateway_port_param;
    connected = false;
    
    printf("MQTT-SN initialized - Gateway: %s:%d\n", gateway_host, gateway_port);
    return MQTTSN_OK;
}

int mqttsn_connect(const char *client_id, uint16_t keep_alive) {
    if (connected) {
        return MQTTSN_OK;
    }
    
    printf("Connecting to MQTT-SN gateway %s:%d...\n", gateway_host, gateway_port);
    printf("Client ID: %s, Keep-alive: %d\n", client_id, keep_alive);
    
    // Build CONNECT packet
    uint8_t packet[64];
    size_t pos = 0;
    
    // Length (will be filled later)
    packet[pos++] = 0;
    
    // Message type
    packet[pos++] = MQTTSN_CONNECT;
    
    // Flags
    packet[pos++] = MQTTSN_FLAG_CLEAN_SESSION;
    
    // Protocol ID
    packet[pos++] = 0x01;
    
    // Duration (keep alive)
    packet[pos++] = (keep_alive >> 8) & 0xFF;
    packet[pos++] = keep_alive & 0xFF;
    
    // Client ID
    size_t client_id_len = strlen(client_id);
    memcpy(&packet[pos], client_id, client_id_len);
    pos += client_id_len;
    
    // Set length
    packet[0] = pos;
    
    printf("Sending CONNECT packet (%zu bytes)...\n", pos);
    printf("Packet hex: ");
    for (size_t i = 0; i < pos && i < 20; i++) {
        printf("%02X ", packet[i]);
    }
    printf("\n");
    
    // Send CONNECT
    if (send_packet(packet, pos) != WIFI_OK) {
        printf("❌ Failed to send CONNECT packet\n");
        return MQTTSN_ERROR;
    }
    
    printf("CONNECT packet sent, waiting for CONNACK (5s timeout)...\n");
    
    // Wait for CONNACK
    uint8_t response[32];
    int len = receive_packet(response, sizeof(response), 5000);
    
    printf("Received response: %d bytes\n", len);
    if (len > 0) {
        printf("Response hex: ");
        for (int i = 0; i < len && i < 20; i++) {
            printf("%02X ", response[i]);
        }
        printf("\n");
    }
    
    if (len < 3) {
        printf("❌ Response too short (expected >=3 bytes, got %d)\n", len);
        return MQTTSN_TIMEOUT;
    }
    
    if (response[1] != MQTTSN_CONNACK) {
        printf("❌ Not a CONNACK (expected 0x05, got 0x%02X)\n", response[1]);
        return MQTTSN_TIMEOUT;
    }
    
    if (response[2] == 0) {
        connected = true;
        printf("✅ MQTT-SN connected successfully!\n");
        return MQTTSN_OK;
    } else {
        printf("❌ MQTT-SN connection rejected with code: %d\n", response[2]);
        return MQTTSN_ERROR;
    }
}

int mqttsn_disconnect(void) {
    if (!connected) {
        return MQTTSN_OK;
    }
    
    uint8_t packet[2] = {2, MQTTSN_DISCONNECT};
    send_packet(packet, 2);
    
    connected = false;
    printf("MQTT-SN disconnected\n");
    return MQTTSN_OK;
}

int mqttsn_subscribe(const char *topic, mqttsn_qos_t qos) {
    if (!connected) {
        return MQTTSN_NOT_CONNECTED;
    }
    
    printf("Subscribing to topic: %s\n", topic);
    
    // Build SUBSCRIBE packet
    uint8_t packet[64];
    size_t pos = 0;
    
    // Length (will be filled later)
    packet[pos++] = 0;
    
    // Message type
    packet[pos++] = MQTTSN_SUBSCRIBE;
    
    // Flags
    uint8_t flags = MQTTSN_FLAG_TOPIC_NAME;
    if (qos == QOS_1) flags |= MQTTSN_FLAG_QOS_1;
    else if (qos == QOS_2) flags |= MQTTSN_FLAG_QOS_2;
    packet[pos++] = flags;
    
    // Message ID
    packet[pos++] = (msg_id >> 8) & 0xFF;
    packet[pos++] = msg_id & 0xFF;
    msg_id++;
    
    // Topic name
    size_t topic_len = strlen(topic);
    memcpy(&packet[pos], topic, topic_len);
    pos += topic_len;
    
    // Set length
    packet[0] = pos;
    
    // Send SUBSCRIBE
    if (send_packet(packet, pos) != WIFI_OK) {
        printf("Failed to send SUBSCRIBE packet\n");
        return MQTTSN_ERROR;
    }
    
    // Wait for SUBACK
    uint8_t response[32];
    int len = receive_packet(response, sizeof(response), 5000);
    if (len < 3 || response[1] != MQTTSN_SUBACK) {
        printf("No SUBACK received\n");
        return MQTTSN_TIMEOUT;
    }
    
    printf("Subscribed to %s successfully\n", topic);
    return MQTTSN_OK;
}

int mqttsn_publish(const char *topic, const uint8_t *data, size_t len, mqttsn_qos_t qos) {
    if (!connected) {
        return MQTTSN_NOT_CONNECTED;
    }
    
    // Build PUBLISH packet
    uint8_t packet[256];
    size_t pos = 0;
    
    // Length (will be filled later)
    packet[pos++] = 0;
    
    // Message type
    packet[pos++] = MQTTSN_PUBLISH;
    
    // Flags
    uint8_t flags = MQTTSN_FLAG_TOPIC_NAME;
    if (qos == QOS_1) flags |= MQTTSN_FLAG_QOS_1;
    else if (qos == QOS_2) flags |= MQTTSN_FLAG_QOS_2;
    packet[pos++] = flags;
    
    // Topic name
    size_t topic_len = strlen(topic);
    memcpy(&packet[pos], topic, topic_len);
    pos += topic_len;
    
    // Message ID (for QoS > 0)
    if (qos > QOS_0) {
        packet[pos++] = (msg_id >> 8) & 0xFF;
        packet[pos++] = msg_id & 0xFF;
        msg_id++;
    }
    
    // Data
    if (pos + len > sizeof(packet)) {
        printf("Publish data too large\n");
        return MQTTSN_ERROR;
    }
    
    memcpy(&packet[pos], data, len);
    pos += len;
    
    // Set length
    packet[0] = pos;
    
    // Send PUBLISH
    return send_packet(packet, pos) == WIFI_OK ? MQTTSN_OK : MQTTSN_ERROR;
}

int mqttsn_poll(void) {
    if (!connected) {
        return MQTTSN_NOT_CONNECTED;
    }
    
    uint8_t buffer[256];
    int len = receive_packet(buffer, sizeof(buffer), 0); // Non-blocking
    
    if (len > 0) {
        if (len >= 2 && buffer[1] == MQTTSN_PUBLISH) {
            // Extract topic and data from PUBLISH message
            if (len < 4) return MQTTSN_OK; // Malformed packet
            
            uint8_t flags = buffer[2];
            size_t pos = 3;
            
            // Find end of topic name (look for message ID or data start)
            size_t topic_start = pos;
            
            // For simplicity, assume topic ends when we find non-printable chars or specific patterns
            // In a real implementation, you'd need to parse the full MQTT-SN protocol
            
            // Extract topic (simplified - assumes topic is null-terminated or we know the format)
            char topic[64] = {0};
            size_t topic_len = 0;
            
            // Look for known topic patterns
            if (pos + 10 < len && memcmp(&buffer[pos], "pico/chunks", 11) == 0) {
                strcpy(topic, "pico/chunks");
                topic_len = 11;
            } else if (pos + 9 < len && memcmp(&buffer[pos], "pico/test", 9) == 0) {
                strcpy(topic, "pico/test");
                topic_len = 9;
            } else if (pos + 9 < len && memcmp(&buffer[pos], "pico/data", 9) == 0) {
                strcpy(topic, "pico/data");
                topic_len = 9;
            } else if (pos + 10 < len && memcmp(&buffer[pos], "pico/block", 10) == 0) {
                strcpy(topic, "pico/block");
                topic_len = 10;
            } else {
                // Unknown topic, try to extract first 20 chars as topic
                topic_len = (len - pos > 20) ? 20 : (len - pos);
                memcpy(topic, &buffer[pos], topic_len);
                topic[topic_len] = '\0';
                
                // Clean up topic (remove non-printable chars)
                for (size_t i = 0; i < topic_len; i++) {
                    if (topic[i] < 32 || topic[i] > 126) {
                        topic_len = i;
                        break;
                    }
                }
                topic[topic_len] = '\0';
            }
            
            pos += topic_len;
            
            // Skip message ID if QoS > 0
            if (flags & (MQTTSN_FLAG_QOS_1 | MQTTSN_FLAG_QOS_2)) {
                pos += 2;
            }
            
            // Extract data
            if (pos < len) {
                const uint8_t *data = &buffer[pos];
                size_t data_len = len - pos;
                
                printf("Received message on '%s': %zu bytes\n", topic, data_len);
                
                // Handle block transfer chunks
                if (strcmp(topic, "pico/chunks") == 0) {
                    process_block_chunk(data, data_len);
                } else if (message_callback) {
                    message_callback(topic, data, data_len);
                }
            }
        }
    }
    
    return MQTTSN_OK;
}

bool mqttsn_is_connected(void) {
    return connected;
}

void mqttsn_set_message_callback(mqttsn_message_callback_t callback) {
    message_callback = callback;
}
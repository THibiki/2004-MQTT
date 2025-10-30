#include "mqtt_sn_client.h"
#include "wifi_driver.h"
#include "MQTTSNPacket.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"  
#include <string.h>
#include <stdio.h>

#define MAX_PACKET_SIZE 256
#define RESPONSE_TIMEOUT_MS 5000
#define MAX_PENDING_MESSAGES 10

typedef struct {
    uint16_t msg_id;
    uint8_t qos;
    uint8_t state;  // For QoS 2: 0=none, 1=wait_PUBREC, 2=wait_PUBCOMP
    uint32_t last_send_time;
    uint32_t retry_timeout;
    uint8_t retry_count;
    uint8_t packet[MAX_PACKET_SIZE];
    size_t packet_len;
} pending_message_t;

static pending_message_t pending_messages[MAX_PENDING_MESSAGES];

static char gateway_ip[16];
static uint16_t gateway_port;
static uint8_t tx_buffer[MAX_PACKET_SIZE];
static uint8_t rx_buffer[MAX_PACKET_SIZE];
static bool connected = false;
static uint16_t msg_id = 1;

// Helper function to find a free slot for pending messages
static int find_free_slot(void) {
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_messages[i].msg_id == 0) {
            return i;
        }
    }
    return -1; // No free slot
}

// Helper function to clear a pending message by message ID
static void clear_pending_message(uint16_t msg_id_to_clear) {
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_messages[i].msg_id == msg_id_to_clear) {
            pending_messages[i].msg_id = 0; // Clear slot
            break;
        }
    }
}

int mqttsn_init(const char *gw_ip, uint16_t gw_port) {
    if (!gw_ip) return MQTTSN_ERROR;
    
    strncpy(gateway_ip, gw_ip, sizeof(gateway_ip) - 1);
    gateway_ip[sizeof(gateway_ip) - 1] = '\0';
    gateway_port = gw_port;
    connected = false;
    msg_id = 1;
    
    printf("MQTT-SN: Initialized (gateway: %s:%d)\n", gateway_ip, gateway_port);
    return MQTTSN_OK;
}

int mqttsn_connect(const char *client_id, uint16_t keep_alive_sec) {
    MQTTSNPacket_connectData options = MQTTSNPacket_connectData_initializer;
    int len;
    
    if (!client_id) return MQTTSN_ERROR;
    
    options.clientID.cstring = (char*)client_id;
    options.duration = keep_alive_sec;
    options.cleansession = 1;
    
    // Serialize CONNECT packet
    len = MQTTSNSerialize_connect(tx_buffer, MAX_PACKET_SIZE, &options);
    if (len <= 0) {
        printf("MQTT-SN: Failed to serialize CONNECT\n");
        return MQTTSN_ERROR;
    }
    
    // Send CONNECT packet
    int send_result = wifi_udp_send(gateway_ip, gateway_port, tx_buffer, len);
    if (send_result < 0) { 
        printf("MQTT-SN: Failed to send CONNECT (error: %d)\n", send_result);
        return MQTTSN_ERROR;
    }

    printf("MQTT-SN: CONNECT sent (%d bytes)\n", len);
    
    // Wait for CONNACK
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start_time < RESPONSE_TIMEOUT_MS) {
        size_t recv_len = MAX_PACKET_SIZE;
        
        // Fixed: wifi_udp_receive takes max_len by value, not pointer
        // We need to update wifi_driver to return actual length received
        int ret = wifi_udp_receive(rx_buffer, recv_len, 100);
        
        if (ret > 0) {  // Assuming wifi_udp_receive returns bytes received
            recv_len = ret;
            unsigned char packet_type = rx_buffer[1];
            
            if (packet_type == MQTTSN_CONNACK) {
                int return_code;
                // Fixed: MQTTSNDeserialize_connack signature
                if (MQTTSNDeserialize_connack(&return_code, rx_buffer, recv_len) == 1) {
                    if (return_code == 0) {
                        connected = true;
                        printf("MQTT-SN: Connected successfully\n");
                        return MQTTSN_OK;
                    } else {
                        printf("MQTT-SN: Connection rejected (code: %d)\n", return_code);
                        return MQTTSN_ERROR;
                    }
                }
            }
        }
        
        cyw43_arch_poll();
        sleep_ms(10);
    }
    
    printf("MQTT-SN: Connection timeout\n");
    return MQTTSN_TIMEOUT;
}

int mqttsn_publish(const char *topic, const uint8_t *payload, size_t len, uint8_t qos) {
    MQTTSN_topicid topic_id;
    int packet_len;
    
    if (!connected) {
        printf("MQTT-SN: Not connected\n");
        return MQTTSN_ERROR;
    }
    
    if (!topic || !payload) return MQTTSN_ERROR;
    
    // For simplicity, use normal topic name
    topic_id.type = MQTTSN_TOPIC_TYPE_NORMAL;
    topic_id.data.long_.name = (char*)topic;
    topic_id.data.long_.len = strlen(topic);
    
    // Serialize PUBLISH packet
    packet_len = MQTTSNSerialize_publish(tx_buffer, MAX_PACKET_SIZE, 
                                         0, qos, 0, msg_id++, 
                                         topic_id, 
                                         (unsigned char*)payload, len);
    
    if (packet_len <= 0) {
        printf("MQTT-SN: Failed to serialize PUBLISH\n");
        return MQTTSN_ERROR;
    }

    if (qos > 0) {
        // Store message for retry
        int slot = find_free_slot();
        pending_messages[slot].msg_id = msg_id;
        pending_messages[slot].qos = qos;
        pending_messages[slot].retry_timeout = 1000; // Start at 1 second
        pending_messages[slot].retry_count = 0;
        memcpy(pending_messages[slot].packet, tx_buffer, packet_len);
        pending_messages[slot].packet_len = packet_len;
        pending_messages[slot].last_send_time = to_ms_since_boot(get_absolute_time());
        
        if (qos == 2) {
            pending_messages[slot].state = 1; // Wait for PUBREC
        }
    }
    
    // Send PUBLISH packet
    int send_result = wifi_udp_send(gateway_ip, gateway_port, tx_buffer, packet_len);
    if (send_result < 0) {
        printf("MQTT-SN: Failed to send PUBLISH (error: %d)\n", send_result);
        return MQTTSN_ERROR;
    }
    
    printf("MQTT-SN: Published to '%s' (%zu bytes payload)\n", topic, len);
    return MQTTSN_OK;
}

int mqttsn_subscribe(const char *topic, uint8_t qos) {
    MQTTSN_topicid topic_id;
    int packet_len;
    
    if (!connected) return MQTTSN_ERROR;
    if (!topic) return MQTTSN_ERROR;
    
    topic_id.type = MQTTSN_TOPIC_TYPE_NORMAL;
    topic_id.data.long_.name = (char*)topic;
    topic_id.data.long_.len = strlen(topic);
    
    // Serialize SUBSCRIBE packet
    packet_len = MQTTSNSerialize_subscribe(tx_buffer, MAX_PACKET_SIZE, 
                                           0, qos, msg_id++, &topic_id);
    
    if (packet_len <= 0) {
        printf("MQTT-SN: Failed to serialize SUBSCRIBE\n");
        return MQTTSN_ERROR;
    }
    
    // Send SUBSCRIBE packet
    int send_result = wifi_udp_send(gateway_ip, gateway_port, tx_buffer, packet_len);
    if (send_result < 0) {
        printf("MQTT-SN: Failed to send SUBSCRIBE (error: %d)\n", send_result);
        return MQTTSN_ERROR;
    }
    
    printf("MQTT-SN: Subscribed to '%s'\n", topic);
    return MQTTSN_OK;
}

void mqttsn_poll(void) {
    if (!connected) return;
    
    size_t recv_len = MAX_PACKET_SIZE;
    int ret = wifi_udp_receive(rx_buffer, recv_len, 0);
    
    if (ret > 0) {
        recv_len = ret;
        unsigned char packet_type = rx_buffer[1];
        
        switch (packet_type) {
            case MQTTSN_PUBACK: {
                unsigned short msg_id_resp;
                // Parse PUBACK
                // Find and remove from pending_messages
                printf("MQTT-SN: PUBACK received for MsgID %d\n", msg_id_resp);
                clear_pending_message(msg_id_resp);
                break;
            }
                    
            case MQTTSN_SUBACK: {
                int granted_qos;
                unsigned char return_code;  // Changed from int to unsigned char
                unsigned short msg_id_resp, topic_id;
                // Fixed: MQTTSNDeserialize_suback signature
                if (MQTTSNDeserialize_suback(&granted_qos, &topic_id, &msg_id_resp,
                                             &return_code, rx_buffer, recv_len) == 1) {
                    printf("MQTT-SN: SUBACK (QoS: %d, TopicID: %d, RC: %d)\n", 
                           granted_qos, topic_id, return_code);
                }
                break;
            }
                
            case MQTTSN_PUBLISH: {
                unsigned char dup, retained;
                int qos;
                unsigned short msg_id_recv;
                int payload_len;
                unsigned char *payload;
                MQTTSN_topicid topic;
                
                // Fixed: MQTTSNDeserialize_publish signature
                if (MQTTSNDeserialize_publish(&dup, &qos, &retained, &msg_id_recv,
                                              &topic, &payload, &payload_len,
                                              rx_buffer, recv_len) == 1) {
                    printf("MQTT-SN: Message received (%d bytes): ", payload_len);
                    for (int i = 0; i < payload_len; i++) {
                        printf("%c", payload[i]);
                    }
                    printf("\n");
                }
                break;
            }
                
            case MQTTSN_PINGREQ: {
                int len = MQTTSNSerialize_pingresp(tx_buffer, MAX_PACKET_SIZE);
                if (len > 0) {
                    wifi_udp_send(gateway_ip, gateway_port, tx_buffer, len);
                }
                break;
            }

            case MQTTSN_PUBREC: {
                unsigned short msg_id_resp;
                // Parse PUBREC
                // Update state to wait for PUBCOMP
                // Send PUBREL
                int len = MQTTSNSerialize_pubrel(tx_buffer, MAX_PACKET_SIZE, msg_id_resp);
                wifi_udp_send(gateway_ip, gateway_port, tx_buffer, len);
                
                // Find the slot for this message ID and update state
                for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
                    if (pending_messages[i].msg_id == msg_id_resp) {
                        pending_messages[i].state = 2; // Wait for PUBCOMP
                        break;
                    }
                }
                break;
            }

            case MQTTSN_PUBCOMP: {
                unsigned short msg_id_resp;
                // Parse PUBCOMP
                // Clear pending message - delivery complete
                clear_pending_message(msg_id_resp);
                break;
            }
                
            default:
                printf("MQTT-SN: Packet type 0x%02X received\n", packet_type);
                break;
        }
    }
    uint32_t now = to_ms_since_boot(get_absolute_time());
    for (int i = 0; i < MAX_PENDING_MESSAGES; i++) {
        if (pending_messages[i].msg_id == 0) continue;
        
        if (now - pending_messages[i].last_send_time > pending_messages[i].retry_timeout) {
            if (pending_messages[i].retry_count >= 4) {
                // Max retries reached, fail
                printf("MQTT-SN: Message %d failed after max retries\n", 
                       pending_messages[i].msg_id);
                pending_messages[i].msg_id = 0; // Clear slot
            } else {
                // Retry with exponential backoff
                wifi_udp_send(gateway_ip, gateway_port, 
                             pending_messages[i].packet, 
                             pending_messages[i].packet_len);
                
                pending_messages[i].retry_count++;
                pending_messages[i].retry_timeout *= 2; // Exponential backoff
                pending_messages[i].last_send_time = now;
                
                printf("MQTT-SN: Retry %d for MsgID %d (timeout now %d ms)\n",
                       pending_messages[i].retry_count,
                       pending_messages[i].msg_id,
                       pending_messages[i].retry_timeout);
            }
        }
    }
}


void mqttsn_disconnect(void) {
    if (!connected) return;
    
    int len = MQTTSNSerialize_disconnect(tx_buffer, MAX_PACKET_SIZE, 0);
    if (len > 0) {
        wifi_udp_send(gateway_ip, gateway_port, tx_buffer, len);
    }
    
    connected = false;
    printf("MQTT-SN: Disconnected\n");
}

bool mqttsn_is_connected(void) {
    return connected;
}

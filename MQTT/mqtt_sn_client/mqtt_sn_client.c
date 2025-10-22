#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "mqtt_sn_client.h"

// UDP receive callback
static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, 
                             const ip_addr_t *addr, u16_t port) {
    mqtt_sn_client_t *client = (mqtt_sn_client_t *)arg;
    
    if (p != NULL) {
        printf("Received UDP packet: %d bytes from %s:%d\n", 
               p->tot_len, ipaddr_ntoa(addr), port);
        
        // Process the MQTT-SN message
        mqtt_sn_message_t msg;
        if (mqtt_sn_parse_message((const uint8_t *)p->payload, p->tot_len, &msg) == 0) {
            printf("Parsed MQTT-SN message: type=0x%02X, length=%d\n", msg.msg_type, msg.length);
            
            // Handle different message types
            switch (msg.msg_type) {
                case MQTT_SN_CONNACK:
                    mqtt_sn_handle_connack(client, (const uint8_t *)p->payload, p->tot_len);
                    break;
                case MQTT_SN_REGACK:
                    mqtt_sn_handle_regack(client, (const uint8_t *)p->payload, p->tot_len);
                    break;
                case MQTT_SN_PUBLISH:
                    mqtt_sn_handle_publish(client, (const uint8_t *)p->payload, p->tot_len);
                    break;
                case MQTT_SN_SUBACK:
                    mqtt_sn_handle_suback(client, (const uint8_t *)p->payload, p->tot_len);
                    break;
                default:
                    printf("Unhandled message type: 0x%02X\n", msg.msg_type);
                    break;
            }
        } else {
            printf("Failed to parse MQTT-SN message\n");
        }
        
        pbuf_free(p);
    }
}

int mqtt_sn_client_init(mqtt_sn_client_t *client, const char *client_id, 
                       const char *gateway_ip, uint16_t gateway_port) {
    if (!client || !client_id || !gateway_ip) {
        return -1;
    }
    
    // Initialize client structure
    memset(client, 0, sizeof(mqtt_sn_client_t));
    
    // Copy client ID
    strncpy(client->client_id, client_id, MQTT_SN_CLIENT_ID_MAX_LEN);
    client->client_id[MQTT_SN_CLIENT_ID_MAX_LEN] = '\0';
    
    // Set gateway address
    if (!ipaddr_aton(gateway_ip, &client->gateway_addr)) {
        printf("Invalid gateway IP address: %s\n", gateway_ip);
        return -1;
    }
    client->gateway_port = gateway_port;
    
    // Set default values
    client->keepalive = MQTT_SN_DEFAULT_KEEPALIVE;
    client->state = MQTT_SN_STATE_DISCONNECTED;
    client->next_msg_id = 1;
    client->next_topic_id = 1;
    
    // Initialize latency measurement
    memset(client->pending_messages, 0, sizeof(client->pending_messages));
    mqtt_sn_reset_latency_stats(client);
    
    // Create UDP PCB
    client->udp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!client->udp_pcb) {
        printf("Failed to create UDP PCB\n");
        return -1;
    }
    
    // Bind to any local port
    if (udp_bind(client->udp_pcb, IP_ANY_TYPE, 0) != ERR_OK) {
        printf("Failed to bind UDP PCB\n");
        udp_remove(client->udp_pcb);
        return -1;
    }
    
    // Set receive callback
    udp_recv(client->udp_pcb, udp_recv_callback, client);
    
    printf("MQTT-SN client initialized: %s -> %s:%d\n", 
           client->client_id, gateway_ip, gateway_port);
    
    return 0;
}

void mqtt_sn_client_cleanup(mqtt_sn_client_t *client) {
    if (client && client->udp_pcb) {
        udp_remove(client->udp_pcb);
        client->udp_pcb = NULL;
    }
}

int mqtt_sn_connect(mqtt_sn_client_t *client) {
    if (!client || client->state != MQTT_SN_STATE_DISCONNECTED) {
        return -1;
    }
    
    uint8_t buffer[256];
    int len = mqtt_sn_build_connect(client, buffer, sizeof(buffer));
    if (len <= 0) {
        printf("Failed to build CONNECT message\n");
        return -1;
    }
    
    // Send CONNECT message
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (!p) {
        printf("Failed to allocate pbuf for CONNECT\n");
        return -1;
    }
    
    memcpy(p->payload, buffer, len);
    
    err_t err = udp_sendto(client->udp_pcb, p, &client->gateway_addr, client->gateway_port);
    pbuf_free(p);
    
    if (err != ERR_OK) {
        printf("Failed to send CONNECT message: %d\n", err);
        return -1;
    }
    
    printf("Sent CONNECT message (%d bytes)\n", len);
    client->state = MQTT_SN_STATE_CONNECTING;
    
    // Record message for latency measurement (CONNECT doesn't have msg_id, use 0)
    mqtt_sn_record_message_sent(client, 0, MQTT_SN_CONNECT);
    
    return 0;
}

int mqtt_sn_register_topic(mqtt_sn_client_t *client, const char *topic_name) {
    if (!client || !topic_name || client->state != MQTT_SN_STATE_CONNECTED) {
        return -1;
    }
    
    uint8_t buffer[256];
    int len = mqtt_sn_build_register(client, topic_name, buffer, sizeof(buffer));
    if (len <= 0) {
        printf("Failed to build REGISTER message\n");
        return -1;
    }
    
    // Send REGISTER message
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (!p) {
        printf("Failed to allocate pbuf for REGISTER\n");
        return -1;
    }
    
    memcpy(p->payload, buffer, len);
    
    err_t err = udp_sendto(client->udp_pcb, p, &client->gateway_addr, client->gateway_port);
    pbuf_free(p);
    
    if (err != ERR_OK) {
        printf("Failed to send REGISTER message: %d\n", err);
        return -1;
    }
    
    printf("Sent REGISTER message for topic: %s (%d bytes)\n", topic_name, len);
    client->state = MQTT_SN_STATE_REGISTERING;
    
    // Record message for latency measurement
    mqtt_sn_record_message_sent(client, client->next_msg_id - 1, MQTT_SN_REGISTER);
    
    return 0;
}

int mqtt_sn_publish(mqtt_sn_client_t *client, uint16_t topic_id, 
                   const uint8_t *data, uint16_t data_len, uint8_t qos) {
    if (!client || !data || client->state != MQTT_SN_STATE_READY) {
        return -1;
    }
    
    uint8_t buffer[512];
    int len = mqtt_sn_build_publish(client, topic_id, data, data_len, qos, buffer, sizeof(buffer));
    if (len <= 0) {
        printf("Failed to build PUBLISH message\n");
        return -1;
    }
    
    // Send PUBLISH message
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (!p) {
        printf("Failed to allocate pbuf for PUBLISH\n");
        return -1;
    }
    
    memcpy(p->payload, buffer, len);
    
    err_t err = udp_sendto(client->udp_pcb, p, &client->gateway_addr, client->gateway_port);
    pbuf_free(p);
    
    if (err != ERR_OK) {
        printf("Failed to send PUBLISH message: %d\n", err);
        return -1;
    }
    
    printf("Sent PUBLISH message (topic_id=%d, data_len=%d, %d bytes)\n", 
           topic_id, data_len, len);
    
    // Record message for latency measurement (only for QoS 1)
    if (qos == MQTT_SN_QOS_1) {
        mqtt_sn_record_message_sent(client, client->next_msg_id - 1, MQTT_SN_PUBLISH);
    }
    
    return 0;
}

int mqtt_sn_subscribe(mqtt_sn_client_t *client, const char *topic_name, uint8_t qos) {
    if (!client || !topic_name || client->state != MQTT_SN_STATE_CONNECTED) {
        return -1;
    }
    
    uint8_t buffer[256];
    int len = mqtt_sn_build_subscribe(client, topic_name, qos, buffer, sizeof(buffer));
    if (len <= 0) {
        printf("Failed to build SUBSCRIBE message\n");
        return -1;
    }
    
    // Send SUBSCRIBE message
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (!p) {
        printf("Failed to allocate pbuf for SUBSCRIBE\n");
        return -1;
    }
    
    memcpy(p->payload, buffer, len);
    
    err_t err = udp_sendto(client->udp_pcb, p, &client->gateway_addr, client->gateway_port);
    pbuf_free(p);
    
    if (err != ERR_OK) {
        printf("Failed to send SUBSCRIBE message: %d\n", err);
        return -1;
    }
    
    printf("Sent SUBSCRIBE message for topic: %s (%d bytes)\n", topic_name, len);
    
    // Record message for latency measurement
    mqtt_sn_record_message_sent(client, client->next_msg_id - 1, MQTT_SN_SUBSCRIBE);
    
    return 0;
}

int mqtt_sn_disconnect(mqtt_sn_client_t *client) {
    if (!client) {
        return -1;
    }
    
    // Build DISCONNECT message
    uint8_t buffer[4];
    buffer[0] = 2;  // Length
    buffer[1] = MQTT_SN_DISCONNECT;
    
    // Send DISCONNECT message
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 2, PBUF_RAM);
    if (!p) {
        printf("Failed to allocate pbuf for DISCONNECT\n");
        return -1;
    }
    
    memcpy(p->payload, buffer, 2);
    
    err_t err = udp_sendto(client->udp_pcb, p, &client->gateway_addr, client->gateway_port);
    pbuf_free(p);
    
    if (err != ERR_OK) {
        printf("Failed to send DISCONNECT message: %d\n", err);
        return -1;
    }
    
    printf("Sent DISCONNECT message\n");
    client->state = MQTT_SN_STATE_DISCONNECTED;
    
    return 0;
}

void mqtt_sn_client_process(mqtt_sn_client_t *client) {
    // This function is called periodically to process any pending operations
    // For now, it's mainly handled by the UDP receive callback
    // In a more complex implementation, this could handle timeouts, retries, etc.
    
    if (!client) {
        return;
    }
    
    // Check for message timeouts
    mqtt_sn_check_timeouts(client);
}

// Latency measurement functions
void mqtt_sn_record_message_sent(mqtt_sn_client_t *client, uint16_t msg_id, uint8_t msg_type) {
    if (!client) return;
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Find an empty slot for this message
    for (int i = 0; i < MQTT_SN_MAX_PENDING_MESSAGES; i++) {
        if (!client->pending_messages[i].active) {
            client->pending_messages[i].msg_id = msg_id;
            client->pending_messages[i].send_time_ms = now;
            client->pending_messages[i].msg_type = msg_type;
            client->pending_messages[i].active = true;
            break;
        }
    }
}

void mqtt_sn_record_message_ack(mqtt_sn_client_t *client, uint16_t msg_id, uint8_t msg_type) {
    if (!client) return;
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Find the pending message
    for (int i = 0; i < MQTT_SN_MAX_PENDING_MESSAGES; i++) {
        if (client->pending_messages[i].active && 
            client->pending_messages[i].msg_id == msg_id &&
            client->pending_messages[i].msg_type == msg_type) {
            
            // Calculate latency
            uint32_t latency = now - client->pending_messages[i].send_time_ms;
            
            // Update statistics
            client->latency_stats.message_count++;
            client->latency_stats.success_count++;
            client->latency_stats.total_latency_ms += latency;
            
            if (client->latency_stats.message_count == 1) {
                client->latency_stats.min_latency_ms = latency;
                client->latency_stats.max_latency_ms = latency;
            } else {
                if (latency < client->latency_stats.min_latency_ms) {
                    client->latency_stats.min_latency_ms = latency;
                }
                if (latency > client->latency_stats.max_latency_ms) {
                    client->latency_stats.max_latency_ms = latency;
                }
            }
            
            // Store in history
            client->latency_stats.latency_history[client->latency_stats.history_index] = latency;
            client->latency_stats.history_index = (client->latency_stats.history_index + 1) % MQTT_SN_STATS_HISTORY_SIZE;
            
            // Clear the pending message
            client->pending_messages[i].active = false;
            
            printf("Message %d (type 0x%02X) latency: %d ms\n", msg_id, msg_type, latency);
            break;
        }
    }
}

void mqtt_sn_check_timeouts(mqtt_sn_client_t *client) {
    if (!client) return;
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    const uint32_t timeout_ms = 5000; // 5 second timeout
    
    for (int i = 0; i < MQTT_SN_MAX_PENDING_MESSAGES; i++) {
        if (client->pending_messages[i].active) {
            if ((now - client->pending_messages[i].send_time_ms) > timeout_ms) {
                printf("Message %d (type 0x%02X) timed out after %d ms\n", 
                       client->pending_messages[i].msg_id,
                       client->pending_messages[i].msg_type,
                       now - client->pending_messages[i].send_time_ms);
                
                client->latency_stats.message_count++;
                client->latency_stats.timeout_count++;
                client->pending_messages[i].active = false;
            }
        }
    }
}

void mqtt_sn_print_latency_stats(mqtt_sn_client_t *client) {
    if (!client) return;
    
    printf("\n=== MQTT-SN Latency Statistics ===\n");
    printf("Total messages: %d\n", client->latency_stats.message_count);
    printf("Successful: %d\n", client->latency_stats.success_count);
    printf("Timeouts: %d\n", client->latency_stats.timeout_count);
    
    if (client->latency_stats.success_count > 0) {
        uint32_t avg_latency = client->latency_stats.total_latency_ms / client->latency_stats.success_count;
        printf("Min latency: %d ms\n", client->latency_stats.min_latency_ms);
        printf("Max latency: %d ms\n", client->latency_stats.max_latency_ms);
        printf("Average latency: %d ms\n", avg_latency);
        printf("Success rate: %.1f%%\n", 
               (float)client->latency_stats.success_count * 100.0f / client->latency_stats.message_count);
    }
    printf("===================================\n\n");
}

void mqtt_sn_reset_latency_stats(mqtt_sn_client_t *client) {
    if (!client) return;
    
    client->latency_stats.min_latency_ms = 0;
    client->latency_stats.max_latency_ms = 0;
    client->latency_stats.total_latency_ms = 0;
    client->latency_stats.message_count = 0;
    client->latency_stats.success_count = 0;
    client->latency_stats.timeout_count = 0;
    client->latency_stats.history_index = 0;
    memset(client->latency_stats.latency_history, 0, sizeof(client->latency_stats.latency_history));
}

#include <stdio.h>
#include <string.h>
#include "mqtt_sn_client.h"

// Message construction functions
int mqtt_sn_build_connect(mqtt_sn_client_t *client, uint8_t *buffer, uint16_t buffer_len) {
    if (!client || !buffer || buffer_len < 8) {
        return -1;
    }
    
    uint16_t client_id_len = strlen(client->client_id);
    uint16_t total_len = 6 + client_id_len;
    
    if (total_len > buffer_len) {
        return -1;
    }
    
    buffer[0] = total_len;                    // Length
    buffer[1] = MQTT_SN_CONNECT;              // Message type
    buffer[2] = 0x00;                         // Flags (clean session)
    buffer[3] = MQTT_SN_PROTOCOL_ID;          // Protocol ID
    buffer[4] = (client->keepalive >> 8) & 0xFF;  // Duration (keepalive) high byte
    buffer[5] = client->keepalive & 0xFF;     // Duration (keepalive) low byte
    memcpy(&buffer[6], client->client_id, client_id_len);
    
    printf("Built CONNECT message: length=%d, client_id=%s, keepalive=%d\n", 
           total_len, client->client_id, client->keepalive);
    
    return total_len;
}

int mqtt_sn_build_register(mqtt_sn_client_t *client, const char *topic_name, 
                          uint8_t *buffer, uint16_t buffer_len) {
    if (!client || !topic_name || !buffer || buffer_len < 8) {
        return -1;
    }
    
    uint16_t topic_name_len = strlen(topic_name);
    uint16_t total_len = 6 + topic_name_len;
    
    if (total_len > buffer_len) {
        return -1;
    }
    
    buffer[0] = total_len;                    // Length
    buffer[1] = MQTT_SN_REGISTER;             // Message type
    buffer[2] = (client->next_topic_id >> 8) & 0xFF;  // Topic ID high byte
    buffer[3] = client->next_topic_id & 0xFF; // Topic ID low byte
    buffer[4] = (client->next_msg_id >> 8) & 0xFF;    // Message ID high byte
    buffer[5] = client->next_msg_id & 0xFF;   // Message ID low byte
    memcpy(&buffer[6], topic_name, topic_name_len);
    
    printf("Built REGISTER message: length=%d, topic_id=%d, msg_id=%d, topic=%s\n", 
           total_len, client->next_topic_id, client->next_msg_id, topic_name);
    
    client->next_msg_id++;
    
    return total_len;
}

int mqtt_sn_build_publish(mqtt_sn_client_t *client, uint16_t topic_id, 
                         const uint8_t *data, uint16_t data_len, uint8_t qos,
                         uint8_t *buffer, uint16_t buffer_len) {
    if (!client || !data || !buffer || buffer_len < 8) {
        return -1;
    }
    
    uint16_t total_len = 7 + data_len;
    if (qos > MQTT_SN_QOS_0) {
        total_len += 2;  // Add message ID for QoS 1/2
    }
    
    if (total_len > buffer_len) {
        return -1;
    }
    
    uint8_t flags = 0;
    flags |= (qos & 0x03);  // QoS level
    flags |= MQTT_SN_TOPIC_NORMAL;  // Topic ID type
    
    buffer[0] = total_len;                    // Length
    buffer[1] = MQTT_SN_PUBLISH;              // Message type
    buffer[2] = flags;                        // Flags
    buffer[3] = (topic_id >> 8) & 0xFF;       // Topic ID high byte
    buffer[4] = topic_id & 0xFF;              // Topic ID low byte
    
    int offset = 5;
    if (qos > MQTT_SN_QOS_0) {
        buffer[offset++] = (client->next_msg_id >> 8) & 0xFF;  // Message ID high byte
        buffer[offset++] = client->next_msg_id & 0xFF;         // Message ID low byte
        client->next_msg_id++;
    }
    
    memcpy(&buffer[offset], data, data_len);
    
    printf("Built PUBLISH message: length=%d, topic_id=%d, qos=%d, data_len=%d\n", 
           total_len, topic_id, qos, data_len);
    
    return total_len;
}

int mqtt_sn_build_subscribe(mqtt_sn_client_t *client, const char *topic_name, 
                           uint8_t qos, uint8_t *buffer, uint16_t buffer_len) {
    if (!client || !topic_name || !buffer || buffer_len < 8) {
        return -1;
    }
    
    uint16_t topic_name_len = strlen(topic_name);
    uint16_t total_len = 6 + topic_name_len;
    
    if (total_len > buffer_len) {
        return -1;
    }
    
    uint8_t flags = 0;
    flags |= (qos & 0x03);  // QoS level
    flags |= MQTT_SN_TOPIC_NORMAL;  // Topic ID type
    
    buffer[0] = total_len;                    // Length
    buffer[1] = MQTT_SN_SUBSCRIBE;            // Message type
    buffer[2] = flags;                        // Flags
    buffer[3] = (client->next_msg_id >> 8) & 0xFF;    // Message ID high byte
    buffer[4] = client->next_msg_id & 0xFF;   // Message ID low byte
    memcpy(&buffer[5], topic_name, topic_name_len);
    
    printf("Built SUBSCRIBE message: length=%d, msg_id=%d, qos=%d, topic=%s\n", 
           total_len, client->next_msg_id, qos, topic_name);
    
    client->next_msg_id++;
    
    return total_len;
}

// Message parsing functions
int mqtt_sn_parse_message(const uint8_t *data, uint16_t data_len, mqtt_sn_message_t *msg) {
    if (!data || !msg || data_len < 2) {
        return -1;
    }
    
    msg->length = data[0];
    msg->msg_type = data[1];
    
    if (msg->length != data_len) {
        printf("Message length mismatch: header=%d, actual=%d\n", msg->length, data_len);
        return -1;
    }
    
    if (data_len > 2) {
        msg->data = (uint8_t *)&data[2];
    } else {
        msg->data = NULL;
    }
    
    return 0;
}

int mqtt_sn_handle_connack(mqtt_sn_client_t *client, const uint8_t *data, uint16_t data_len) {
    if (!client || !data || data_len < 3) {
        return -1;
    }
    
    mqtt_sn_connack_t *connack = (mqtt_sn_connack_t *)data;
    uint8_t return_code = connack->return_code;
    
    printf("Received CONNACK: return_code=%d\n", return_code);
    
    // Record acknowledgment for latency measurement
    mqtt_sn_record_message_ack(client, 0, MQTT_SN_CONNECT);
    
    if (client->on_connect) {
        client->on_connect(return_code);
    }
    
    if (return_code == MQTT_SN_ACCEPTED) {
        client->state = MQTT_SN_STATE_CONNECTED;
    } else {
        client->state = MQTT_SN_STATE_DISCONNECTED;
    }
    
    return 0;
}

int mqtt_sn_handle_regack(mqtt_sn_client_t *client, const uint8_t *data, uint16_t data_len) {
    if (!client || !data || data_len < 7) {
        return -1;
    }
    
    mqtt_sn_regack_t *regack = (mqtt_sn_regack_t *)data;
    uint16_t topic_id = (regack->topic_id << 8) | regack->topic_id;
    uint8_t return_code = regack->return_code;
    
    printf("Received REGACK: topic_id=%d, msg_id=%d, return_code=%d\n", 
           topic_id, regack->msg_id, return_code);
    
    // Record acknowledgment for latency measurement
    mqtt_sn_record_message_ack(client, regack->msg_id, MQTT_SN_REGISTER);
    
    if (client->on_register) {
        client->on_register(topic_id, return_code);
    }
    
    if (return_code == MQTT_SN_ACCEPTED) {
        client->state = MQTT_SN_STATE_READY;
        client->next_topic_id++;
    } else {
        client->state = MQTT_SN_STATE_CONNECTED;
    }
    
    return 0;
}

int mqtt_sn_handle_publish(mqtt_sn_client_t *client, const uint8_t *data, uint16_t data_len) {
    if (!client || !data || data_len < 7) {
        return -1;
    }
    
    mqtt_sn_publish_t *publish = (mqtt_sn_publish_t *)data;
    uint16_t topic_id = (publish->topic_id << 8) | publish->topic_id;
    uint8_t qos = publish->flags & 0x03;
    
    // Calculate data offset
    int data_offset = 5;  // length + msg_type + flags + topic_id (2 bytes)
    if (qos > MQTT_SN_QOS_0) {
        data_offset += 2;  // Add message ID for QoS 1/2
    }
    
    if (data_len <= data_offset) {
        printf("Invalid PUBLISH message: too short\n");
        return -1;
    }
    
    uint16_t data_len_actual = data_len - data_offset;
    const uint8_t *payload = &data[data_offset];
    
    printf("Received PUBLISH: topic_id=%d, qos=%d, data_len=%d\n", 
           topic_id, qos, data_len_actual);
    
    if (client->on_publish) {
        client->on_publish(topic_id, payload, data_len_actual);
    }
    
    // Send PUBACK for QoS 1
    if (qos == MQTT_SN_QOS_1) {
        uint8_t puback_buffer[8];
        puback_buffer[0] = 8;  // Length
        puback_buffer[1] = MQTT_SN_PUBACK;  // Message type
        puback_buffer[2] = (topic_id >> 8) & 0xFF;  // Topic ID high byte
        puback_buffer[3] = topic_id & 0xFF;  // Topic ID low byte
        puback_buffer[4] = (publish->msg_id >> 8) & 0xFF;  // Message ID high byte
        puback_buffer[5] = publish->msg_id & 0xFF;  // Message ID low byte
        puback_buffer[6] = MQTT_SN_ACCEPTED;  // Return code
        
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 8, PBUF_RAM);
        if (p) {
            memcpy(p->payload, puback_buffer, 8);
            udp_sendto(client->udp_pcb, p, &client->gateway_addr, client->gateway_port);
            pbuf_free(p);
            printf("Sent PUBACK for topic_id=%d\n", topic_id);
        }
    }
    
    // Record acknowledgment for latency measurement (for QoS 1 PUBLISH)
    if (qos == MQTT_SN_QOS_1) {
        mqtt_sn_record_message_ack(client, publish->msg_id, MQTT_SN_PUBLISH);
    }
    
    return 0;
}

int mqtt_sn_handle_suback(mqtt_sn_client_t *client, const uint8_t *data, uint16_t data_len) {
    if (!client || !data || data_len < 8) {
        return -1;
    }
    
    mqtt_sn_suback_t *suback = (mqtt_sn_suback_t *)data;
    uint16_t topic_id = (suback->topic_id << 8) | suback->topic_id;
    uint8_t return_code = suback->return_code;
    
    printf("Received SUBACK: topic_id=%d, msg_id=%d, return_code=%d\n", 
           topic_id, suback->msg_id, return_code);
    
    // Record acknowledgment for latency measurement
    mqtt_sn_record_message_ack(client, suback->msg_id, MQTT_SN_SUBSCRIBE);
    
    if (client->on_subscribe) {
        client->on_subscribe(topic_id, return_code);
    }
    
    return 0;
}

#include <stdio.h>
#include <string.h>
#include "mqtt_sn_test.h"

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

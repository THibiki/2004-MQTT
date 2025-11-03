#include "mqtt_sn_client.h"
#include "wifi_driver.h"
#include "block_transfer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/mutex.h"

// MQTT-SN message types (MQTT-SN v1.2 specification)
#define MQTTSN_ADVERTISE    0x00
#define MQTTSN_SEARCHGW     0x01
#define MQTTSN_GWINFO       0x02
#define MQTTSN_CONNECT      0x04
#define MQTTSN_CONNACK      0x05
#define MQTTSN_WILLTOPICREQ 0x06
#define MQTTSN_WILLTOPIC    0x07
#define MQTTSN_WILLMSGREQ   0x08
#define MQTTSN_WILLMSG      0x09
#define MQTTSN_REGISTER     0x0A
#define MQTTSN_REGACK       0x0B
#define MQTTSN_PUBLISH      0x0C
#define MQTTSN_PUBACK       0x0D
#define MQTTSN_PUBCOMP      0x0E
#define MQTTSN_PUBREC       0x0F
#define MQTTSN_PUBREL       0x10
#define MQTTSN_SUBSCRIBE    0x12
#define MQTTSN_SUBACK       0x13
#define MQTTSN_UNSUBSCRIBE  0x14
#define MQTTSN_UNSUBACK     0x15
#define MQTTSN_PINGREQ      0x16
#define MQTTSN_PINGRESP     0x17
#define MQTTSN_DISCONNECT   0x18

// MQTT-SN flags
#define MQTTSN_FLAG_DUP           0x80
#define MQTTSN_FLAG_QOS_0         0x00
#define MQTTSN_FLAG_QOS_1         0x20
#define MQTTSN_FLAG_QOS_2         0x40
#define MQTTSN_FLAG_QOS_MASK      0x60
#define MQTTSN_FLAG_RETAIN        0x10
#define MQTTSN_FLAG_WILL          0x08
#define MQTTSN_FLAG_CLEAN_SESSION 0x04
#define MQTTSN_FLAG_TOPIC_ID      0x00  // Topic ID type: normal
#define MQTTSN_FLAG_TOPIC_PRE     0x01  // Topic ID type: pre-defined
#define MQTTSN_FLAG_TOPIC_SHORT   0x02  // Topic ID type: short name
#define MQTTSN_FLAG_TOPIC_MASK    0x03

// Packet queue for incoming messages
#define PACKET_QUEUE_SIZE 16
#define MAX_PACKET_SIZE 256

typedef struct {
    uint8_t data[MAX_PACKET_SIZE];
    size_t length;
    bool used;
} queued_packet_t;

static queued_packet_t packet_queue[PACKET_QUEUE_SIZE];
static volatile int queue_write_idx = 0;
static volatile int queue_read_idx = 0;
static volatile int queue_count = 0;

// Mutex for thread-safe queue access (UDP callback vs main thread)
static mutex_t queue_mutex;
static bool queue_mutex_initialized = false;

// Topic registration mapping
#define MAX_REGISTERED_TOPICS 20

typedef struct {
    char topic_name[64];
    uint16_t topic_id;
    bool registered;
} topic_registration_t;

static topic_registration_t topic_registry[MAX_REGISTERED_TOPICS];
static int topic_registry_count = 0;

// Connection state
static bool connected = false;
static char gateway_host[64];
static uint16_t gateway_port;
static uint16_t msg_id = 1;
static uint16_t keep_alive_duration = 60;
static uint32_t last_ping_time = 0;
static mqttsn_message_callback_t message_callback = NULL;

// Queue management functions
static void queue_init(void) {
    // Initialize mutex for thread-safe queue access
    if (!queue_mutex_initialized) {
        mutex_init(&queue_mutex);
        queue_mutex_initialized = true;
    }
    
    // Clear queue
    mutex_enter_blocking(&queue_mutex);
    for (int i = 0; i < PACKET_QUEUE_SIZE; i++) {
        packet_queue[i].used = false;
        packet_queue[i].length = 0;
    }
    queue_write_idx = 0;
    queue_read_idx = 0;
    queue_count = 0;
    mutex_exit(&queue_mutex);
}

static bool queue_push(const uint8_t *data, size_t len) {
    // Thread-safe push - called from UDP callback (interrupt context)
    if (!queue_mutex_initialized) {
        return false; // Mutex not ready
    }
    
    bool result = false;
    mutex_enter_blocking(&queue_mutex);
    
    if (queue_count < PACKET_QUEUE_SIZE && len <= MAX_PACKET_SIZE) {
        memcpy(packet_queue[queue_write_idx].data, data, len);
        packet_queue[queue_write_idx].length = len;
        packet_queue[queue_write_idx].used = true;
        
        queue_write_idx = (queue_write_idx + 1) % PACKET_QUEUE_SIZE;
        queue_count++;
        result = true;
    }
    
    mutex_exit(&queue_mutex);
    return result;
}

static bool queue_pop(uint8_t *buffer, size_t max_len, size_t *out_len) {
    // Thread-safe pop - called from main thread
    if (!queue_mutex_initialized) {
        return false; // Mutex not ready
    }
    
    bool result = false;
    mutex_enter_blocking(&queue_mutex);
    
    if (queue_count > 0) {
        if (packet_queue[queue_read_idx].length <= max_len) {
            memcpy(buffer, packet_queue[queue_read_idx].data, packet_queue[queue_read_idx].length);
            *out_len = packet_queue[queue_read_idx].length;
            packet_queue[queue_read_idx].used = false;
            
            queue_read_idx = (queue_read_idx + 1) % PACKET_QUEUE_SIZE;
            queue_count--;
            result = true;
        }
    }
    
    mutex_exit(&queue_mutex);
    return result;
}

// Topic registry helper functions
static void topic_registry_init(void) {
    for (int i = 0; i < MAX_REGISTERED_TOPICS; i++) {
        topic_registry[i].topic_name[0] = '\0';
        topic_registry[i].topic_id = 0;
        topic_registry[i].registered = false;
    }
    topic_registry_count = 0;
}

static uint16_t topic_registry_find(const char *topic) {
    for (int i = 0; i < topic_registry_count; i++) {
        if (topic_registry[i].registered && 
            strcmp(topic_registry[i].topic_name, topic) == 0) {
            return topic_registry[i].topic_id;
        }
    }
    return 0; // Not found
}

static bool topic_registry_add(const char *topic, uint16_t topic_id) {
    if (topic_registry_count >= MAX_REGISTERED_TOPICS) {
        return false;
    }
    
    // Check if already exists
    for (int i = 0; i < topic_registry_count; i++) {
        if (strcmp(topic_registry[i].topic_name, topic) == 0) {
            topic_registry[i].topic_id = topic_id;
            topic_registry[i].registered = true;
            return true;
        }
    }
    
    // Add new entry
    strncpy(topic_registry[topic_registry_count].topic_name, topic, 63);
    topic_registry[topic_registry_count].topic_name[63] = '\0';
    topic_registry[topic_registry_count].topic_id = topic_id;
    topic_registry[topic_registry_count].registered = true;
    topic_registry_count++;
    
    return true;
}

static const char* topic_registry_get_name(uint16_t topic_id) {
    for (int i = 0; i < topic_registry_count; i++) {
        if (topic_registry[i].registered && topic_registry[i].topic_id == topic_id) {
            return topic_registry[i].topic_name;
        }
    }
    return NULL;
}

// Helper function to send MQTT-SN packet
static int send_packet(const uint8_t *data, size_t len) {
    return wifi_udp_send(gateway_host, gateway_port, data, len);
}

// Helper function to receive MQTT-SN packet with queue support
static int receive_packet(uint8_t *buffer, size_t max_len, uint32_t timeout_ms) {
    // First check if there's a queued packet
    size_t queued_len;
    if (queue_pop(buffer, max_len, &queued_len)) {
        return queued_len;
    }
    
    // No queued packet - poll the network
    uint8_t temp_buffer[MAX_PACKET_SIZE];
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    
    while (true) {
        int len = wifi_udp_receive(temp_buffer, sizeof(temp_buffer), 0); // Non-blocking
        
        if (len > 0) {
            // Got a packet - return it directly if it fits
            if (len <= max_len) {
                memcpy(buffer, temp_buffer, len);
                return len;
            } else {
                return -1; // Buffer too small
            }
        }
        
        // Check timeout
        if (timeout_ms > 0) {
            uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start_time;
            if (elapsed >= timeout_ms) {
                return WIFI_ETIMEDOUT; // Timeout
            }
        } else {
            return 0; // Non-blocking mode, no data available
        }
        
        // Small delay to avoid busy-waiting
        sleep_ms(10);
    }
}

// Background packet receiver - should be called periodically to fill queue
static void receive_background_packets(void) {
    uint8_t temp_buffer[MAX_PACKET_SIZE];
    
    // Non-blocking poll for packets
    int len = wifi_udp_receive(temp_buffer, sizeof(temp_buffer), 0);
    
    if (len > 0 && len <= MAX_PACKET_SIZE) {
        // Queue the packet for later processing
        if (!queue_push(temp_buffer, len)) {
            printf("Warning: Packet queue full, dropping packet\n");
        }
    }
}

// Wait for a specific message type, optionally matching message ID
static int wait_for_message(uint8_t expected_type, uint16_t expected_msg_id, bool match_msg_id,
                           uint8_t *buffer, size_t max_len, uint32_t timeout_ms) {
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    uint8_t temp_buffer[MAX_PACKET_SIZE];
    
    while (true) {
        // Receive any packet (from queue or network)
        int len = receive_packet(temp_buffer, sizeof(temp_buffer), 100);
        
        if (len > 2) {
            uint8_t msg_type = temp_buffer[1];
            
            // Check if this is the message we're waiting for
            if (msg_type == expected_type) {
                bool msg_id_matches = true;
                
                if (match_msg_id) {
                    if (expected_type == MQTTSN_PUBACK && len >= 7) {
                        // PUBACK: msg_id at bytes [4:5]
                        uint16_t msg_id = (temp_buffer[4] << 8) | temp_buffer[5];
                        msg_id_matches = (msg_id == expected_msg_id);
                    } else if (expected_type == MQTTSN_REGACK && len >= 7) {
                        // REGACK: msg_id at bytes [4:5]
                        uint16_t msg_id = (temp_buffer[4] << 8) | temp_buffer[5];
                        msg_id_matches = (msg_id == expected_msg_id);
                    } else if (expected_type == MQTTSN_SUBACK && len >= 8) {
                        // SUBACK: msg_id at bytes [5:6]
                        uint16_t msg_id = (temp_buffer[5] << 8) | temp_buffer[6];
                        msg_id_matches = (msg_id == expected_msg_id);
                    }
                }
                
                if (msg_id_matches) {
                    // Found the message we're waiting for
                    if (len <= max_len) {
                        memcpy(buffer, temp_buffer, len);
                        return len;
                    } else {
                        return -1; // Buffer too small
                    }
                }
            } else if (msg_type == MQTTSN_PINGRESP) {
                // Always handle PINGRESP immediately
                if (expected_type == MQTTSN_PINGRESP) {
                    if (len <= max_len) {
                        memcpy(buffer, temp_buffer, len);
                        return len;
                    }
                }
                // Otherwise just ignore it
            } else if (msg_type == MQTTSN_PUBLISH) {
                // Handle incoming PUBLISH messages immediately (even while waiting)
                // This ensures we don't miss incoming messages during QoS 1 PUBACK wait
                size_t pos = 2;
                uint8_t flags = temp_buffer[pos++];
                uint8_t topic_id_type = flags & MQTTSN_FLAG_TOPIC_MASK;
                
                char topic[64] = {0};
                
                if (topic_id_type == MQTTSN_FLAG_TOPIC_ID) {
                    // Normal topic ID (2 bytes)
                    uint16_t topic_id = (temp_buffer[pos] << 8) | temp_buffer[pos + 1];
                    pos += 2;
                    
                    // Look up topic name from registry
                    const char *topic_name = topic_registry_get_name(topic_id);
                    if (topic_name) {
                        strncpy(topic, topic_name, sizeof(topic) - 1);
                    } else {
                        snprintf(topic, sizeof(topic), "unknown/%d", topic_id);
                    }
                } else {
                    // Short or pre-defined - skip for now
                    pos += 2;
                }
                
                // Skip message ID if QoS > 0
                if (flags & (MQTTSN_FLAG_QOS_1 | MQTTSN_FLAG_QOS_2)) {
                    pos += 2;
                }
                
                // Extract payload and call callback
                if (pos < len && message_callback && topic[0] != '\0') {
                    const uint8_t *data = &temp_buffer[pos];
                    size_t data_len = len - pos;
                    message_callback(topic, data, data_len);
                }
            } else if (msg_type == MQTTSN_REGISTER) {
                // Handle REGISTER from gateway
                if (len >= 7) {
                    uint16_t topic_id = (temp_buffer[2] << 8) | temp_buffer[3];
                    uint16_t reg_msg_id = (temp_buffer[4] << 8) | temp_buffer[5];
                    
                    // Extract topic name
                    char topic_name[64] = {0};
                    size_t topic_len = len - 6;
                    if (topic_len > 63) topic_len = 63;
                    memcpy(topic_name, &temp_buffer[6], topic_len);
                    topic_name[topic_len] = '\0';
                    
                    // Store mapping
                    topic_registry_add(topic_name, topic_id);
                    
                    // Send REGACK
                    uint8_t regack[7];
                    regack[0] = 7;
                    regack[1] = MQTTSN_REGACK;
                    regack[2] = (topic_id >> 8) & 0xFF;
                    regack[3] = topic_id & 0xFF;
                    regack[4] = (reg_msg_id >> 8) & 0xFF;
                    regack[5] = reg_msg_id & 0xFF;
                    regack[6] = 0x00;  // Return code: accepted
                    send_packet(regack, 7);
                }
            } else {
                // Not the message we want - re-queue it for later
                if (!queue_push(temp_buffer, len)) {
                    printf("Warning: Can't re-queue packet, queue full\n");
                }
            }
        }
        
        // Check timeout
        uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start_time;
        if (elapsed >= timeout_ms) {
            return WIFI_ETIMEDOUT;
        }
        
        // Small delay
        sleep_ms(10);
    }
}

int mqttsn_init(const char *gateway_host_param, uint16_t gateway_port_param) {
    strncpy(gateway_host, gateway_host_param, sizeof(gateway_host) - 1);
    gateway_host[sizeof(gateway_host) - 1] = '\0';
    gateway_port = gateway_port_param;
    connected = false;
    msg_id = 1;
    last_ping_time = 0;
    
    // Initialize packet queue
    queue_init();
    
    // Initialize topic registry
    topic_registry_init();
    
    printf("MQTT-SN initialized - Gateway: %s:%d\n", gateway_host, gateway_port);
    return MQTTSN_OK;
}

int mqttsn_connect(const char *client_id, uint16_t keep_alive) {
    if (connected) {
        return MQTTSN_OK;
    }
    
    keep_alive_duration = keep_alive;
    last_ping_time = to_ms_since_boot(get_absolute_time());
    
    printf("Connecting to MQTT-SN gateway %s:%d...\n", gateway_host, gateway_port);
    printf("Client ID: %s, Keep-alive: %d seconds\n", client_id, keep_alive);
    
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
    
    // Wait specifically for CONNACK message
    uint8_t response[32];
    int len = wait_for_message(MQTTSN_CONNACK, 0, false, response, sizeof(response), 5000);
    
    printf("Received response: %d bytes\n", len);
    if (len > 0) {
        printf("Response hex: ");
        for (int i = 0; i < len && i < 20; i++) {
            printf("%02X ", response[i]);
        }
        printf("\n");
    }
    
    if (len < 3) {
        printf("❌ Response too short or timeout (got %d)\n", len);
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

int mqttsn_register_topic(const char *topic) {
    if (!connected) {
        return MQTTSN_NOT_CONNECTED;
    }
    
    // Check if already registered
    uint16_t existing_id = topic_registry_find(topic);
    if (existing_id != 0) {
        printf("Topic '%s' already registered with ID %d\n", topic, existing_id);
        return existing_id;
    }
    
    printf("Registering topic: %s\n", topic);
    
    // Build REGISTER packet
    uint8_t packet[64];
    size_t pos = 0;
    
    // Length (will be filled later)
    packet[pos++] = 0;
    
    // Message type
    packet[pos++] = MQTTSN_REGISTER;
    
    // Topic ID (0x0000 for registration from client)
    packet[pos++] = 0x00;
    packet[pos++] = 0x00;
    
    // Message ID
    uint16_t current_msg_id = msg_id++;
    packet[pos++] = (current_msg_id >> 8) & 0xFF;
    packet[pos++] = current_msg_id & 0xFF;
    
    // Topic name
    size_t topic_len = strlen(topic);
    memcpy(&packet[pos], topic, topic_len);
    pos += topic_len;
    
    // Set length
    packet[0] = pos;
    
    // Send REGISTER
    if (send_packet(packet, pos) != WIFI_OK) {
        printf("Failed to send REGISTER packet\n");
        return MQTTSN_ERROR;
    }
    
    // Wait for REGACK
    uint8_t response[32];
    int len = wait_for_message(MQTTSN_REGACK, current_msg_id, true, response, sizeof(response), 3000);
    
    if (len < 7) {
        printf("No REGACK received or timeout\n");
        return MQTTSN_TIMEOUT;
    }
    
    // Parse REGACK: [Length][MsgType][TopicID(2)][MsgID(2)][ReturnCode]
    uint16_t assigned_topic_id = (response[2] << 8) | response[3];
    uint8_t return_code = response[6];
    
    if (return_code == 0) {
        // Success - store mapping
        topic_registry_add(topic, assigned_topic_id);
        printf("Topic '%s' registered with ID %d\n", topic, assigned_topic_id);
        return assigned_topic_id;
    } else {
        printf("Topic registration failed with code: %d\n", return_code);
        return MQTTSN_ERROR;
    }
}

int mqttsn_send_pingreq(void) {
    if (!connected) {
        return MQTTSN_NOT_CONNECTED;
    }
    
    // Build PINGREQ packet (just length + message type)
    uint8_t packet[2] = {2, MQTTSN_PINGREQ};
    
    if (send_packet(packet, 2) != WIFI_OK) {
        return MQTTSN_ERROR;
    }
    
    last_ping_time = to_ms_since_boot(get_absolute_time());
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
    
    // Flags - Use topic name type for SUBSCRIBE
    uint8_t flags = MQTTSN_FLAG_TOPIC_ID;  // Normal topic name
    if (qos == QOS_1) flags |= MQTTSN_FLAG_QOS_1;
    else if (qos == QOS_2) flags |= MQTTSN_FLAG_QOS_2;
    packet[pos++] = flags;
    
    // Message ID
    uint16_t current_msg_id = msg_id++;
    packet[pos++] = (current_msg_id >> 8) & 0xFF;
    packet[pos++] = current_msg_id & 0xFF;
    
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
    
    // Wait for SUBACK with message ID matching
    uint8_t response[32];
    int len = wait_for_message(MQTTSN_SUBACK, current_msg_id, true, response, sizeof(response), 5000);
    
    if (len < 8) {
        printf("No SUBACK received or timeout\n");
        return MQTTSN_TIMEOUT;
    }
    
    // Parse SUBACK: [Length][MsgType][Flags][TopicID(2)][MsgID(2)][ReturnCode]
    uint16_t topic_id = (response[3] << 8) | response[4];
    uint8_t return_code = response[7];
    
    if (return_code == 0) {
        // Store the topic mapping from SUBACK
        topic_registry_add(topic, topic_id);
        printf("Subscribed to '%s' successfully (Topic ID: %d)\n", topic, topic_id);
        return MQTTSN_OK;
    } else {
        printf("Subscription failed with code: %d\n", return_code);
        return MQTTSN_ERROR;
    }
}

int mqttsn_publish(const char *topic, const uint8_t *data, size_t len, mqttsn_qos_t qos) {
    if (!connected) {
        return MQTTSN_NOT_CONNECTED;
    }
    
    // Try to find topic ID - if not found, register it first
    uint16_t topic_id = topic_registry_find(topic);
    if (topic_id == 0) {
        printf("Topic '%s' not registered, registering now...\n", topic);
        int result = mqttsn_register_topic(topic);
        if (result <= 0) {
            printf("Failed to register topic for publishing\n");
            return MQTTSN_ERROR;
        }
        topic_id = result;
    }
    
    // Build PUBLISH packet with Topic ID
    uint8_t packet[256];
    size_t pos = 0;
    
    // Length (will be filled later)
    packet[pos++] = 0;
    
    // Message type
    packet[pos++] = MQTTSN_PUBLISH;
    
    // Flags - Use normal topic ID type
    uint8_t flags = MQTTSN_FLAG_TOPIC_ID;  // Topic ID type = 0 (normal)
    if (qos == QOS_1) flags |= MQTTSN_FLAG_QOS_1;
    else if (qos == QOS_2) flags |= MQTTSN_FLAG_QOS_2;
    packet[pos++] = flags;
    
    // Topic ID (2 bytes)
    packet[pos++] = (topic_id >> 8) & 0xFF;
    packet[pos++] = topic_id & 0xFF;
    
    // Message ID (for QoS > 0)
    uint16_t current_msg_id = 0;
    if (qos > QOS_0) {
        current_msg_id = msg_id++;
        if (msg_id == 0) msg_id = 1;  // Wrap around, avoid 0
        packet[pos++] = (current_msg_id >> 8) & 0xFF;
        packet[pos++] = current_msg_id & 0xFF;
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
    if (send_packet(packet, pos) != WIFI_OK) {
        return MQTTSN_ERROR;
    }
    
    // For QoS 1, wait for PUBACK
    if (qos == QOS_1) {
        uint8_t ack_buffer[16];
        int ack_len = wait_for_message(MQTTSN_PUBACK, current_msg_id, true, ack_buffer, sizeof(ack_buffer), 1000);
        
        if (ack_len < 0) {
            // Timeout or error
            return MQTTSN_ERROR;
        }
        
        // PUBACK received successfully
        return MQTTSN_OK;
    }
    
    return MQTTSN_OK;
}

int mqttsn_poll(void) {
    if (!connected) {
        return MQTTSN_NOT_CONNECTED;
    }
    
    // Auto send PINGREQ if needed (keep-alive management)
    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t ping_interval_ms = (keep_alive_duration * 1000) / 2;  // Send at half the keep-alive interval
    if (now - last_ping_time > ping_interval_ms) {
        mqttsn_send_pingreq();
    }
    
    uint8_t buffer[256];
    int len = receive_packet(buffer, sizeof(buffer), 0); // Non-blocking
    
    if (len > 2) {
        uint8_t msg_type = buffer[1];
        
        if (msg_type == MQTTSN_PUBLISH) {
            // Parse PUBLISH message with Topic ID
            if (len < 7) return MQTTSN_OK; // Malformed packet
            
            uint8_t flags = buffer[2];
            uint8_t topic_id_type = flags & MQTTSN_FLAG_TOPIC_MASK;
            size_t pos = 3;
            
            char topic[64] = {0};
            
            if (topic_id_type == MQTTSN_FLAG_TOPIC_ID) {
                // Normal topic ID (2 bytes)
                uint16_t topic_id = (buffer[pos] << 8) | buffer[pos + 1];
                pos += 2;
                
                // Look up topic name from registry
                const char *topic_name = topic_registry_get_name(topic_id);
                if (topic_name) {
                    strncpy(topic, topic_name, sizeof(topic) - 1);
                } else {
                    snprintf(topic, sizeof(topic), "unknown/%d", topic_id);
                }
            } else if (topic_id_type == MQTTSN_FLAG_TOPIC_SHORT) {
                // Short topic name (2 chars)
                topic[0] = buffer[pos++];
                topic[1] = buffer[pos++];
                topic[2] = '\0';
            } else {
                // Pre-defined topic ID - treat same as normal
                uint16_t topic_id = (buffer[pos] << 8) | buffer[pos + 1];
                pos += 2;
                snprintf(topic, sizeof(topic), "predefined/%d", topic_id);
            }
            
            // Message ID if QoS > 0
            if (flags & (MQTTSN_FLAG_QOS_1 | MQTTSN_FLAG_QOS_2)) {
                pos += 2;
            }
            
            // Extract payload
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
        else if (msg_type == MQTTSN_REGISTER) {
            // Gateway is registering a topic with us
            if (len < 7) return MQTTSN_OK;
            
            uint16_t topic_id = (buffer[2] << 8) | buffer[3];
            uint16_t msg_id = (buffer[4] << 8) | buffer[5];
            
            // Extract topic name
            char topic_name[64] = {0};
            size_t topic_len = len - 6;
            if (topic_len > 63) topic_len = 63;
            memcpy(topic_name, &buffer[6], topic_len);
            topic_name[topic_len] = '\0';
            
            // Store mapping
            topic_registry_add(topic_name, topic_id);
            printf("Gateway registered topic '%s' with ID %d\n", topic_name, topic_id);
            
            // Send REGACK
            uint8_t regack[7];
            regack[0] = 7;
            regack[1] = MQTTSN_REGACK;
            regack[2] = (topic_id >> 8) & 0xFF;
            regack[3] = topic_id & 0xFF;
            regack[4] = (msg_id >> 8) & 0xFF;
            regack[5] = msg_id & 0xFF;
            regack[6] = 0x00;  // Return code: accepted
            send_packet(regack, 7);
        }
        else if (msg_type == MQTTSN_PINGRESP) {
            // Gateway responded to our ping - connection is alive
            // No action needed, just confirms we're connected
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
// mqttsn_client.c
// Lightweight MQTT-SN client harness that uses mqttsn_adapter.
// This file does not require the Paho library to compile. If Paho is
// detected at build time (CMake defines HAVE_PAHO), the example will
// include the Paho headers and show where to call them.

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "protocol/mqttsn/mqttsn_adapter.h"
#include "config/network_config.h"
#include "drivers/block_transfer.h"

#ifdef HAVE_PAHO
#include "MQTTSNPacket.h"
#include "MQTTSNConnect.h"
#include "MQTTSNPublish.h"
#include "MQTTSNSubscribe.h"
#include "MQTTSNSearch.h"
#endif

static bool mqttsn_initialized = false;
static bool mqttsn_connected = false;
static unsigned short mqttsn_registered_topicid = 0;  // For pico/test
static unsigned short mqttsn_chunks_topicid = 0;      // For pico/chunks
static unsigned short mqttsn_retransmit_topicid = 0;  // For pico/retransmit
static unsigned short mqttsn_msg_id = 1;
static int current_qos = 0;  // Default to QoS 0

// Get current QoS level
int mqttsn_get_qos(void) {
    return current_qos;
}

// Set QoS level (0, 1, or 2)
void mqttsn_set_qos(int qos) {
    if (qos >= 0 && qos <= 2) {
        current_qos = qos;
        printf("[MQTTSN] QoS level set to %d\n", qos);
    } else {
        printf("[MQTTSN] Invalid QoS level %d (must be 0, 1, or 2)\n", qos);
    }
}

int mqttsn_demo_init(uint16_t local_port, const char *client_id){
    int rc = mqttsn_transport_open(local_port);
    if (rc != 0){
        printf("[MQTTSN] Transport open failed: %d\n", rc);
        return -1;
    }

#ifdef HAVE_PAHO
    // Step 1: Build and send CONNECT packet
    unsigned char buf[256];
    MQTTSNPacket_connectData options = MQTTSNPacket_connectData_initializer;
    
    options.clientID.cstring = (client_id != NULL) ? (char*)client_id : "pico_w_client";
    
    options.duration = 60; // keepalive in seconds
    options.cleansession = 1;

    int len = MQTTSNSerialize_connect(buf, sizeof(buf), &options);
    if (len <= 0) {
        printf("[MQTTSN] Failed to serialize CONNECT (rc=%d)\n", len);
        return -1;
    }
    
    printf("[MQTTSN] Sending CONNECT (%d bytes)...\n", len);
    printf("[DEBUG] CONNECT packet: ");
    for(int i = 0; i < len; i++) {
        printf("%02x ", buf[i]);
    }
    printf("\n");

    int s = mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, buf, len);
    if (s != 0) {
        printf("[MQTTSN] CONNECT send failed (err=%d)\n", s);
        return -2;
    }

    // Step 2: Wait for CONNACK
    printf("[MQTTSN] Waiting for CONNACK...\n");
    int r = mqttsn_transport_receive(buf, sizeof(buf), 5000);
    if (r > 0) {
        printf("[DEBUG] Received %d bytes: ", r);
        for(int i = 0; i < r && i < 20; i++) {
            printf("%02x ", buf[i]);
        }
        printf("\n");

        int connack_rc = -1;
        int d = MQTTSNDeserialize_connack(&connack_rc, buf, r);
        if (d == 1) {
            if (connack_rc == MQTTSN_RC_ACCEPTED) {
                printf("[MQTTSN] ✓ CONNECT accepted (CONNACK received)\n");
                mqttsn_connected = true;
            } else {
                printf("[MQTTSN] ✗ CONNECT rejected (code=%d)\n", connack_rc);
                return -3;
            }
        } else {
            printf("[MQTTSN] ✗ Failed to parse CONNACK\n");
            return -4;
        }
    } else {
        printf("[MQTTSN] ✗ CONNACK not received (rc=%d)\n", r);
        return -5;
    }

    // Step 3: Register the default topic
    const char *default_topic = "pico/test";
    printf("[MQTTSN] Registering topic '%s'...\n", default_topic);
    
    unsigned short topicid = 0;  // 0 means we want the gateway to assign an ID
    
    // MQTTSNSerialize_register(buf, buflen, topicid, msgid, topicname)
    // The last parameter should be MQTTSNString, not MQTTSN_topicid
    MQTTSNString topic_string = MQTTSNString_initializer;
    topic_string.cstring = (char*)default_topic;
    topic_string.lenstring.len = strlen(default_topic);
    
    len = MQTTSNSerialize_register(buf, sizeof(buf), topicid, mqttsn_msg_id, &topic_string);
    if (len <= 0) {
        printf("[MQTTSN] Failed to serialize REGISTER (rc=%d)\n", len);
        printf("[DEBUG] Buffer size: %zu, topic length: %zu, msgid: %u\n", 
               sizeof(buf), strlen(default_topic), mqttsn_msg_id);
        return -6;
    }

    printf("[DEBUG] REGISTER packet (%d bytes): ", len);
    for(int i = 0; i < len; i++) {
        printf("%02x ", buf[i]);
    }
    printf("\n");

    s = mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, buf, len);
    if (s != 0) {
        printf("[MQTTSN] REGISTER send failed (err=%d)\n", s);
        return -7;
    }

    // Step 4: Wait for REGACK
    printf("[MQTTSN] Waiting for REGACK...\n");
    r = mqttsn_transport_receive(buf, sizeof(buf), 5000);
    if (r > 0) {
        printf("[DEBUG] Received %d bytes: ", r);
        for(int i = 0; i < r && i < 20; i++) {
            printf("%02x ", buf[i]);
        }
        printf("\n");

        unsigned short returned_topicid = 0;
        unsigned short returned_msgid = 0;
        unsigned char return_code = 0;

        int d = MQTTSNDeserialize_regack(&returned_topicid, &returned_msgid, &return_code, buf, r);
        if (d == 1) {
            if (return_code == MQTTSN_RC_ACCEPTED) {
                mqttsn_registered_topicid = returned_topicid;
                printf("[MQTTSN] ✓ Topic registered (TopicID=%u, MsgID=%u)\n", 
                       returned_topicid, returned_msgid);
            } else {
                printf("[MQTTSN] ✗ Topic registration rejected (code=%d)\n", return_code);
                return -8;
            }
        } else {
            printf("[MQTTSN] ✗ Failed to parse REGACK\n");
            return -9;
        }
    } else {
        printf("[MQTTSN] ✗ REGACK not received (rc=%d)\n", r);
        return -10;
    }

    mqttsn_msg_id++;
    
    // Also register pico/chunks topic for block transfers
    printf("[MQTTSN] Registering topic 'pico/chunks' for block transfers...\n");
    const char *chunks_topic = "pico/chunks";
    MQTTSNString chunks_topic_string = MQTTSNString_initializer;
    chunks_topic_string.cstring = (char*)chunks_topic;
    chunks_topic_string.lenstring.len = strlen(chunks_topic);
    
    len = MQTTSNSerialize_register(buf, sizeof(buf), 0, mqttsn_msg_id, &chunks_topic_string);
    if (len > 0) {
        s = mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, buf, len);
        if (s == 0) {
            r = mqttsn_transport_receive(buf, sizeof(buf), 5000);
            if (r > 0) {
                unsigned short chunks_topicid = 0;
                unsigned short chunks_msgid = 0;
                unsigned char chunks_rc = 0;
                if (MQTTSNDeserialize_regack(&chunks_topicid, &chunks_msgid, &chunks_rc, buf, r) == 1) {
                    if (chunks_rc == MQTTSN_RC_ACCEPTED) {
                        mqttsn_chunks_topicid = chunks_topicid;
                        printf("[MQTTSN] ✓ Topic 'pico/chunks' registered (TopicID=%u)\n", chunks_topicid);
                        mqttsn_msg_id++;
                    } else {
                        printf("[MQTTSN] ⚠ Topic 'pico/chunks' registration rejected (code=%d)\n", chunks_rc);
                    }
                }
            }
        }
    }
    
    // Also register pico/retransmit topic for retransmission requests
    printf("[MQTTSN] Registering topic 'pico/retransmit' for retransmission...\n");
    const char *retx_topic = "pico/retransmit";
    MQTTSNString retx_topic_string = MQTTSNString_initializer;
    retx_topic_string.cstring = (char*)retx_topic;
    retx_topic_string.lenstring.len = strlen(retx_topic);
    
    len = MQTTSNSerialize_register(buf, sizeof(buf), 0, mqttsn_msg_id, &retx_topic_string);
    if (len > 0) {
        s = mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, buf, len);
        if (s == 0) {
            r = mqttsn_transport_receive(buf, sizeof(buf), 5000);
            if (r > 0) {
                unsigned short retx_topicid = 0;
                unsigned short retx_msgid = 0;
                unsigned char retx_rc = 0;
                if (MQTTSNDeserialize_regack(&retx_topicid, &retx_msgid, &retx_rc, buf, r) == 1) {
                    if (retx_rc == MQTTSN_RC_ACCEPTED) {
                        mqttsn_retransmit_topicid = retx_topicid;
                        printf("[MQTTSN] ✓ Topic 'pico/retransmit' registered (TopicID=%u)\n", retx_topicid);
                        mqttsn_msg_id++;
                    } else {
                        printf("[MQTTSN] ⚠ Topic 'pico/retransmit' registration rejected (code=%d)\n", retx_rc);
                    }
                }
            }
        }
    }
#else
    printf("[MQTTSN] Paho not available at build time\n");
#endif

    mqttsn_initialized = true;
    printf("[MQTTSN] ✓✓✓ Initialization complete - ready to publish ✓✓✓\n");
    return 0;
}

// Send a small test payload to the gateway and print timing/result.
int mqttsn_demo_send_test(const char *payload){
    if (!mqttsn_initialized){
        printf("[MQTTSN] Not initialized\n");
        return -1;
    }

    size_t len = strlen(payload);
    uint32_t t0 = mqttsn_get_time_ms();
    int rc = mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, (const uint8_t*)payload, len);
    uint32_t t1 = mqttsn_get_time_ms();

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
    if (!mqttsn_initialized) {
        printf("[MQTTSN] ✗ Cannot publish - not initialized\n");
        return -1;
    }
    if (!mqttsn_connected) {
        printf("[MQTTSN] ✗ Cannot publish - not connected\n");
        return -2;
    }

    // Print payload
    printf("[PUBLISHER] Payload (%d bytes): %.*s\n", payloadlen, payloadlen, (const char*)payload);
    
    // Select appropriate topic ID based on topic name
    unsigned short topic_id_to_use = 0;
    if (strcmp(topicname, "pico/chunks") == 0) {
        topic_id_to_use = mqttsn_chunks_topicid;
    } else if (strcmp(topicname, "pico/retransmit") == 0) {
        topic_id_to_use = mqttsn_retransmit_topicid;
    } else if (strcmp(topicname, "pico/test") == 0 || strcmp(topicname, "pico/block") == 0) {
        topic_id_to_use = mqttsn_registered_topicid;
    } else {
        // Default to registered topic ID
        topic_id_to_use = mqttsn_registered_topicid;
    }
    
    if (topic_id_to_use == 0) {
        printf("[MQTTSN] ✗ Cannot publish to '%s' - topic not registered\n", topicname);
        return -3;
    }

    unsigned char buf[512];
    MQTTSN_topicid topic;
    
    topic.type = MQTTSN_TOPIC_TYPE_NORMAL;  
    topic.data.id = topic_id_to_use;

    // For QoS 0, MsgId = 0; for QoS 1 and 2, use sequential message ID
    unsigned short msgid = (current_qos == 0) ? 0 : mqttsn_msg_id;

    int len = MQTTSNSerialize_publish(buf, sizeof(buf), 
                                       0,           // dup = 0
                                       current_qos, // qos = current QoS level
                                       0,           // retained = 0
                                       msgid, 
                                       topic, 
                                       (unsigned char*)payload, 
                                       payloadlen);
    if (len <= 0) {
        printf("[MQTTSN] Failed to serialize PUBLISH (rc=%d)\n", len);
        return -4;
    }

    printf("[DEBUG] PUBLISH packet (%d bytes, QoS=%d): ", len, current_qos);
    for(int i = 0; i < len && i < 30; i++) {
        printf("%02x ", buf[i]);
    }
    printf("...\n");

    int s = mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, buf, len);
    if (s != 0) {
        printf("[MQTTSN] PUBLISH send failed (err=%d)\n", s);
        return -5;
    }
    
    if (current_qos == 0) {
        // QoS 0: Fire and forget - no acknowledgment, returns success immediately
        // WARNING: This does NOT guarantee delivery - packets may be lost
        printf("[MQTTSN] ✓ PUBLISH sent (QoS 0, no ACK) to '%s' (TopicID=%u, len=%d)\n", 
               topicname, topic_id_to_use, payloadlen);
        return 0;  // QoS 0 returns immediately without waiting
    }
    
    printf("[MQTTSN] ✓ PUBLISH sent to '%s' (TopicID=%u, MsgID=%u, QoS=%d, len=%d)\n", 
           topicname, topic_id_to_use, msgid, current_qos, payloadlen);
    
    // Wait for acknowledgment for QoS 1 and 2
    if (current_qos == 1) {
        // Wait for PUBACK, but process other messages too
        printf("[MQTTSN] Waiting for PUBACK (QoS 1)...\n");
        bool puback_received = false;
        uint32_t wait_start = to_ms_since_boot(get_absolute_time());
        
        while (!puback_received && (to_ms_since_boot(get_absolute_time()) - wait_start) < 5000) {
            int r = mqttsn_transport_receive(buf, sizeof(buf), 100);
            if (r <= 0) {
                continue;  // No message, keep waiting
            }
            
            printf("[DEBUG] Received %d bytes: ", r);
            for(int i = 0; i < r && i < 20; i++) {
                printf("%02x ", buf[i]);
            }
            printf("\n");
            
            if (r < 2) continue;
            uint8_t msg_type = buf[1];
            
            // PUBACK format: [Length][0x0D][TopicId MSB][TopicId LSB][MsgId MSB][MsgId LSB][ReturnCode]
            if (r >= 7 && msg_type == 0x0D) {  // 0x0D = PUBACK
                unsigned short ack_topicid = (buf[2] << 8) | buf[3];
                unsigned short ack_msgid = (buf[4] << 8) | buf[5];
                unsigned char return_code = buf[6];
                
                if (return_code == 0x00) {
                    printf("[MQTTSN] ✓ PUBACK received (TopicID=%u, MsgID=%u)\n", 
                           ack_topicid, ack_msgid);
                    puback_received = true;
                } else {
                    printf("[MQTTSN] ✗ PUBACK with error code=%d\n", return_code);
                    return -6;
                }
            } else if (msg_type == 0x0C) {  // PUBLISH - forward to main loop handler
                printf("[MQTTSN] ✓ Received PUBLISH while waiting for PUBACK - processing...\n");
                
                #ifdef HAVE_PAHO
                unsigned char dup, retained;
                unsigned short recv_msgid;
                int qos;
                MQTTSN_topicid topic;
                unsigned char *payload_ptr;
                int payloadlen_recv;
                
                if (MQTTSNDeserialize_publish(&dup, &qos, &retained, &recv_msgid, 
                                             &topic, &payload_ptr, &payloadlen_recv, 
                                             buf, r) == 1) {
                    printf("[MQTTSN] PUBLISH decoded: TopicID=%u, QoS=%d, PayloadLen=%d\n",
                           topic.data.id, qos, payloadlen_recv);
                    
                    // Check if this is a RETX request
                    if (payloadlen_recv >= 5 && strncmp((char*)payload_ptr, "RETX:", 5) == 0) {
                        printf("[PUBLISHER] 📩 RETX request during publish wait!\n");
                        printf("[PUBLISHER] Payload: %.*s\n", payloadlen_recv, payload_ptr);
                        
                        char request_msg[256];
                        int copy_len = (payloadlen_recv < 255) ? payloadlen_recv : 255;
                        memcpy(request_msg, payload_ptr, copy_len);
                        request_msg[copy_len] = '\0';
                        
                        block_transfer_handle_retransmit_request(request_msg);
                    }
                }
                #endif
            } else {
                printf("[MQTTSN] Received message type 0x%02X while waiting for PUBACK\n", msg_type);
            }
        }
        
        if (!puback_received) {
            printf("[MQTTSN] ✗ PUBACK not received (timeout)\n");
            return -7;
        }
        
        mqttsn_msg_id++;  // Increment only after acknowledgment
        
    } else if (current_qos == 2) {
        // QoS 2: Wait for PUBREC
        printf("[MQTTSN] Waiting for PUBREC (QoS 2)...\n");
        int r = mqttsn_transport_receive(buf, sizeof(buf), 5000);
        if (r > 0) {
            printf("[DEBUG] Received %d bytes: ", r);
            for(int i = 0; i < r && i < 20; i++) {
                printf("%02x ", buf[i]);
            }
            printf("\n");
            
            // PUBREC format: [Length][0x0F][MsgId MSB][MsgId LSB]
            if (r >= 4 && buf[1] == 0x0F) {  // 0x0F = PUBREC
                unsigned short rec_msgid = (buf[2] << 8) | buf[3];
                printf("[MQTTSN] ✓ PUBREC received (MsgID=%u)\n", rec_msgid);
                
                // Send PUBREL
                unsigned char pubrel[4];
                pubrel[0] = 4;        // Length
                pubrel[1] = 0x10;     // PUBREL type
                pubrel[2] = (msgid >> 8);
                pubrel[3] = (msgid & 0xFF);
                
                mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, pubrel, sizeof(pubrel));
                printf("[MQTTSN] → PUBREL sent (MsgID=%u)\n", msgid);
                
                // Wait for PUBCOMP
                printf("[MQTTSN] Waiting for PUBCOMP...\n");
                r = mqttsn_transport_receive(buf, sizeof(buf), 5000);
                if (r > 0) {
                    printf("[DEBUG] Received %d bytes: ", r);
                    for(int i = 0; i < r && i < 20; i++) {
                        printf("%02x ", buf[i]);
                    }
                    printf("\n");
                    
                    // PUBCOMP format: [Length][0x0E][MsgId MSB][MsgId LSB]
                    if (r >= 4 && buf[1] == 0x0E) {  // 0x0E = PUBCOMP
                        unsigned short comp_msgid = (buf[2] << 8) | buf[3];
                        printf("[MQTTSN] ✓ PUBCOMP received (MsgID=%u) - QoS 2 complete\n", comp_msgid);
                    } else {
                        printf("[MQTTSN] ✗ Expected PUBCOMP but received different message\n");
                        return -8;
                    }
                } else {
                    printf("[MQTTSN] ✗ PUBCOMP not received (timeout)\n");
                    return -9;
                }
            } else {
                printf("[MQTTSN] ✗ Expected PUBREC but received different message\n");
                return -10;
            }
        } else {
            printf("[MQTTSN] ✗ PUBREC not received (timeout)\n");
            return -11;
        }
        
        mqttsn_msg_id++;  // Increment only after complete handshake
    }
    // For QoS 0, no acknowledgment needed, message ID stays 0
    
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
    unsigned char buf[512];
    int rc = mqttsn_transport_receive(buf, sizeof(buf), timeout_ms);
    
    if (rc > 0) {
        printf("[UDP] Received %d bytes (blocking, %lu ms timeout)\n", rc, timeout_ms);
        
        // Check message type
        if (rc >= 2) {
            uint8_t length = buf[0];
            uint8_t msg_type = buf[1];
            
            printf("[MQTTSN] Received message type=0x%02X, length=%d\n", msg_type, length);
            
            switch (msg_type) {
                case 0x18: // DISCONNECT
                    printf("[MQTTSN] ✗ Received DISCONNECT from gateway\n");
                    printf("[MQTTSN] Gateway or broker closed the connection\n");
                    printf("[INFO] Check if broker is running on 127.0.0.1:1883\n");
                    mqttsn_connected = false;
                    mqttsn_registered_topicid = 0;
                    return -1;
                    
                case 0x0C: // PUBLISH
                    #ifdef HAVE_PAHO
                    {
                        // Parse PUBLISH message
                        unsigned char dup, retained;
                        unsigned short msgid;
                        int qos;
                        MQTTSN_topicid topic;
                        unsigned char *payload;
                        int payloadlen;
                        
                        int parse_rc = MQTTSNDeserialize_publish(&dup, &qos, &retained, &msgid, 
                                                                 &topic, &payload, &payloadlen, 
                                                                 buf, rc);
                        if (parse_rc == 1) {
                            printf("[MQTTSN] Received PUBLISH: TopicID=%u, QoS=%d, MsgID=%u, PayloadLen=%d\n",
                                   topic.data.id, qos, msgid, payloadlen);
                            
                            // Print payload as text (if printable)
                            printf("[MQTTSN] Payload: ");
                            for (int i = 0; i < payloadlen && i < 100; i++) {
                                if (payload[i] >= 32 && payload[i] < 127) {
                                    printf("%c", payload[i]);
                                } else {
                                    printf(".");
                                }
                            }
                            if (payloadlen > 100) printf("...");
                            printf("\n");
                            
                            // Send PUBACK for QoS 1
                            if (qos == 1) {
                                unsigned char puback_buf[7];
                                puback_buf[0] = 7;
                                puback_buf[1] = 0x0D;  // PUBACK
                                puback_buf[2] = (topic.data.id >> 8);
                                puback_buf[3] = (topic.data.id & 0xFF);
                                puback_buf[4] = (msgid >> 8);
                                puback_buf[5] = (msgid & 0xFF);
                                puback_buf[6] = 0x00;  // Return code (accepted)
                                mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, 
                                                     puback_buf, sizeof(puback_buf));
                            }
                        }
                    }
                    #endif
                    break;
                    
                case 0x16: // PINGREQ
                    printf("[MQTTSN] Received PINGREQ - sending PINGRESP\n");
                    // Send PINGRESP
                    unsigned char pingresp[] = {0x02, 0x17};
                    mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, 
                                         pingresp, sizeof(pingresp));
                    break;
                    
                default:
                    printf("[MQTTSN] Received non-PUBLISH or unhandled message\n");
                    break;
            }
        }
        
        return rc;
    }
    return rc;
}

void mqttsn_demo_close(void){
    if (mqttsn_initialized){
        // Send DISCONNECT if connected
        if (mqttsn_connected) {
#ifdef HAVE_PAHO
            unsigned char buf[16];
            int len = MQTTSNSerialize_disconnect(buf, sizeof(buf), 0);
            if (len > 0) {
                mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, buf, len);
                printf("[MQTTSN] DISCONNECT sent\n");
            }
#endif
        }
        
        mqttsn_transport_close();
        mqttsn_initialized = false;
        mqttsn_connected = false;
        mqttsn_registered_topicid = 0;
        printf("[MQTTSN] Client closed\n");
    }
}

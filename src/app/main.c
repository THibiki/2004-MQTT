#include <stdio.h>
#include <string.h>

// Pico SDK header files
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include "hardware/gpio.h"
#include "ff.h"

// Custom header files
#include "config/network_config.h"
#include "drivers/wifi_driver.h"
#include "drivers/udp_driver.h"
#include "net/network_errors.h"
#include "protocol/mqttsn/mqttsn_client.h"
#include "protocol/mqttsn/mqttsn_adapter.h"
#include "drivers/block_transfer.h"
#include "drivers/sd_card.h"

#ifdef HAVE_PAHO
#include "MQTTSNPacket.h"
#include "MQTTSNPublish.h"
#endif

#define QOS_TOGGLE 22  // GP22
#define BLOCK_TRANSFER 21  // GP22

// Timestamp (ms) for rate-limiting WiFi status messages
static uint32_t last_wifi_wait_print = 0;

// Button debouncing
static volatile uint32_t last_button_press = 0;
static uint32_t last_block_transfer_button_press = 0;
static const uint32_t DEBOUNCE_MS = 300;

// GPIO interrupt handler for button
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == QOS_TOGGLE && (events & GPIO_IRQ_EDGE_FALL)) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Debounce check
        if (now - last_button_press > DEBOUNCE_MS) {
            last_button_press = now;
            
            // Toggle QoS level: 0 -> 1 -> 2 -> 0
            int current_qos = mqttsn_get_qos();
            int next_qos = (current_qos + 1) % 3;
            mqttsn_set_qos(next_qos);
            
            printf("\n[BUTTON] QoS level changed: %d -> %d\n", current_qos, next_qos);
            printf("[INFO] Next publish will use QoS %d\n", next_qos);
        }
    }
}

// init SD + block transfer
static bool app_init_sd_card_once(void){
    static bool initialised = false;

    if (initialised) {
        return true;
    }

    printf("[SD] Initialising SD card...\n");
    if(sd_card_init_with_detection() != 0){
        printf("[SD] SD card hardware initialisation failed.\n");
        return false;
    }

    if (sd_card_mount_fat32() != 0){
        printf("[SD] FAT32 mount failed.\n");
        return false;
    }
    
    printf("[SD] SD card initalised and FAT32 mounted!\n");
    initialised = true;
    return true;
}

// Simple poll-based check for GP21
static bool block_transfer_button_pressed(void){
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (gpio_get(BLOCK_TRANSFER) == 0){
        if (now - last_block_transfer_button_press > DEBOUNCE_MS) {
            last_block_transfer_button_press = now;
            return true;
        }
    }
    return false;
}

static void app_start_block_transfer(void){
    if (!app_init_sd_card_once()) {
        printf("[APP] Cannot start image transfer: SD initialisation failed\n");
        return;
    }

    // Scan SD card for image files and get the first one
    printf("\n[APP] Scanning SD card for images...\n");
    const char *filename = sd_card_get_first_image();
    
    if (filename == NULL) {
        printf("[APP] ✗ No image files found on SD card\n");
        printf("[APP] Please add a .jpg or .jpeg file to the SD card\n");
        return;
    }
    
    const char *topic = "pico/chunks";  // Send to pico/chunks for receiver to save to repo
    int qos = mqttsn_get_qos();

    printf("\n[APP] Block transfer requested (file='%s', topic='%s', QoS='%d')\n", filename, topic, qos);
    printf("[APP] Sending image from SD card to GitHub repo...\n");

    int rc = send_image_file_qos(topic, filename, (uint8_t)qos);
    if (rc == 0){
        printf("[APP] ✓ Block Transfer completed successfully\n");
        printf("[APP] Image '%s' sent to GitHub repo\n", filename);
    } else {
        printf("[APP] ✗ Block Transfer failed (rc=%d)\n", rc);
    }
}

void buttons_init() {

    gpio_init(BLOCK_TRANSFER);
    gpio_set_dir(BLOCK_TRANSFER, GPIO_IN);
    gpio_pull_up(BLOCK_TRANSFER);  // Enable pull-up (button connects to GND)

    gpio_init(QOS_TOGGLE);
    gpio_set_dir(QOS_TOGGLE, GPIO_IN);
    gpio_pull_up(QOS_TOGGLE);  // Enable pull-up (button connects to GND)

    // Enable interrupt on falling edge (button press)
    gpio_set_irq_enabled_with_callback(QOS_TOGGLE, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
}

int main(){
    stdio_init_all();
    sleep_ms(3000); // Provide time for serial monitor to connect

    printf("\n=== MQTT-SN Pico W Client Starting ===\n");

    // ========================= Button Setup =========================
    buttons_init();
    
    printf("[BUTTON] GP22 configured for QoS toggle (pull-up enabled), GP21: Block transfer\n");
    printf("[INFO] Press button to cycle: QoS 0 -> QoS 1 -> QoS 2 -> QoS 0\n");

    // ========================= WiFi Init =========================
    if (wifi_init(WIFI_SSID, WIFI_PASSWORD) != 0){
        printf("[WARNING] WiFi Initialisation Failed...\n");
        return 1;
    }

    if (wifi_connect() != 0) {
        printf("[WARNING] Initial connection failed - will retry automatically\n");
    }

    block_transfer_init();

    // Main Loop
    bool was_connected = wifi_is_connected();
    absolute_time_t last_status_print = get_absolute_time();
    bool mqtt_demo_started = false;
    bool subscribed_to_retransmit = false;
    uint32_t last_publish = 0;
    uint32_t connection_start_time = 0;

    while (true){
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // ========================= WiFi Reconnection Handling =========================
        wifi_auto_reconnect();  
        bool is_connected = wifi_is_connected();

        // ========================= WiFi Events =========================
        // 1. WiFi Reconnected
        if (is_connected && !was_connected){
            printf("[INFO] WiFi Reconnected! Reinitializing Network Services...\n");
            connection_start_time = now;
            
            // Reset MQTT demo state on reconnection
            mqtt_demo_started = false;
            mqttsn_demo_close();
        }

        // 2. WiFi Disconnected
        if (!is_connected && was_connected){
            printf("[WARNING] WiFi Connection Lost!\n");
            mqtt_demo_started = false;
        }
        
        was_connected = is_connected;  

        // 3. WiFi Connected
        if (is_connected){

            if (!mqtt_demo_started){
                printf("\n[MQTT-SN] Initializing MQTT-SN Demo...\n");
                
                // Give publisher a unique client ID
                // Modify mqttsn_client.c to accept client ID parameter
                if (mqttsn_demo_init(0, "pico_w_publisher") == 0) {
                    printf("[MQTT-SN] ✓ MQTT-SN Demo initialized successfully\n");
                    
                    // Subscribe to retransmit requests from subscriber
                    printf("[MQTT-SN] Subscribing to retransmit topic...\n");
                    unsigned short retx_topicid = 0;
                    int sub_ret = mqttsn_demo_subscribe("pico/retransmit", 200, &retx_topicid);
                    if (sub_ret > 0) {
                        printf("[MQTT-SN] ✓ Subscribed to 'pico/retransmit' (TopicID=%u)\n", retx_topicid);
                        subscribed_to_retransmit = true;
                    } else {
                        printf("[MQTT-SN] ⚠️  Failed to subscribe to retransmit topic\n");
                    }
                    
                    mqtt_demo_started = true;
                } else {
                    printf("[MQTT-SN] ✗ MQTT-SN Demo initialization failed, retrying...\n");
                    sleep_ms(10000);
                }
            } else {
                // Process ALL incoming MQTT-SN messages - check multiple times
                // Use shorter timeout to process messages quickly
                for (int i = 0; i < 10; i++) {
                    unsigned char recv_buf[256];
                    int recv_rc = mqttsn_transport_receive(recv_buf, sizeof(recv_buf), 5);
                    
                    if (recv_rc <= 0 || recv_rc < 2) {
                        break;  // No more messages
                    }
                    uint8_t msg_type = recv_buf[1];
                    
                    // Debug: show what we received with hex dump
                    printf("[PUBLISHER] Received message type=0x%02X, length=%d: ", msg_type, recv_rc);
                    for (int i = 0; i < (recv_rc < 20 ? recv_rc : 20); i++) {
                        printf("%02X ", recv_buf[i]);
                    }
                    printf("\n");
                    
                    if (msg_type == 0x0C) {  // PUBLISH
                        #ifdef HAVE_PAHO
                        unsigned char dup, retained;
                        unsigned short msgid;
                        int qos;
                        MQTTSN_topicid topic;
                        unsigned char *payload;
                        int payloadlen;
                        
                        int deserialize_result = MQTTSNDeserialize_publish(&dup, &qos, &retained, &msgid, 
                                                     &topic, &payload, &payloadlen, 
                                                     recv_buf, recv_rc);
                        
                        printf("[PUBLISHER] Deserialize result: %d\n", deserialize_result);
                        
                        if (deserialize_result == 1) {
                            
                            printf("[PUBLISHER] PUBLISH decoded: TopicID=%u, QoS=%d, PayloadLen=%d\n",
                                   topic.data.id, qos, payloadlen);
                            
                            // Check if this is a retransmit request
                            if (payloadlen >= 5 && strncmp((char*)payload, "RETX:", 5) == 0) {
                                printf("\n[PUBLISHER] 📩 Retransmit request received!\n");
                                printf("[PUBLISHER] Payload: %.*s\n", payloadlen, payload);
                                
                                // Null-terminate payload for string processing
                                char request_msg[256];
                                int copy_len = (payloadlen < 255) ? payloadlen : 255;
                                memcpy(request_msg, payload, copy_len);
                                request_msg[copy_len] = '\0';
                                
                                // Handle the retransmit request
                                block_transfer_handle_retransmit_request(request_msg);
                            } else {
                                printf("[PUBLISHER] Regular message (not RETX): %.*s\n", 
                                       payloadlen < 50 ? payloadlen : 50, payload);
                            }
                            
                            // Send PUBACK for QoS 1
                            if (qos == 1) {
                                unsigned char puback_buf[7];
                                puback_buf[0] = 7;
                                puback_buf[1] = 0x0D;
                                puback_buf[2] = (topic.data.id >> 8);
                                puback_buf[3] = (topic.data.id & 0xFF);
                                puback_buf[4] = (msgid >> 8);
                                puback_buf[5] = (msgid & 0xFF);
                                puback_buf[6] = 0x00;
                                mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, 
                                                     puback_buf, sizeof(puback_buf));
                                printf("[PUBLISHER] PUBACK sent for MsgID=%u\n", msgid);
                            }
                        } else {
                            printf("[PUBLISHER] Failed to deserialize PUBLISH\n");
                        }
                        #endif
                    } else if (msg_type == 0x16) {  // PINGREQ
                        unsigned char pingresp[] = {0x02, 0x17};
                        mqttsn_transport_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, 
                                             pingresp, sizeof(pingresp));
                    } else if (msg_type == 0x18) {  // DISCONNECT
                        printf("[MQTTSN] Connection lost - will reconnect...\n");
                        mqtt_demo_started = false;
                        subscribed_to_retransmit = false;
                        mqttsn_demo_close();
                        sleep_ms(5000);
                        continue;
                    }
                }  // End of receive loop

                // Periodically publish every 5 seconds
                uint32_t now_ms = to_ms_since_boot(get_absolute_time());
                if (now_ms - last_publish > 5000){
                    static uint32_t message_count = 0;
                    char msg[64];
                    int qos = mqttsn_get_qos();
                    snprintf(msg, sizeof(msg), "Hello from Pico W #%lu (QoS%d)", message_count++, qos);
                    
                    printf("\n[MQTTSN] >>> Publishing message #%lu with QoS %d <<<\n", message_count, qos);
                    
                    uint32_t pub_start = to_ms_since_boot(get_absolute_time());
                    int pub_result = mqttsn_demo_publish_name("pico/test", (const uint8_t*)msg, (int)strlen(msg));
                    uint32_t pub_end = to_ms_since_boot(get_absolute_time());
                    
                    if (pub_result == 0) {
                        printf("[MQTTSN] ✓ SUCCESS: Message published (latency=%lums)\n", pub_end - pub_start);
                    } else {
                        printf("[MQTTSN] ✗ WARNING: Publish failed (rc=%d)\n", pub_result);
                        mqtt_demo_started = false;
                        mqttsn_demo_close();
                    }
                    last_publish = now_ms;
                }

                if (block_transfer_button_pressed()) {
                    printf("[BUTTON] Block Transfer button pressed.\n");
                    app_start_block_transfer();
                }
            }

        } else {
            // Prints every 5 seconds
            uint32_t now_ms = to_ms_since_boot(get_absolute_time());
            if (now_ms - last_wifi_wait_print >= 5000) {
                printf("[APP] Waiting for WiFi... (Status: %s)\n", wifi_get_status());
                last_wifi_wait_print = now_ms;
            }
        }

        // Print stats every 30 seconds
        if (absolute_time_diff_us(last_status_print, get_absolute_time()) > 30000000) {
            printf("\n=== System Statistics ===\n");
            wifi_print_stats();
            printf("MQTT-SN Status: %s\n", mqtt_demo_started ? "Connected" : "Disconnected");
            printf("Current QoS Level: %d\n", mqttsn_get_qos());
            if (mqtt_demo_started) {
                printf("Uptime: %lu seconds\n", (now - connection_start_time) / 1000);
            }
            last_status_print = get_absolute_time();
            // Read statistics
            sleep_ms(3000);
        }

        sleep_ms(10);
    }

    mqttsn_demo_close();
    return 0;
}
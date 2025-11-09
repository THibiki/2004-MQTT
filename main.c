#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"

#include "network_config.h"
#include "wifi_driver.h"
#include "udp_driver.h"
#include "network_errors.h"
#include "mqttsn_client_example.h"

int main(){
    stdio_init_all();
    sleep_ms(3000); // Provide time for serial monitor to connect

    printf("\n=== MQTT-SN Pico W Client Starting ===\n");

    // ========================= WiFi Init =========================
    if (wifi_init(WIFI_SSID, WIFI_PASSWORD) != 0){
        printf("[WARNING] WiFi Initialisation Failed...\n");
        return 1;
    }

    if (wifi_connect() != 0) {
        printf("[WARNING] Initial connection failed - will retry automatically\n");
    }

    sleep_ms(2000);

    // Main Loop
    bool was_connected = wifi_is_connected();
    absolute_time_t last_status_print = get_absolute_time();
    bool mqtt_demo_started = false;
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
            mqttsn_demo_close(); // Close any existing MQTT-SN connection
        }

        // 2. WiFi Disconnected
        if (!is_connected && was_connected){
            printf("[WARNING] WiFi Connection Lost!\n");
            mqtt_demo_started = false;
        }
        
        // Update was_connected as WiFi connection status changed
        was_connected = is_connected;  

        // 3. WiFi Connected
        if (is_connected){
            // Poll for WiFi stack
            cyw43_arch_poll();

            // Initialize MQTT-SN demo once after connection
            if (!mqtt_demo_started){
                printf("\n=== Network Connectivity Test ===\n");
                
                // Test 1: Basic UDP connectivity to gateway
                printf("[TEST 1] Testing UDP connectivity to gateway %s:%d...\n", "172.20.10.7", 1885);
                const char *test_ping = "PING";
                int rc = wifi_udp_create(0);  // Random port
                if (rc == 0){
                    printf("[TEST 1] UDP socket created, sending PING...\n");
                    rc = wifi_udp_send("172.20.10.7", 1885, (uint8_t*)test_ping, 4);
                    printf("[TEST 1] UDP send result: %d\n", rc);
                    
                    if (rc == 0) {
                        uint8_t response[256];
                        printf("[TEST 1] Waiting for response...\n");
                        int recv = wifi_udp_receive(response, sizeof(response), 2000);
                        if (recv > 0) {
                            printf("[TEST 1] SUCCESS: Gateway responded with %d bytes: ", recv);
                            for(int i = 0; i < recv && i < 20; i++) {
                                printf("%02x ", response[i]);
                            }
                            printf("\n");
                        } else {
                            printf("[TEST 1] No response from gateway (rc=%d) - gateway may not be responding to raw UDP\n", recv);
                        }
                    }
                    wifi_udp_close();
                } else {
                    printf("[TEST 1] FAILED: Could not create UDP socket (rc=%d)\n", rc);
                }

                // Test 2: Test with configured MQTT-SN gateway IP
                printf("\n[TEST 2] Testing with configured MQTT-SN gateway: %s:%d\n", 
                       MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT);
                rc = wifi_udp_create(0);
                if (rc == 0) {
                    rc = wifi_udp_send(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, (uint8_t*)test_ping, 4);
                    printf("[TEST 2] Send to %s:%d result: %d\n", 
                           MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT, rc);
                    
                    if (rc == 0) {
                        uint8_t response[256];
                        int recv = wifi_udp_receive(response, sizeof(response), 2000);
                        if (recv > 0) {
                            printf("[TEST 2] SUCCESS: Gateway responded!\n");
                        } else {
                            printf("[TEST 2] No response (rc=%d)\n", recv);
                        }
                    }
                    wifi_udp_close();
                }

                // Test 3: Initialize MQTT-SN client
                printf("\n[TEST 3] Initializing MQTT-SN client...\n");
                printf("[DEBUG] Using gateway: %s:%d\n", MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT);
                
                if (mqttsn_demo_init(0) == 0){
                    printf("[TEST 3] SUCCESS: MQTT-SN client initialized\n");
                    
                    // Subscribe to a test topic
                    printf("[MQTTSN] Subscribing to test/topic...\n");
                    unsigned short topicid = 0;
                    int sub_result = mqttsn_demo_subscribe("test/topic", 1, &topicid);
                    if (sub_result > 0) {
                        printf("[MQTTSN] SUCCESS: Subscribed to test/topic, topicid=%u\n", topicid);
                    } else {
                        printf("[MQTTSN] WARNING: Subscribe failed (rc=%d)\n", sub_result);
                    }
                    
                    mqtt_demo_started = true;
                    last_publish = to_ms_since_boot(get_absolute_time());
                    printf("[MQTTSN] Client ready - starting publish loop\n");
                } else {
                    printf("[TEST 3] FAILED: MQTT-SN initialization failed\n");
                    // Retry after delay
                    sleep_ms(5000);
                }
            } else {
                // MQTT-SN client is running - process messages and publish
                
                // Process incoming MQTT-SN messages (non-blocking)
                int processed = mqttsn_demo_process_once(100); // 100ms timeout
                if (processed > 0) {
                    printf("[MQTTSN] Processed incoming message (%d bytes)\n", processed);
                }

                // Periodically publish every 5 seconds
                uint32_t now_ms = to_ms_since_boot(get_absolute_time());
                if (now_ms - last_publish > 5000){
                    static uint32_t message_count = 0;
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Hello from Pico W #%lu", message_count++);
                    
                    printf("[MQTTSN] Publishing: %s\n", msg);
                    int pub_result = mqttsn_demo_publish_name("test/topic", (const uint8_t*)msg, (int)strlen(msg));
                    if (pub_result == 0) {
                        printf("[MQTTSN] SUCCESS: Message published\n");
                    } else {
                        printf("[MQTTSN] WARNING: Publish failed (rc=%d)\n", pub_result);
                    }
                    last_publish = now_ms;
                }
            }

        } else {
            // Wait for WiFi Connection
            if (now % 5000 < 100) { // Print every ~5 seconds to avoid spam
                printf("[APP] Waiting for WiFi... (Status: %s)\n", wifi_get_status());
            }
        }

        // Print WiFi stats every 30 seconds
        if (absolute_time_diff_us(last_status_print, get_absolute_time()) > 30000000) {
            printf("\n=== WiFi Statistics ===\n");
            wifi_print_stats();
            printf("MQTT-SN Status: %s\n", mqtt_demo_started ? "Connected" : "Disconnected");
            if (mqtt_demo_started) {
                printf("Uptime: %lu seconds\n", (now - connection_start_time) / 1000);
            }
            last_status_print = get_absolute_time();
        }

        cyw43_arch_poll(); // network processing
        sleep_ms(10);
    }

    // Cleanup (though we never reach here in this example)
    mqttsn_demo_close();
    return 0;
}
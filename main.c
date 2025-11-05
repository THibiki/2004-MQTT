#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"

#include "network_config.h"    // Update this to match the wifi_config base file or change wifi config base to this file name
#include "wifi_driver.h"
#include "udp_driver.h"
#include "network_errors.h"

// MQTT-SN Constants
#define MQTTSN_TYPE_ADVERTISE 0x05
#define MQTT_SN_PORT 1883

// Gateway detection state
static bool gateway_detected = false;
static absolute_time_t last_advertise_time = 0;  // Initialize to 0, use nil_time for comparison
static uint8_t gateway_id = 0;
static uint16_t gateway_duration = 0;

// Parse ADVERTISE message: [0x05] [gw_id_high] [gw_id_low] [duration_high] [duration_low]
// Format: 05 00 01 00 3C = ADVERTISE, gw_id=1, duration=60
static bool parse_advertise_message(uint8_t *data, size_t len, uint8_t *gw_id, uint16_t *duration) {
    if (len < 5 || data[0] != MQTTSN_TYPE_ADVERTISE) {
        return false;
    }
    
    // Gateway ID is 2 bytes big-endian (but typically just 1 byte value)
    uint16_t gw_id_16 = (data[1] << 8) | data[2];
    *gw_id = (uint8_t)(gw_id_16 & 0xFF);  // Use low byte
    
    // Duration is 2 bytes big-endian
    *duration = (data[3] << 8) | data[4];
    return true;
}

// Check if gateway hasn't sent ADVERTISE in 2x the duration period
static void check_gateway_timeout(void) {
    if (gateway_detected && last_advertise_time != 0) {
        uint32_t time_since_advertise = absolute_time_diff_us(last_advertise_time, get_absolute_time()) / 1000000;
        uint32_t timeout = (gateway_duration * 2) + 10; // 2x duration + 10 seconds buffer
        
        if (time_since_advertise > timeout) {
            gateway_detected = false;
            printf("\n[GATEWAY] Gateway timeout - no ADVERTISE received for %d seconds\n", time_since_advertise);
        }
    }
}

int main(){
    stdio_init_all();
    sleep_ms(3000); // Provide time for serial monitor to connect

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
    bool was_connected = wifi_is_connected();   //
    absolute_time_t last_status_print = get_absolute_time();
    absolute_time_t last_waiting_print = get_absolute_time();
    bool udp_socket_created = false;
    
    // Create UDP socket if already connected on startup
    if (was_connected) {
        // Wait a bit for IP to be fully assigned
        sleep_ms(500);
        
        if (wifi_udp_create(MQTT_SN_PORT) == 0) {
            printf("[INFO] UDP socket created on port %d for gateway detection\n", MQTT_SN_PORT);
            udp_socket_created = true;
            printf("[INFO] Listening for gateway ADVERTISE messages on port %d\n", MQTT_SN_PORT);
        } else {
            printf("[WARNING] Failed to create UDP socket for gateway detection\n");
        }
    }

    while (true){
        // ========================= WiFi Reconnection Handling =========================
        wifi_auto_reconnect();  
        bool is_connected = wifi_is_connected();

        // ========================= WiFi Events =========================
        // 1. WiFi Reconnected
        if (is_connected && !was_connected){
            // is_connected refers to new wifi status gotten (Connect/Not Connected)
            // was_connected refers to PREVIOUS wifi status gotten (Connect/Not Connected)
            printf("[INFO] WiFi Reconnected! Reinitalizing Network Services...\n");
            
            // Wait a bit for IP to be fully assigned
            sleep_ms(500);
            
            // Create UDP socket for gateway detection
            if (!udp_socket_created) {
                if (wifi_udp_create(MQTT_SN_PORT) == 0) {
                    printf("[INFO] UDP socket created on port %d for gateway detection\n", MQTT_SN_PORT);
                    udp_socket_created = true;
                    printf("[INFO] Listening for gateway ADVERTISE messages on port %d\n", MQTT_SN_PORT);
                } else {
                    printf("[WARNING] Failed to create UDP socket for gateway detection\n");
                }
            }
        }

        // 2. WiFi Disconnected
        if (!is_connected && was_connected){
            printf("[WARNING] WiFi Connection Lost!\n");
            
            // Close UDP socket
            if (udp_socket_created) {
                wifi_udp_close();
                udp_socket_created = false;
                gateway_detected = false;
                printf("[INFO] Gateway detection stopped (WiFi disconnected)\n");
            }
        }
        
        // Update was_connected as WiFi connection was lost
        was_connected = is_connected;  

        // 3. WiFi Connected
        if (is_connected){
            // Poll for WiFi stack - call this frequently to process network packets
            cyw43_arch_poll();
            
            // Check for gateway ADVERTISE messages
            if (udp_socket_created) {
                // Always have a buffer ready to catch packets
                uint8_t buffer[256];
                int bytes_received = wifi_udp_receive(buffer, sizeof(buffer), 0); // Non-blocking
                
                if (bytes_received > 0) {
                    uint8_t gw_id;
                    uint16_t duration;
                    
                    if (parse_advertise_message(buffer, bytes_received, &gw_id, &duration)) {
                        bool was_detected = gateway_detected;
                        gateway_detected = true;
                        gateway_id = gw_id;
                        gateway_duration = duration;
                        last_advertise_time = get_absolute_time();
                        
                        if (!was_detected) {
                            printf("\n[GATEWAY] ✅ Gateway detected! ID: %d, Duration: %d seconds\n", gw_id, duration);
                        } else {
                            // Print periodic update every 30 seconds
                            static absolute_time_t last_update = 0;
                            if (absolute_time_diff_us(last_update, get_absolute_time()) > 30000000) {
                                uint32_t time_since = absolute_time_diff_us(last_advertise_time, get_absolute_time()) / 1000000;
                                printf("[GATEWAY] Still active (ID: %d, last seen: %ds ago)\n", gw_id, time_since);
                                last_update = get_absolute_time();
                            }
                        }
                    }
                }
            }

        } else {
            // Wait for WiFi Connection - only print every 5 seconds
            if (absolute_time_diff_us(last_waiting_print, get_absolute_time()) > 5000000) {
                printf("[APP] Waiting for WiFi... (Status: %s)\n", 
                       wifi_get_status());
                last_waiting_print = get_absolute_time();
            }
        }

        // Print WiFi stats every 60 seconds
        if (absolute_time_diff_us(last_status_print, get_absolute_time()) > 60000000) {
            printf("\n========================= WiFi Statistics =========================\n");
            wifi_print_stats();
            
            // Print gateway status
            if (gateway_detected) {
                uint32_t time_since = absolute_time_diff_us(last_advertise_time, get_absolute_time()) / 1000000;
                printf("[GATEWAY] Status: Detected (ID: %d, Duration: %ds, Last seen: %ds ago)\n", 
                       gateway_id, gateway_duration, time_since);
            } else {
                printf("[GATEWAY] Status: Not detected (listening for ADVERTISE messages...)\n");
            }
            printf("================================================================\n");
            
            last_status_print = get_absolute_time();
        }

        // Check gateway timeout periodically
        if (udp_socket_created && gateway_detected) {
            check_gateway_timeout();
        }
        
        cyw43_arch_poll(); // network processing - call this frequently
        
        sleep_ms(10);
    }

    return 0;
}
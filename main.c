#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"

#include "network_config.h"    // Update this to match the wifi_config base file or change wifi config base to this file name
#include "wifi_driver.h"
#include "udp_driver.h"
#include "network_errors.h"
#include "mqttsn_client_example.h"

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
    bool mqtt_demo_started = false;
    uint32_t last_publish = 0;

    while (true){
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // ========================= WiFi Reconnection Handling =========================
        wifi_auto_reconnect();  
        bool is_connected = wifi_is_connected();

        // ========================= WiFi Events =========================
        // 1. WiFi Reconnected
        if (is_connected && !was_connected){
            // is_connected refers to new wifi status gotten (Connect/Not Connected)
            // was_connected refers to PREVIOUS wifi status gotten (Connect/Not Connected)
            printf("[INFO] WiFi Reconnected! Reinitalizing Network Services...\n");


        }

        // 2. WiFi Disconnected
        if (!is_connected && was_connected){
            printf("[WARNING] WiFi Connection Lost!\n");

        }
        
        // Update was_connected as WiFi connection was lost
        was_connected = is_connected;  

        // 3. WiFi Connected
        if (is_connected){

            // Poll for WiFi stack
            cyw43_arch_poll();

            // Initialize MQTT-SN demo once after connection
            if (!mqtt_demo_started){
                if (mqttsn_demo_init(1883) == 0){
                    // subscribe to a test topic (packetid 1)
                    unsigned short topicid = 0;
                    mqttsn_demo_subscribe("test/topic", 1, &topicid);
                    mqtt_demo_started = true;
                    last_publish = to_ms_since_boot(get_absolute_time());
                }
            } else {
                // Process incoming MQTT-SN messages (non-blocking)
                mqttsn_demo_process_once(0);

                // Periodically publish
                uint32_t now_ms = to_ms_since_boot(get_absolute_time());
                if (now_ms - last_publish > 5000){
                    const char *msg = "hello from pico_w";
                    mqttsn_demo_publish_name("test/topic", (const uint8_t*)msg, (int)strlen(msg));
                    last_publish = now_ms;
                }
            }

        } else {
            // Wait for WiFi Connection
            printf("[APP] Waiting for WiFi... (Status: %s)\n", 
                       wifi_get_status());
        }

        // Print WiFi stats every 60 seconds
        if (absolute_time_diff_us(last_status_print, get_absolute_time()) > 60000000) {
            printf("========================= WiFi Statistics =========================");
            wifi_print_stats();
            last_status_print = get_absolute_time();
        }

        cyw43_arch_poll(); // network processing
        
        sleep_ms(10);
    }

    return 0;
}
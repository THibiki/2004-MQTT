#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"

#include "wifi_config.h"
#include "wifi.h"

int main(){
    stdio_init_all();
    sleep_ms(3000); // Provide time for serial monitor to connect

    // ========== WiFi Init ==========

    if (wifi_init(WIFI_SSID, WIFI_PASSWORD) != 0){
        printf("[WARNING] WiFi Initialisation Failed...\n");
        return 1;
    }

    printf("\n=== Initial Connection ===\n");

    if (wifi_connect() != 0) {
        printf("[WARNING] Initial connection failed - will retry automatically\n");
    }

    sleep_ms(2000);

    // ======== Application Init ========
    if (wifi_is_connected()) {
        printf("\n=== Initializing Application ===\n");
        
        // Initialize features (MQTT-SN, file transfer etc.)
        
        printf("[INFO] Application ready\n");
    } else {
        printf("[WARNING] Starting without WiFi - waiting for connection\n");
    }

    // Main Loop
    bool was_connected = wifi_is_connected();   //
    int loop_counter = 0;
    absolute_time_t last_stats_print = get_absolute_time();

    while (1){
        wifi_auto_reconnect();  // handles wifi reconnection automatically

        bool is_connected = wifi_is_connected();

        // WiFi Reconnected 
        if (is_connected && !was_connected){
            // is_connected refers to new wifi status gotten (Connect/Not Connected)
            // was_connected refers to PREVIOUS wifi status gotten (Connect/Not Connected)
            printf("[INFO] WiFi Reconnected!\n");

            printf("[INFO] Reinitalizing Network Services...\n");
            // reinit_mqttsn();

            printf("[INFO] Resuming Interrupted Services...\n");
        }

        // WiFi Disconnected
        if (!is_connected && was_connected){
            printf("[WARNING] WiFi Connection Lost!\n");

            // mqtt_connected = false;
        }
        
        // Update was_connected as WiFi connection was lost
        was_connected = is_connected;  

        if (is_connected){
            if(loop_counter % 1000 == 0){
                printf("[APP] Running... (loop:%d)\n", loop_counter);

                // Application core codes: MQTT-SN messages, File Transfer
            }
        } else {
            // Wait for WiFi Connection
            if (loop_counter % 10000 == 0) {
                printf("[APP] Waiting for WiFi... (Status: %s)\n", 
                       wifi_get_status());
            }
        }

        // Print WiFi stats every 60 seconds
        if (absolute_time_diff_us(last_stats_print, get_absolute_time()) > 60000000) {
            wifi_print_stats();
            last_stats_print = get_absolute_time();
        }

        cyw43_arch_poll(); // network processing
        
        loop_counter++;
        sleep_ms(1);
    }

    return 0;
}
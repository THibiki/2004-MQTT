#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

int main() {
    stdio_init_all();
    
    printf("=== Pico W USB Test Program ===\n");
    printf("If you can see this, USB serial is working!\n");
    
    // Wait to ensure USB enumeration is complete
    sleep_ms(3000);
    
    int count = 0;
    while (true) {
        printf("Heartbeat #%d - USB working, program running\n", ++count);
        sleep_ms(2000);
        
        // Test WiFi but don't exit on failure
        if (count == 5) {
            printf("Testing WiFi initialization...\n");
            if (cyw43_arch_init()) {
                printf("WiFi init failed - continuing anyway\n");
            } else {
                printf("WiFi init successful!\n");
                cyw43_arch_enable_sta_mode();
                
                printf("Testing WiFi connection to 'jer'...\n");
                if (cyw43_arch_wifi_connect_timeout_ms("jer", "jeraldgoh", CYW43_AUTH_WPA2_AES_PSK, 15000)) {
                    printf("WiFi connection failed - continuing anyway\n");
                } else {
                    printf("SUCCESS: Connected to WiFi 'jer'!\n");
                }
            }
        }
        
        // Keep running forever with heartbeat
        if (count > 10) {
            count = 0; // Reset counter to avoid overflow
        }
    }
    
    return 0;
}
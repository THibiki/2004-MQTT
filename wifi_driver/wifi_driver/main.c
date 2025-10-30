#include "pico/stdlib.h"
#include "wifi_driver.h"
#include "mqtt_sn_client.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>

int main() {
    int ret;
    
    stdio_init_all();
    sleep_ms(2000);
    
    printf("\n=== MQTT-SN Client Test ===\n");
    
    // Initialize WiFi
    printf("Initializing WiFi...\n");
    ret = wifi_init();
    if (ret != WIFI_OK) {
        printf("WiFi init failed: %d\n", ret);
        return -1;
    }
    sleep_ms(2000);
    
    // Connect to WiFi
    printf("Connecting to WiFi...\n");
    const char *ssid = "jer";
    const char *password = "jeraldgoh";
    ret = wifi_connect(ssid, password);
    if (ret != WIFI_OK) {
        printf("WiFi connect failed: %d\n", ret);
        return -1;
    }
    
    printf("WiFi connected!\n");
    sleep_ms(1000);

    // Create UDP socket
    printf("Creating UDP socket...\n");
    ret = wifi_udp_create(0);  // Use 0 for auto port assignment
    if (ret != WIFI_OK) {
        printf("UDP socket creation failed: %d\n", ret);
        return -1;
    }
    
    const char *gateway_ip = "172.20.10.14";  // Windows Wi-Fi IP (UDP relay will forward to WSL)
    uint16_t gateway_port = 1884;
    
    // Initialize MQTT-SN
    printf("Initializing MQTT-SN...\n");
    if (mqttsn_init(gateway_ip, gateway_port) != MQTTSN_OK) {
        printf("MQTT-SN init failed\n");
        return -1;
    }
    
    // Connect to gateway
    printf("Connecting to MQTT-SN gateway...\n");
    ret = mqttsn_connect("PicoW_Client", 60);
    if (ret != MQTTSN_OK) {
        printf("MQTT-SN connect failed: %d\n", ret);
        printf("Make sure the gateway is running on %s:%d\n", gateway_ip, gateway_port);
        return -1;
    }
    
    printf("MQTT-SN connected!\n");
    
    // Subscribe to a test topic
    printf("Subscribing to 'pico/test'...\n");
    mqttsn_subscribe("pico/test", 0);
    
    // Main loop
    uint32_t last_publish = 0;
    int counter = 0;
    
    printf("\n=== Starting main loop ===\n");
    printf("Publishing to 'pico/data' every 5 seconds\n");
    printf("Listening for messages on 'pico/test'\n\n");
    
    while (true) {
        // Poll for incoming MQTT-SN messages
        mqttsn_poll();
        
        // Publish every 5 seconds
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_publish > 5000) {
            char message[64];
            snprintf(message, sizeof(message), "Hello from Pico W! Count: %d", counter++);
            
            printf("Publishing: %s\n", message);
            mqttsn_publish("pico/data", (uint8_t*)message, strlen(message), 0);
            
            last_publish = now;
        }
        
        // Poll WiFi stack
        cyw43_arch_poll();
        sleep_ms(10);
    }
    
    return 0;
}

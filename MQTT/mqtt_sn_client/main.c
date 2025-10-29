#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "mqtt_sn_client.h"

// WiFi credentials - set these for your network
#ifndef WIFI_SSID
#define WIFI_SSID "Hibiki"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "Asura96Kai"
#endif

// MQTT-SN Gateway configuration
// Updated to your laptop IP from ipconfig output and recommended MQTT-SN UDP port
#define GATEWAY_IP "10.132.53.215"
#define GATEWAY_PORT 1884

// Global client instance
mqtt_sn_client_t client;

// Callback functions
void on_connect_callback(uint8_t return_code) {
    printf("MQTT-SN Connect callback: return_code=%d\n", return_code);
    if (return_code == MQTT_SN_ACCEPTED) {
        printf("Successfully connected to MQTT-SN gateway!\n");
        client.state = MQTT_SN_STATE_CONNECTED;
    } else {
        printf("Failed to connect to MQTT-SN gateway\n");
        client.state = MQTT_SN_STATE_DISCONNECTED;
    }
}

void on_register_callback(uint16_t topic_id, uint8_t return_code) {
    printf("MQTT-SN Register callback: topic_id=%d, return_code=%d\n", topic_id, return_code);
    if (return_code == MQTT_SN_ACCEPTED) {
        printf("Successfully registered topic with ID: %d\n", topic_id);
    } else {
        printf("Failed to register topic\n");
    }
}

void on_publish_callback(uint16_t topic_id, const uint8_t *data, uint16_t data_len) {
    printf("MQTT-SN Publish callback: topic_id=%d, data_len=%d\n", topic_id, data_len);
    printf("Received data: ");
    for (int i = 0; i < data_len; i++) {
        printf("%c", data[i]);
    }
    printf("\n");
}

void on_subscribe_callback(uint16_t topic_id, uint8_t return_code) {
    printf("MQTT-SN Subscribe callback: topic_id=%d, return_code=%d\n", topic_id, return_code);
    if (return_code == MQTT_SN_ACCEPTED) {
        printf("Successfully subscribed to topic with ID: %d\n", topic_id);
    } else {
        printf("Failed to subscribe to topic\n");
    }
}

int main() {
    stdio_init_all();
    
    printf("MQTT-SN Client for Pico W\n");
    printf("========================\n");
    
    // Initialize WiFi
    if (cyw43_arch_init()) {
        printf("Failed to initialize WiFi\n");
        return -1;
    }
    
    cyw43_arch_enable_sta_mode();
    
    printf("Connecting to WiFi: %s\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Failed to connect to WiFi\n");
        cyw43_arch_deinit();
        return -1;
    }
    
    printf("Connected to WiFi!\n");
    
    // Initialize MQTT-SN client
    if (mqtt_sn_client_init(&client, "pico_client", GATEWAY_IP, GATEWAY_PORT) != 0) {
        printf("Failed to initialize MQTT-SN client\n");
        cyw43_arch_deinit();
        return -1;
    }
    
    // Set callbacks
    client.on_connect = on_connect_callback;
    client.on_register = on_register_callback;
    client.on_publish = on_publish_callback;
    client.on_subscribe = on_subscribe_callback;
    
    printf("MQTT-SN client initialized\n");
    printf("Gateway: %s:%d\n", GATEWAY_IP, GATEWAY_PORT);
    
    // Main loop
    uint32_t last_connect_attempt = 0;
    uint32_t last_publish = 0;
    uint32_t last_register = 0;
    uint32_t last_stats_report = 0;
    bool topic_registered = false;
    bool connected = false;
    
    while (true) {
        // Process MQTT-SN messages
        mqtt_sn_client_process(&client);
        
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Try to connect if not connected
        if (!connected && (now - last_connect_attempt) > 5000) {
            printf("Attempting to connect to MQTT-SN gateway...\n");
            if (mqtt_sn_connect(&client) == 0) {
                last_connect_attempt = now;
            }
        }
        
        // Register topic after connecting
        if (client.state == MQTT_SN_STATE_CONNECTED && !topic_registered && 
            (now - last_register) > 2000) {
            printf("Registering topic: test/topic\n");
            if (mqtt_sn_register_topic(&client, "test/topic") == 0) {
                last_register = now;
                topic_registered = true;
            }
        }
        
        // Publish messages periodically
        if (client.state == MQTT_SN_STATE_READY && (now - last_publish) > 10000) {
            static uint32_t message_count = 0;
            char message[64];
            snprintf(message, sizeof(message), "Hello from Pico W! Message #%d", ++message_count);
            
            printf("Publishing message: %s\n", message);
            mqtt_sn_publish(&client, 1, (const uint8_t*)message, strlen(message), MQTT_SN_QOS_0);
            last_publish = now;
        }
        
        // Subscribe to a topic (example)
        if (client.state == MQTT_SN_STATE_CONNECTED && (now - last_register) > 5000) {
            printf("Subscribing to topic: pico/commands\n");
            mqtt_sn_subscribe(&client, "pico/commands", MQTT_SN_QOS_0);
            last_register = now;
        }
        
        // Print latency statistics every 30 seconds
        if ((now - last_stats_report) > 30000) {
            mqtt_sn_print_latency_stats(&client);
            last_stats_report = now;
        }
        
        // Update connection status
        connected = (client.state == MQTT_SN_STATE_CONNECTED || client.state == MQTT_SN_STATE_READY);
        
        sleep_ms(100);
    }
    
    // Print final statistics
    printf("\n=== Final MQTT-SN Statistics ===\n");
    mqtt_sn_print_latency_stats(&client);
    
    // Cleanup
    mqtt_sn_client_cleanup(&client);
    cyw43_arch_deinit();
    
    return 0;
}

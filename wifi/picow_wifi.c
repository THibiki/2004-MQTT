#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"

#include "wifi_config.h"    // Update this to match the wifi_config base file or change wifi config base to this file name
#include "wifi.h"
#include "udp_driver.h"
#include "mqtt_sn_client.h"
#include "network_errors.h"

// Button GPIO definitions (Maker Pico W buttons)
#define BTN_WIFI_INIT     20  // Button 1: Initialize WiFi & MQTT
#define BTN_BLOCK_TRANSFER 21 // Button 2: Start image transfer
#define BTN_QOS_TOGGLE    22  // Button 3: Toggle QoS mode (0 or 1)

// Debounce settings
#define DEBOUNCE_MS 200

static bool mqtt_initialized = false;
static bool mqtt_connected = false;
static uint32_t last_button_press[3] = {0};

// Initialize button GPIOs
// Button debounce helper
bool button_pressed(uint gpio_pin, int button_index) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (gpio_get(gpio_pin) == 0) {  // Active low (button pressed)
        if (now - last_button_press[button_index] > DEBOUNCE_MS) {
            last_button_press[button_index] = now;
            return true;
        }
    }
    return false;
}

// MQTT message callback - handles incoming messages from subscribed topics
void on_message_received(const char *topic, const uint8_t *data, size_t len) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    printf("\n[%lu ms] 📬 Received message:\n", now);
    printf("  Topic: %s\n", topic);
    printf("  Size: %zu bytes\n", len);
    
    // Print data as string if printable, otherwise as hex
    bool is_printable = true;
    for (size_t i = 0; i < len && i < 100; i++) {
        if (data[i] < 32 && data[i] != '\n' && data[i] != '\r' && data[i] != '\t') {
            is_printable = false;
            break;
        }
    }
    
    if (is_printable) {
        printf("  Data: %.*s\n\n", (int)len, data);
    } else {
        printf("  Data (hex): ");
        for (size_t i = 0; i < len && i < 32; i++) {
            printf("%02X ", data[i]);
        }
        if (len > 32) printf("... (%zu more bytes)", len - 32);
        printf("\n\n");
    }
    
    // Handle specific topics
    if (strcmp(topic, "pico/test") == 0) {
        // Echo back with "ACK: " prefix
        char response[128];
        snprintf(response, sizeof(response), "ACK: %.*s", (int)len, data);
        // Always use QoS 0 for responses to avoid blocking in callback
        mqttsn_publish("pico/response", (uint8_t*)response, strlen(response), 0);
        printf("  → Sent acknowledgment to pico/response\n\n");
    }
    else if (strcmp(topic, "pico/command") == 0) {
        // Handle commands (you can add custom commands here)
        if (len == 4 && memcmp(data, "ping", 4) == 0) {
            const char *pong = "pong";
            // Always use QoS 0 for responses to avoid blocking in callback
            mqttsn_publish("pico/response", (uint8_t*)pong, 4, 0);
            printf("  → Responded with 'pong'\n\n");
        }
    }
}

// Initialize button GPIOs
void buttons_init() {
    gpio_init(BTN_WIFI_INIT);
    gpio_set_dir(BTN_WIFI_INIT, GPIO_IN);
    gpio_pull_up(BTN_WIFI_INIT);
    
    gpio_init(BTN_BLOCK_TRANSFER);
    gpio_set_dir(BTN_BLOCK_TRANSFER, GPIO_IN);
    gpio_pull_up(BTN_BLOCK_TRANSFER);
    
    gpio_init(BTN_QOS_TOGGLE);
    gpio_set_dir(BTN_QOS_TOGGLE, GPIO_IN);
    gpio_pull_up(BTN_QOS_TOGGLE);
}


int setup_mqttsn_services(void){

    int ret = wifi_udp_create(MQTTSN_GATEWAY_PORT);
    if (ret != WIFI_OK){
        printf("[ERROR] UDP socket creation failed.\n");
        sleep_ms(1000);
        return -1;
    }

    printf("[INFO] UDP socket ready at port %d!\n", MQTTSN_GATEWAY_PORT);
    sleep_ms(500);

    // 2. Initialize MQTT-SN Client
    ret = mqttsn_init(MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT);
    if (ret != MQTTSN_OK){
        printf("[ERROR] MQTT-SN Initialization failed.\n");
        wifi_udp_close();
        sleep_ms(1000);
        return -1;
    }

    mqtt_initialized = true;
    printf("[INFO] MQTT-SN Initialized!\n");

    // 3. Connect to MQTT-SM Gateway (python_mqtt_gateway.py)
    ret = mqttsn_connect("PicoW_Client", 60);
    if (ret != MQTTSN_OK){
        printf("[ERROR] Gateway Connection Failed. Error=%d\n", ret);
        mqtt_initialized = false;
        wifi_udp_close();
        sleep_ms(1000);
        return -1;
    }

    printf("[INFO] Connected to MQTT-SN Gateway, %s:%d!\n", MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT);

    // 4. Register message callback for incoming messages
    mqttsn_set_message_callback(on_message_received);

    // 5. Subscribe to topics
    mqttsn_subscribe("pico/test", QOS_0);
    mqttsn_subscribe("pico/command", QOS_0);
    mqttsn_subscribe("pico/chunks", QOS_0);
    mqttsn_subscribe("pico/block", QOS_0);
    printf("[INFO] Subscribed to: pico/test, pico/command, pico/chunks, pico/block\n");

    mqtt_connected = true;

    printf("[INFO] MQTT-SN is operational!\n");
    return 0;
}

int reinit_mqttsn(void){
    printf("\n============= Reinitializing MQTT-SN After WiFi Recovery =============\n");

    // Clean up old connection if exists
    if (mqtt_connected){
        printf("[INFO] Disconnecting old MQTT-SN session...\n");
        mqttsn_disconnect();
        mqtt_connected = false;
    }

    if (mqtt_initialized){
        printf("[INFO] Closing old UDP socket...\n");
        wifi_udp_close();
        mqtt_initialized = false;
    }

    sleep_ms(1000); // delay to ensure proper cleanup

    // Reinitialise MQTT-SN Client
    return setup_mqttsn_services();
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

    // ========================= Application Init =========================
    if (wifi_is_connected()) {
        
        // Initialize features (MQTT-SN, file transfer etc.)
        
        if (setup_mqttsn_services() == 0){
            printf("[INFO] Application ready\n\n");
        } else {
            printf("[WARNING] MQTT-SN initialization failed - will retry on reconnect\n");
        }

    } else {
        printf("[WARNING] Starting without WiFi - waiting for connection\n");
    }

    // Main Loop
    bool was_connected = wifi_is_connected();   //
    uint32_t last_publish = 0;
    uint32_t sequence_number = 0;
    absolute_time_t last_stats_print = get_absolute_time();

    printf("========================= Entering Main Loop =========================");

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

            if (reinit_mqttsn() == 0 ){
                printf("[INFO] MQTT-SN reinitialized sucessfully\n");
                printf("[INFO] Ready for publish/subscribe\n\n");
            } else {
                printf("[WARNING] MQTT-SN reinitialization failed.\n");
                printf("[WARNING] Will retry in next cycle\n\n");
            }
        }

        // 2. WiFi Disconnected
        if (!is_connected && was_connected){
            printf("[WARNING] WiFi Connection Lost! MQTT-SN service unavailable\n");
            printf("[INFO] Auto-reconnect will attempt to restore WiFi...\n\n");
            mqtt_connected = false;
            mqtt_initialized = false;
        }
        
        // Update was_connected as WiFi connection was lost
        was_connected = is_connected;  

        // 3. WiFi Connected & MQTT-SN Connected
        if (is_connected && mqtt_connected){

            // Publish test messages every 5 seconds
            if (now - last_publish > 5000){
                char message[64];
                snprintf(message, sizeof(message), "seq=%lu,timestamp=%lu", sequence_number, now);
                printf("[%lu ms] Publishing QoS 0: seq=%lu (fire-and-forget)\n", now, sequence_number);

                int ret = mqttsn_publish("pico/data", (uint8_t*)message, strlen(message), 0);
                if (ret == MQTTSN_OK){
                    printf("[MQTT-SN] Published Sucessfully\n");
                    sequence_number++;
                } else {
                    printf("[MQTT-SN] Published Failed\n");
                }

                last_publish = now;
            }

            // Poll for incoming MQTT-SN messages
            mqttsn_poll();

            // Poll for WiFi stack
            cyw43_arch_poll();

        } else if (is_connected && !mqtt_connected){

            printf("[APP] WiFi Connected, attempting MQTT-SN connection...\n");
            if (setup_mqttsn_services() == 0){
                printf("[INFO] MQTT-SN Connected Sucessfully.\n");
            }
            
            printf("[WARNING] MQTT-SN connection attempt failed\n");

        } else {
            // Wait for WiFi Connection
            printf("[APP] Waiting for WiFi... (Status: %s)\n", 
                       wifi_get_status());
        }

        // Print WiFi stats every 60 seconds
        if (absolute_time_diff_us(last_stats_print, get_absolute_time()) > 60000000) {
            printf("========================= System Statistics =========================");
            wifi_print_stats();
            printf("Message Published: %lu\n", sequence_number);
            printf("System Uptime: %lu seconds\n", now/1000);
            last_stats_print = get_absolute_time();
        }

        cyw43_arch_poll(); // network processing
        
        sleep_ms(10);
    }

    return 0;
}
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"

#include "network_config.h"
#include "wifi_driver.h"
#include "udp_driver.h"
#include "network_errors.h"
#include "mqttsn_client.h"
#include "hardware/gpio.h"

#define QOS_TOGGLE 22  // GP22

// Button debouncing
static volatile uint32_t last_button_press = 0;
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

int main(){
    stdio_init_all();
    sleep_ms(3000); // Provide time for serial monitor to connect

    printf("\n=== MQTT-SN Pico W Client Starting ===\n");

    // ========================= Button Setup =========================
    gpio_init(QOS_TOGGLE);
    gpio_set_dir(QOS_TOGGLE, GPIO_IN);
    gpio_pull_up(QOS_TOGGLE);  // Enable pull-up (button connects to GND)
    
    // Enable interrupt on falling edge (button press)
    gpio_set_irq_enabled_with_callback(QOS_TOGGLE, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    
    printf("[BUTTON] GP22 configured for QoS toggle (pull-up enabled)\n");
    printf("[INFO] Press button to cycle: QoS 0 -> QoS 1 -> QoS 2 -> QoS 0\n");

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
            cyw43_arch_poll();

            if (!mqtt_demo_started){
                printf("\n[TEST] Initializing MQTT-SN client...\n");
                printf("[DEBUG] Using gateway: %s:%d\n", MQTTSN_GATEWAY_IP, MQTTSN_GATEWAY_PORT);
                printf("[INFO] Initial QoS level: %d\n", mqttsn_get_qos());
                
                if (mqttsn_demo_init(0) == 0){
                    printf("[TEST] ✓✓✓ SUCCESS: MQTT-SN client fully initialized and connected ✓✓✓\n");
                    mqtt_demo_started = true;
                    last_publish = to_ms_since_boot(get_absolute_time());
                    printf("[MQTTSN] Ready to publish with QoS %d\n", mqttsn_get_qos());
                } else {
                    printf("[TEST] ✗ FAILED: MQTT-SN initialization failed\n");
                    printf("[INFO] Will retry in 10 seconds...\n");
                    sleep_ms(10000);
                }
            } else {
                // Process incoming MQTT-SN messages
                int processed = mqttsn_demo_process_once(100);
                if (processed > 0) {
                    printf("[MQTTSN] Processed incoming message (%d bytes)\n", processed);
                } else if (processed == -1) {
                    printf("[MQTTSN] Connection lost - will reconnect...\n");
                    mqtt_demo_started = false;
                    mqttsn_demo_close();
                    sleep_ms(5000);
                    continue;
                }

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
            }

        } else {
            if (now % 5000 < 100) {
                printf("[APP] Waiting for WiFi... (Status: %s)\n", wifi_get_status());
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
        }

        cyw43_arch_poll();
        sleep_ms(10);
    }

    mqttsn_demo_close();
    return 0;
}
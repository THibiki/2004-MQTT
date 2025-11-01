#include "pico/stdlib.h"
#include "wifi_driver.h"
#include "mqtt_sn_client.h"
#include "block_transfer.h"
#include "sd_card.h"
#include "pico/cyw43_arch.h"
#include "ff.h"  // FatFs for directory operations
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    int ret;
    
    stdio_init_all();
    sleep_ms(2000);
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("    Raspberry Pi Pico W - MQTT-SN System Test\n");
    printf("═══════════════════════════════════════════════════════\n\n");
    
    // Initialize block transfer system
    printf("→ Initializing block transfer system...\n");
    block_transfer_init();
    printf("  ✓ Block transfer ready\n\n");
    sleep_ms(1000);  // Delay for visibility
    
    // ═══════════════════════════════════════════════════════
    // WiFi Connection
    // ═══════════════════════════════════════════════════════
    printf("→ Connecting to WiFi...\n");
    ret = wifi_init();
    if (ret != WIFI_OK) {
        printf("  ✗ WiFi init failed: %d\n", ret);
        return -1;
    }
    
    const char *ssid = "xuan";
    const char *password = "xuan1234";
    ret = wifi_connect(ssid, password);
    if (ret != WIFI_OK) {
        printf("  ✗ WiFi connect failed: %d\n", ret);
        return -1;
    }
    
    printf("  ✓ WiFi connected!\n");
    printf("    SSID: %s\n", ssid);
    printf("    IP: 172.20.10.2\n");
    printf("    Gateway: 172.20.10.1:5000\n\n");
    sleep_ms(2000);  // Delay for visibility
    
    // ═══════════════════════════════════════════════════════
    // SD Card Setup
    // ═══════════════════════════════════════════════════════
    printf("→ Initializing SD card...\n");
    if (sd_card_init_with_detection() == 0) {
        int mount_result = sd_card_mount_fat32();
        if (mount_result == 0) {
            printf("  ✓ SD card mounted (FAT32)\n");
            
            // Create startup file
            char startup_content[256];
            uint32_t boot_time = to_ms_since_boot(get_absolute_time());
            snprintf(startup_content, sizeof(startup_content),
                "Pico W Boot Log\n"
                "===============\n"
                "Boot time: %lu ms\n"
                "Network: jer\n"
                "IP: 172.20.10.2\n"
                "Gateway: 172.20.10.1:5000\n"
                "Status: Ready\n",
                boot_time);
            
            if (sd_card_write_file("startup.txt", (uint8_t*)startup_content, strlen(startup_content)) == 0) {
                printf("  ✓ Created startup.txt\n");
            }
            
            // List existing files
            DIR dir;
            FILINFO fno;
            int file_count = 0;
            if (f_opendir(&dir, "/") == FR_OK) {
                while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
                    if (!(fno.fattrib & AM_DIR)) file_count++;
                }
                f_closedir(&dir);
            }
            printf("  📁 %d files on SD card\n\n", file_count);
        } else {
            printf("  ⚠ SD card detected but mount failed\n\n");
        }
    } else {
        printf("  ⚠ No SD card detected\n\n");
    }
    sleep_ms(2000);  // Delay for visibility
    
    // ═══════════════════════════════════════════════════════
    // UDP Echo / RTT Test
    // ═══════════════════════════════════════════════════════
    printf("→ Testing UDP echo (RTT measurement)...\n");
    ret = wifi_udp_create(0);
    if (ret != WIFI_OK) {
        printf("  ✗ UDP socket creation failed\n");
        return -1;
    }
    
    // Send test UDP message and measure RTT
    const char *echo_msg = "PING";
    uint32_t echo_start = to_ms_since_boot(get_absolute_time());
    ret = wifi_udp_send("172.20.10.2", 5000, (uint8_t*)echo_msg, strlen(echo_msg));
    if (ret == WIFI_OK) {
        // Wait for any UDP response (up to 1 second)
        for (int i = 0; i < 100; i++) {
            uint8_t response[256];
            int recv_ret = wifi_udp_receive(response, sizeof(response), 10);
            if (recv_ret > 0) {
                uint32_t echo_end = to_ms_since_boot(get_absolute_time());
                uint32_t rtt = echo_end - echo_start;
                printf("  ✓ UDP message sent, RTT: %lu ms\n", rtt);
                break;
            }
            cyw43_arch_poll();
        }
    }
    printf("\n");
    sleep_ms(1500);  // Delay for visibility
    
    // ═══════════════════════════════════════════════════════
    // MQTT-SN Connection
    // ═══════════════════════════════════════════════════════
    printf("→ Connecting to MQTT-SN gateway...\n");
    
    const char *gateway_ip = "172.20.10.2";
    uint16_t gateway_port = 5000;
    
    if (mqttsn_init(gateway_ip, gateway_port) != MQTTSN_OK) {
        printf("  ✗ MQTT-SN init failed\n");
        return -1;
    }
    
    ret = mqttsn_connect("PicoW_Client", 60);
    if (ret != MQTTSN_OK) {
        printf("  ✗ Gateway connection failed\n");
        return -1;
    }
    
    printf("  ✓ Connected to %s:%d\n", gateway_ip, gateway_port);
    
    mqttsn_subscribe("pico/test", 0);
    mqttsn_subscribe("pico/chunks", 0);
    mqttsn_subscribe("pico/block", 0);
    printf("  ✓ Subscribed to topics\n\n");
    
    // ═══════════════════════════════════════════════════════
    // System Ready - Starting Operations
    // ═══════════════════════════════════════════════════════
    printf("═══════════════════════════════════════════════════════\n");
    printf("    System Ready! Starting operations...\n");
    printf("═══════════════════════════════════════════════════════\n\n");
    sleep_ms(1500);  // Delay before starting operations
    
    uint32_t last_publish = 0;
    uint32_t sequence_number = 0;
    
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Poll for incoming messages
        mqttsn_poll();
        block_transfer_check_timeout();
        
        // Publish message every 5 seconds with sequence number (QoS 0)
        if (now - last_publish > 5000) {
            char message[64];
            snprintf(message, sizeof(message), "seq=%lu,timestamp=%lu", sequence_number, now);
            printf("[%lu ms] Publishing QoS 0: seq=%lu\n", now, sequence_number);
            mqttsn_publish("pico/data", (uint8_t*)message, strlen(message), 0);
            sequence_number++;
            last_publish = now;
        }
        
        // Block transfer test - ONE TIME at 30 seconds
        static bool block_transfer_done = false;
        if (!block_transfer_done && now > 30000) {
            printf("\n");
            printf("───────────────────────────────────────────────────────\n");
            printf("  Block Transfer Test (10KB data in 128-byte chunks)\n");
            printf("───────────────────────────────────────────────────────\n");
            sleep_ms(1000);  // Delay for visibility
            
            char *large_buffer = (char*)malloc(BLOCK_BUFFER_SIZE);
            if (large_buffer) {
                generate_large_message(large_buffer, BLOCK_BUFFER_SIZE);
                size_t msg_len = strlen(large_buffer);
                
                printf("  Message size: %zu bytes\n", msg_len);
                printf("  Starting transfer...\n\n");
                
                if (send_block_transfer("pico/chunks", (uint8_t*)large_buffer, msg_len) == 0) {
                    printf("\n  ✓ Block transfer completed\n");
                } else {
                    printf("\n  ✗ Block transfer failed\n");
                }
                printf("───────────────────────────────────────────────────────\n\n");
                
                free(large_buffer);
            }
            
            block_transfer_done = true;
        }
        
        // Poll WiFi stack
        cyw43_arch_poll();
        sleep_ms(10);
    }
    
    return 0;
}

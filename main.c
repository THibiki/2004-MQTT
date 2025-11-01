#include "pico/stdlib.h"
#include "wifi_driver.h"
#include "mqtt_sn_client.h"
#include "block_transfer.h"
#include "sd_card.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"
#include "ff.h"  // FatFs for directory operations
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Button GPIO definitions (Maker Pico W buttons)
#define BTN_WIFI_INIT     20  // Button 1: Initialize WiFi & MQTT
#define BTN_BLOCK_TRANSFER 21 // Button 2: Start image transfer
#define BTN_QOS_TOGGLE    22  // Button 3: Toggle QoS mode (0 or 1)

// Debounce settings
#define DEBOUNCE_MS 200

// Global state
static bool wifi_initialized = false;
static bool mqtt_connected = false;
static bool sd_card_mounted = false;
static uint8_t current_qos = 1;  // Default QoS 1
static uint32_t last_button_press[3] = {0};
static uint32_t last_sd_check = 0;
#define SD_CHECK_INTERVAL_MS 1000  // Check SD card every second

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

// Check if SD card is present and mounted
bool check_sd_card_status() {
    // Check hardware initialization status first
    if (!sd_card_is_initialized()) {
        return false;
    }
    
    // Try to access SD card by checking if we can open the root directory
    DIR dir;
    FRESULT res = f_opendir(&dir, "/");
    if (res == FR_OK) {
        f_closedir(&dir);
        
        // Double-check by trying to read a file
        FIL test_file;
        FRESULT test_res = f_open(&test_file, "/download.jpg", FA_READ);
        if (test_res == FR_OK) {
            f_close(&test_file);
            return true;
        }
    }
    return false;
}

// Initialize or re-initialize SD card (called when needed)
bool initialize_sd_card() {
    printf("→ Initializing SD card...\n");
    
    // Force complete unmount and hardware deinit to clear any stale state
    f_unmount("/");
    sd_card_deinit();
    sleep_ms(300);
    
    // Try up to 3 times
    for (int attempt = 1; attempt <= 3; attempt++) {
        if (attempt > 1) {
            printf("  Retry attempt %d/3...\n", attempt);
            sleep_ms(1000);  // Longer delay between retries
        }
        
        // Complete hardware re-initialization
        int init_result = sd_card_init_with_detection();
        if (init_result == 0) {
            printf("  Hardware initialized, mounting filesystem...\n");
            sleep_ms(500);  // Longer stabilization delay
            
            int mount_result = sd_card_mount_fat32();
            if (mount_result == 0) {
                printf("  Filesystem mounted, verifying access...\n");
                sleep_ms(200);
                
                // Verify filesystem access
                DIR dir;
                FILINFO fno;
                int file_count = 0;
                
                FRESULT dir_res = f_opendir(&dir, "/");
                if (dir_res == FR_OK) {
                    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
                        if (!(fno.fattrib & AM_DIR)) file_count++;
                    }
                    f_closedir(&dir);
                    
                    // Test file access to ensure filesystem is truly working
                    FIL test_file;
                    FRESULT test_res = f_open(&test_file, "/download.jpg", FA_READ);
                    if (test_res == FR_OK) {
                        f_close(&test_file);
                        printf("  ✓ SD card fully operational!\n");
                        printf("  📁 %d files on SD card\n\n", file_count);
                        return true;
                    } else {
                        printf("  ✗ File access test failed (FR: %d)\n", test_res);
                        f_unmount("/");
                        sleep_ms(200);
                    }
                } else {
                    printf("  ✗ Directory access failed (FR: %d)\n", dir_res);
                    f_unmount("/");
                    sleep_ms(200);
                }
            } else {
                printf("  ✗ Mount failed (code: %d)\n", mount_result);
            }
        } else {
            printf("  ✗ Hardware init failed (code: %d)\n", init_result);
        }
    }
    
    printf("  ⚠ SD card initialization failed after 3 attempts\n\n");
    return false;
}

// Wait for SD card to be inserted
void wait_for_sd_card() {
    printf("\n⚠️  SD CARD REMOVED!\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("    Please insert SD card to continue...\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("Waiting");
    fflush(stdout);
    
    sd_card_mounted = false;
    
    // Completely unmount the filesystem and deinitialize hardware
    f_unmount("/");
    sd_card_deinit();  // Force complete hardware reset
    sleep_ms(200);
    
    while (!sd_card_mounted) {
        sleep_ms(1000);  // Check every second to allow card to stabilize
        
        printf(".");
        fflush(stdout);
        
        // Multiple attempts to reinitialize
        for (int attempt = 1; attempt <= 2; attempt++) {
            // Force hardware deinit before each attempt
            sd_card_deinit();
            sleep_ms(200);
            
            // Complete hardware re-initialization
            int init_result = sd_card_init_with_detection();
            if (init_result == 0) {
                printf("\n→ SD card detected (attempt %d), mounting...\n", attempt);
                
                // Wait for card to stabilize
                sleep_ms(300);
                
                int mount_result = sd_card_mount_fat32();
                if (mount_result == 0) {
                    // Critical: Verify filesystem by actually trying to access it
                    DIR dir;
                    FILINFO fno;
                    int file_count = 0;
                    
                    FRESULT res = f_opendir(&dir, "/");
                    if (res == FR_OK) {
                        // List files to fully verify access
                        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
                            if (!(fno.fattrib & AM_DIR)) {
                                file_count++;
                            }
                        }
                        f_closedir(&dir);
                        
                        // Try to actually read a file to ensure it's really working
                        printf("  Testing file access...\n");
                        FIL test_file;
                        FRESULT test_res = f_open(&test_file, "/download.jpg", FA_READ);
                        if (test_res == FR_OK) {
                            f_close(&test_file);
                            printf("✓ SD card fully operational!\n");
                            printf("📁 %d files found on SD card\n", file_count);
                            printf("Resuming operations...\n\n");
                            
                            sd_card_mounted = true;
                            return;
                        } else {
                            printf("✗ File access test failed (FR: %d)\n", test_res);
                            f_unmount("/");
                            sleep_ms(500);
                        }
                    } else {
                        printf("✗ Directory access failed (FR: %d)\n", res);
                        f_unmount("/");
                        sleep_ms(500);
                    }
                } else {
                    printf("✗ Mount failed (result: %d)\n", mount_result);
                    sleep_ms(500);
                }
            }
            
            // If first attempt fails, wait before retry
            if (attempt == 1 && !sd_card_mounted) {
                sleep_ms(500);
            }
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

int main() {
    int ret;
    
    stdio_init_all();
    sleep_ms(2000);
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("    Raspberry Pi Pico W - MQTT-SN Button Control\n");
    printf("═══════════════════════════════════════════════════════\n\n");
    
    // Initialize buttons
    printf("→ Initializing button controls...\n");
    buttons_init();
    printf("  ✓ Buttons ready:\n");
    printf("    • GPIO %d: WiFi & MQTT Init\n", BTN_WIFI_INIT);
    printf("    • GPIO %d: Image Transfer\n", BTN_BLOCK_TRANSFER);
    printf("    • GPIO %d: Toggle QoS (current: QoS %d)\n\n", BTN_QOS_TOGGLE, current_qos);
    sleep_ms(1000);
    
    // Initialize block transfer system
    printf("→ Initializing block transfer system...\n");
    block_transfer_init();
    printf("  ✓ Block transfer ready\n\n");
    sleep_ms(1000);  // Delay for visibility
    
    // ═══════════════════════════════════════════════════════
    // Display User Menu
    // ═══════════════════════════════════════════════════════
    printf("═══════════════════════════════════════════════════════\n");
    printf("                    CONTROL MENU                       \n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("  GPIO %d: Initialize WiFi & MQTT Connection\n", BTN_WIFI_INIT);
    printf("  GPIO %d: Transfer Image (download.jpg)\n", BTN_BLOCK_TRANSFER);
    printf("  GPIO %d: Toggle QoS Mode\n", BTN_QOS_TOGGLE);
    printf("           • QoS 0 - Fast, publishes seq messages\n");
    printf("           • QoS 1 - Reliable, waits for PUBACK\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("  Current QoS: %d\n", current_qos);
    printf("═══════════════════════════════════════════════════════\n\n");
    
    uint32_t last_publish = 0;
    uint32_t sequence_number = 0;
    
    // Main loop
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // ═══════════════════════════════════════════════════════
        // Button 1: WiFi & MQTT Initialization (GPIO 20)
        // ═══════════════════════════════════════════════════════
        if (!wifi_initialized && button_pressed(BTN_WIFI_INIT, 0)) {
            printf("\n🔘 Button pressed: Initializing WiFi & MQTT...\n\n");
            
            // WiFi Connection
            printf("→ Connecting to WiFi...\n");
            ret = wifi_init();
            if (ret != WIFI_OK) {
                printf("  ✗ WiFi init failed: %d\n", ret);
                sleep_ms(1000);
                continue;
            }
            
            const char *ssid = "jer";
            const char *password = "jeraldgoh";
            ret = wifi_connect(ssid, password);
            if (ret != WIFI_OK) {
                printf("  ✗ WiFi connect failed: %d\n", ret);
                sleep_ms(1000);
                continue;
            }
            
            printf("  ✓ WiFi connected!\n");
            printf("    SSID: %s\n", ssid);
            printf("    IP: 172.20.10.2\n");
            printf("    Gateway: 172.20.10.14:1884\n\n");
            sleep_ms(1000);
            
            // SD Card Setup
            sd_card_mounted = initialize_sd_card();
            
            if (!sd_card_mounted) {
                printf("  → You can try again by pressing GPIO %d when card is inserted\n\n", BTN_BLOCK_TRANSFER);
            }
            
            sleep_ms(1000);
            
            // UDP Socket Setup
            printf("→ Setting up UDP socket...\n");
            ret = wifi_udp_create(1884);
            if (ret != WIFI_OK) {
                printf("  ✗ UDP socket creation failed\n");
                sleep_ms(1000);
                continue;
            }
            printf("  ✓ UDP socket ready on port 1884\n\n");
            sleep_ms(500);
            
            // MQTT-SN Connection
            printf("→ Connecting to MQTT-SN gateway...\n");
            
            const char *gateway_ip = "172.20.10.14";
            uint16_t gateway_port = 1884;
            
            if (mqttsn_init(gateway_ip, gateway_port) != MQTTSN_OK) {
                printf("  ✗ MQTT-SN init failed\n");
                sleep_ms(1000);
                continue;
            }
            
            ret = mqttsn_connect("PicoW_Client", 60);
            if (ret != MQTTSN_OK) {
                printf("  ✗ Gateway connection failed\n");
                sleep_ms(1000);
                continue;
            }
            
            printf("  ✓ Connected to %s:%d\n", gateway_ip, gateway_port);
            
            mqttsn_subscribe("pico/test", 0);
            mqttsn_subscribe("pico/chunks", 0);
            mqttsn_subscribe("pico/block", 0);
            printf("  ✓ Subscribed to topics\n\n");
            
            wifi_initialized = true;
            mqtt_connected = true;
            
            printf("═══════════════════════════════════════════════════════\n");
            printf("    System Ready!\n");
            printf("    • GPIO %d: Transfer image (QoS %d)\n", BTN_BLOCK_TRANSFER, current_qos);
            printf("    • GPIO %d: Toggle QoS\n", BTN_QOS_TOGGLE);
            printf("═══════════════════════════════════════════════════════\n\n");
        }
        
        // ═══════════════════════════════════════════════════════
        // Button 2: Image Transfer (GPIO 21)
        // ═══════════════════════════════════════════════════════
        if (button_pressed(BTN_BLOCK_TRANSFER, 1)) {
            // Check if WiFi is initialized
            if (!wifi_initialized || !mqtt_connected) {
                printf("\n⚠️  WiFi not initialized!\n");
                printf("    Please press GPIO %d to initialize WiFi first.\n\n", BTN_WIFI_INIT);
                continue;
            }
            
            // Check if SD card is mounted, if not try to initialize it
            if (!sd_card_mounted || !check_sd_card_status()) {
                printf("\n🔘 Button pressed: Image transfer requested\n");
                
                if (!sd_card_mounted) {
                    printf("   → SD card not mounted\n\n");
                } else {
                    printf("   → SD card status check failed (may have been removed)\n\n");
                    sd_card_mounted = false;  // Update state
                }
                
                // Try to initialize SD card
                if (initialize_sd_card()) {
                    sd_card_mounted = true;
                } else {
                    // If initialization fails, wait for user to insert card
                    printf("═══════════════════════════════════════════════════════\n");
                    printf("    Please insert SD card and try again...\n");
                    printf("═══════════════════════════════════════════════════════\n\n");
                    continue;
                }
            }
            
            printf("\n🔘 Button pressed: Starting image transfer (QoS %d)...\n\n", current_qos);
            
            printf("───────────────────────────────────────────────────────\n");
            printf("  📸 Image Transfer & Status Log Creation\n");
            printf("───────────────────────────────────────────────────────\n");
            
            // Create status log file with Pico W details
            char status_log[1024];
            uint32_t uptime = to_ms_since_boot(get_absolute_time());
            uint32_t uptime_sec = uptime / 1000;
            uint32_t hours = uptime_sec / 3600;
            uint32_t minutes = (uptime_sec % 3600) / 60;
            uint32_t seconds = uptime_sec % 60;
            
            snprintf(status_log, sizeof(status_log),
                "═══════════════════════════════════════════════════════\n"
                "    Raspberry Pi Pico W - Status Report\n"
                "═══════════════════════════════════════════════════════\n"
                "\n"
                "SYSTEM INFORMATION\n"
                "──────────────────────────────────────────────────────\n"
                "Device:           Raspberry Pi Pico W\n"
                "Board:            pico_w\n"
                "Uptime:           %02lu:%02lu:%02lu (%lu ms)\n"
                "Timestamp:        %lu ms since boot\n"
                "\n"
                "NETWORK CONFIGURATION\n"
                "──────────────────────────────────────────────────────\n"
                "WiFi Status:      Connected\n"
                "SSID:             jer\n"
                "IP Address:       172.20.10.2\n"
                "Gateway:          172.20.10.14:1884\n"
                "Protocol:         MQTT-SN over UDP\n"
                "\n"
                "MQTT-SN CONNECTION\n"
                "──────────────────────────────────────────────────────\n"
                "Client ID:        PicoW_Client\n"
                "Gateway IP:       172.20.10.14\n"
                "Gateway Port:     1884\n"
                "Status:           Connected\n"
                "Subscribed:       pico/test, pico/chunks, pico/block\n"
                "\n"
                "TRANSFER SETTINGS\n"
                "──────────────────────────────────────────────────────\n"
                "Current QoS:      %d\n"
                "Block Size:       128 bytes per chunk\n"
                "Max Buffer:       10240 bytes (10 KB)\n"
                "Chunk Header:     8 bytes (block_id, part_num, total, len)\n"
                "Data per Chunk:   120 bytes\n"
                "\n"
                "IMAGE TRANSFER\n"
                "──────────────────────────────────────────────────────\n"
                "Source File:      download.jpg\n"
                "Location:         SD Card (FAT32)\n"
                "Topic:            pico/block\n"
                "QoS Mode:         %d (%s)\n"
                "Transfer Status:  Starting...\n"
                "\n"
                "═══════════════════════════════════════════════════════\n"
                "    Log created at %lu ms\n"
                "═══════════════════════════════════════════════════════\n",
                hours, minutes, seconds, uptime,
                uptime,
                current_qos,
                current_qos, (current_qos == 0) ? "Fire-and-Forget" : "Reliable with PUBACK",
                uptime
            );
            
            printf("\n  → Writing status log to SD card...\n");
            if (sd_card_write_file("transfer_log.txt", (uint8_t*)status_log, strlen(status_log)) == 0) {
                printf("  ✓ Status log saved: transfer_log.txt\n");
            } else {
                printf("  ✗ Failed to save status log\n");
            }
            
            const char *image_file = "download.jpg";
            printf("\n  → Looking for %s on SD card...\n", image_file);
            
            // Send image with current QoS setting
            if (send_image_file_qos("pico/block", image_file, current_qos) == 0) {
                printf("\n  ✓ Image transfer completed (QoS %d)\n", current_qos);
                
                // Update log with completion status
                char completion_note[256];
                uint32_t completion_time = to_ms_since_boot(get_absolute_time());
                uint32_t transfer_duration = completion_time - uptime;
                snprintf(completion_note, sizeof(completion_note),
                    "\n[TRANSFER COMPLETE]\n"
                    "Completion Time: %lu ms\n"
                    "Transfer Duration: %lu ms (%.2f seconds)\n"
                    "Status: SUCCESS\n",
                    completion_time, transfer_duration, transfer_duration / 1000.0f
                );
                
                // Append to log file
                printf("  → Updating status log...\n");
                // Note: This appends the completion status
                char final_log[1536];
                snprintf(final_log, sizeof(final_log), "%s%s", status_log, completion_note);
                if (sd_card_write_file("transfer_log.txt", (uint8_t*)final_log, strlen(final_log)) == 0) {
                    printf("  ✓ Status log updated with completion info\n");
                }
            } else {
                printf("\n  ✗ Image transfer failed\n");
            }
            printf("───────────────────────────────────────────────────────\n\n");
        }
        
        // ═══════════════════════════════════════════════════════
        // Button 3: Toggle QoS (GPIO 22)
        // ═══════════════════════════════════════════════════════
        if (button_pressed(BTN_QOS_TOGGLE, 2)) {
            current_qos = (current_qos == 0) ? 1 : 0;
            printf("\n🔘 Button pressed: QoS toggled to %d\n", current_qos);
            if (current_qos == 0) {
                printf("   → QoS 0 mode: Will publish sequence messages every 5s\n\n");
            } else {
                printf("   → QoS 1 mode: Reliable image transfers with PUBACK\n\n");
            }
        }
        
        // ═══════════════════════════════════════════════════════
        // QoS 0 Mode: Publish sequence messages every 5 seconds
        // ═══════════════════════════════════════════════════════
        if (mqtt_connected && current_qos == 0 && (now - last_publish > 5000)) {
            char message[64];
            snprintf(message, sizeof(message), "seq=%lu,timestamp=%lu", sequence_number, now);
            printf("[%lu ms] Publishing QoS 0: seq=%lu (fire-and-forget)\n", now, sequence_number);
            mqttsn_publish("pico/data", (uint8_t*)message, strlen(message), 0);
            sequence_number++;
            last_publish = now;
        }
        
        // ═══════════════════════════════════════════════════════
        // QoS 1 Mode: Publish sequence messages every 5 seconds
        // ═══════════════════════════════════════════════════════
        if (mqtt_connected && current_qos == 1 && (now - last_publish > 5000)) {
            char message[64];
            snprintf(message, sizeof(message), "seq=%lu,timestamp=%lu", sequence_number, now);
            printf("[%lu ms] Publishing QoS 1: seq=%lu (waiting for PUBACK...)\n", now, sequence_number);
            
            int ret = mqttsn_publish("pico/data", (uint8_t*)message, strlen(message), 1);
            if (ret == MQTTSN_OK) {
                printf("         ✓ PUBACK received for seq=%lu\n", sequence_number);
            } else {
                printf("         ✗ PUBACK timeout for seq=%lu\n", sequence_number);
            }
            
            sequence_number++;
            last_publish = now;
        }
        
        // ═══════════════════════════════════════════════════════
        // Periodic SD Card Check (every 500ms for faster detection)
        // ═══════════════════════════════════════════════════════
        if (sd_card_mounted && (now - last_sd_check > 500)) {
            last_sd_check = now;
            
            // Check if SD card is still accessible
            bool sd_ok = check_sd_card_status();
            if (!sd_ok) {
                printf("\n[%lu ms] ⚠️  SD CARD REMOVAL DETECTED! (periodic check)\n", now);
                // SD card was removed, wait for it to be re-inserted
                wait_for_sd_card();
                // Reset the check timer after recovery
                last_sd_check = to_ms_since_boot(get_absolute_time());
            }
        }
        
        // Poll for incoming messages if connected
        if (mqtt_connected) {
            mqttsn_poll();
            block_transfer_check_timeout();
        }
        
        // Poll WiFi stack if initialized
        if (wifi_initialized) {
            cyw43_arch_poll();
        }
        
        sleep_ms(10);
    }
    
    return 0;
}

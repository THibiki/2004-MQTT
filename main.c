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
static uint8_t current_qos = 0;  // Start with QoS 0
static uint8_t qos_mode = 2;     // 0=QoS0, 1=QoS1, 2=Stopped (start in Stopped mode)
static uint32_t last_button_press[3] = {0};
static uint32_t last_sd_check = 0;
#define SD_CHECK_INTERVAL_MS 1000  // Check SD card every second

// Image filename configuration
static char selected_image[64] = "download.jpg";  // Default filename
static int image_file_count = 0;
static char image_files[10][64];  // Store up to 10 image filenames

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

// Scan SD card for image files and automatically select the first one found
bool scan_and_select_image() {
    if (!sd_card_mounted) {
        printf("  ⚠ SD card not mounted\n");
        return false;
    }
    
    DIR dir;
    FILINFO fno;
    image_file_count = 0;
    
    printf("\n📸 Scanning for image files...\n");
    
    FRESULT res = f_opendir(&dir, "/");
    if (res != FR_OK) {
        printf("  ✗ Failed to open directory (FR: %d)\n", res);
        return false;
    }
    
    // Scan for .jpg and .jpeg files
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
        if (!(fno.fattrib & AM_DIR)) {
            size_t len = strlen(fno.fname);
            // Check for .jpg or .jpeg extension (case insensitive)
            if (len > 4) {
                const char *ext = &fno.fname[len - 4];
                if (strcasecmp(ext, ".jpg") == 0 || 
                    (len > 5 && strcasecmp(&fno.fname[len - 5], ".jpeg") == 0)) {
                    if (image_file_count < 10) {
                        strncpy(image_files[image_file_count], fno.fname, 63);
                        image_files[image_file_count][63] = '\0';
                        printf("  [%d] %s (%lu bytes)\n", 
                               image_file_count + 1, fno.fname, fno.fsize);
                        image_file_count++;
                    }
                }
            }
        }
    }
    f_closedir(&dir);
    
    if (image_file_count == 0) {
        printf("  ⚠ No .jpg/.jpeg files found on SD card\n");
        return false;
    }
    
    // Auto-select the first image found
    strncpy(selected_image, image_files[0], 63);
    selected_image[63] = '\0';
    
    printf("\n  ✓ Auto-selected: %s\n", selected_image);
    if (image_file_count > 1) {
        printf("  ℹ Found %d image file(s) - using first one\n", image_file_count);
    }
    printf("\n");
    
    return true;
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
        printf("  🔔 Matched pico/test - preparing response...\n");
        // Echo back with "ACK: " prefix
        char response[128];
        snprintf(response, sizeof(response), "ACK: %.*s", (int)len, data);
        printf("  📤 Sending response (QoS 0): %s\n", response);
        // Always use QoS 0 for responses to avoid blocking in callback
        int ret = mqttsn_publish("pico/response", (uint8_t*)response, strlen(response), 0);
        if (ret == MQTTSN_OK) {
            printf("  ✅ Response sent successfully to pico/response\n\n");
        } else {
            printf("  ❌ Response send failed (ret=%d)\n\n", ret);
        }
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

// Check if SD card is accessible (returns true if mounted and working)
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
        // Just verify directory access is working
        return true;
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
                    printf("  📁 Files on SD card:\n");
                    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
                        if (!(fno.fattrib & AM_DIR)) {
                            file_count++;
                            printf("     • %s (%lu bytes)\n", fno.fname, fno.fsize);
                        }
                    }
                    f_closedir(&dir);
                    
                    if (file_count > 0) {
                        printf("  ✓ SD card fully operational!\n");
                        printf("  ✓ Found %d file(s)\n\n", file_count);
                        return true;
                    } else {
                        printf("  ⚠ No files found on SD card\n");
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
                        
                        // Verify we found at least some files
                        printf("  Found %d files on SD card\n", file_count);
                        
                        if (file_count > 0) {
                            printf("✓ SD card fully operational!\n");
                            printf("📁 %d files found on SD card\n", file_count);
                            printf("Resuming operations...\n\n");
                            
                            sd_card_mounted = true;
                            return;
                        } else {
                            printf("⚠ No files found on SD card\n");
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
    printf("    • GPIO %d: Cycle Mode (QoS0 → QoS1 → Stop → ...)\n\n", BTN_QOS_TOGGLE);
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
    printf("  GPIO %d: Transfer Image (auto-detects .jpg from SD)\n", BTN_BLOCK_TRANSFER);
    printf("  GPIO %d: Toggle QoS Mode\n", BTN_QOS_TOGGLE);
    printf("           • QoS 0 - Fast, publishes seq messages\n");
    printf("           • QoS 1 - Reliable, waits for PUBACK\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("  Current QoS: %d\n", current_qos);
    printf("═══════════════════════════════════════════════════════\n\n");
    
    uint32_t last_publish = 0;
    uint32_t sequence_number = 0;
    
    // Retry and failure tracking for QoS 1
    uint32_t consecutive_failures = 0;
    uint32_t total_failed_publishes = 0;
    
    // WiFi reconnection tracking
    uint32_t last_wifi_check = 0;
    #define WIFI_CHECK_INTERVAL_MS 5000  // Check WiFi every 5 seconds
    
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
            printf("  ✓ UDP socket ready on port 1884\n");
            
            // Connect UDP to gateway to lock source port
            const char *gateway_ip = "172.20.10.14";
            uint16_t gateway_port = 1884;
            
            ret = wifi_udp_connect_remote(gateway_ip, gateway_port);
            if (ret != WIFI_OK) {
                printf("  ✗ Failed to connect UDP to gateway\n");
                sleep_ms(1000);
                continue;
            }
            printf("  ✓ UDP connected to gateway (source port locked)\n\n");
            sleep_ms(500);
            
            // MQTT-SN Connection
            printf("→ Connecting to MQTT-SN gateway...\n");
            
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
            
            // Register message callback for incoming messages
            mqttsn_set_message_callback(on_message_received);
            
            // Subscribe to topics
            mqttsn_subscribe("pico/test", 0);
            mqttsn_subscribe("pico/command", 0);
            mqttsn_subscribe("pico/chunks", 0);
            mqttsn_subscribe("pico/block", 0);
            printf("  ✓ Subscribed to topics: pico/test, pico/command, pico/chunks, pico/block\n\n");
            
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
            
            // Scan for image files and auto-select first
            if (!scan_and_select_image()) {
                printf("  ✗ No image files found. Please add .jpg/.jpeg files to SD card.\n\n");
                continue;
            }
            
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
                "Source File:      %s\n"
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
                selected_image,
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
            
            printf("\n  → Transferring: %s\n", selected_image);
            
            // Send image with current QoS setting
            if (send_image_file_qos("pico/block", selected_image, current_qos) == 0) {
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
            // Cycle through: QoS 0 → QoS 1 → Stopped → QoS 0 → ...
            qos_mode = (qos_mode + 1) % 3;
            
            printf("\n🔘 Button pressed: Mode changed\n");
            if (qos_mode == 0) {
                current_qos = 0;
                printf("   → QoS 0 mode: Publishing every 5s (fire-and-forget)\n\n");
            } else if (qos_mode == 1) {
                current_qos = 1;
                printf("   → QoS 1 mode: Publishing every 5s (with PUBACK)\n\n");
            } else {
                printf("   → STOPPED: No publishing\n\n");
            }
        }
        
        // ═══════════════════════════════════════════════════════
        // QoS 0 Mode: Publish sequence messages every 5 seconds
        // ═══════════════════════════════════════════════════════
        if (mqtt_connected && qos_mode == 0 && (now - last_publish > 5000)) {
            char message[64];
            snprintf(message, sizeof(message), "seq=%lu,timestamp=%lu", sequence_number, now);
            printf("[%lu ms] Publishing QoS 0: seq=%lu (fire-and-forget)\n", now, sequence_number);
            mqttsn_publish("pico/data", (uint8_t*)message, strlen(message), 0);
            sequence_number++;
            last_publish = now;
        }
        
        // ═══════════════════════════════════════════════════════
        // QoS 1 Mode: Publish with retry logic and gateway-down detection
        // ═══════════════════════════════════════════════════════
        if (mqtt_connected && qos_mode == 1 && (now - last_publish > 5000)) {
            char message[64];
            snprintf(message, sizeof(message), "seq=%lu,timestamp=%lu", sequence_number, now);
            printf("\n[%lu ms] QoS 1 Publish: seq=%lu to topic 'pico/data'\n", now, sequence_number);
            printf("         Message: %s\n", message);
            
            bool publish_success = false;
            int max_retries = 3;
            uint32_t retry_delays[] = {2000, 4000, 8000};  // Exponential backoff: 2s, 4s, 8s
            
            for (int attempt = 0; attempt < max_retries; attempt++) {
                int ret = mqttsn_publish("pico/data", (uint8_t*)message, strlen(message), 1);
                
                if (ret == MQTTSN_OK) {
                    printf("         [SUCCESS] PUBACK received for seq=%lu", sequence_number);
                    if (attempt > 0) {
                        printf(" (retry attempt %d)", attempt);
                    }
                    printf(" - Gateway acknowledged message\n");
                    
                    publish_success = true;
                    consecutive_failures = 0;  // Reset on success
                    break;
                } else {
                    // Publish failed
                    if (attempt < max_retries - 1) {
                        // Not last attempt - retry with backoff
                        printf("         [TIMEOUT] No PUBACK received for seq=%lu (attempt %d/%d failed)\n", 
                               sequence_number, attempt + 1, max_retries);
                        printf("         Retrying in %lu ms (exponential backoff)...\n", retry_delays[attempt]);
                        
                        // Wait with polling to keep system responsive
                        uint32_t retry_start = to_ms_since_boot(get_absolute_time());
                        while (to_ms_since_boot(get_absolute_time()) - retry_start < retry_delays[attempt]) {
                            if (mqtt_connected) {
                                mqttsn_poll();
                            }
                            if (wifi_initialized) {
                                cyw43_arch_poll();
                            }
                            sleep_ms(10);
                        }
                    } else {
                        // Last attempt failed
                        printf("         [FAILED] No PUBACK received for seq=%lu after all %d attempts\n", 
                               sequence_number, max_retries);
                    }
                }
            }
            
            if (!publish_success) {
                // All retries failed - likely gateway is down
                consecutive_failures++;
                total_failed_publishes++;
                
                printf("         [FAILURE] Publish failed after %d retries\n", max_retries);
                printf("         Failure stats: consecutive=%lu, total=%lu\n", 
                       consecutive_failures, total_failed_publishes);
                
                // Check if gateway appears to be down (5 consecutive failures)
                if (consecutive_failures >= 5) {
                    printf("\n");
                    printf("═══════════════════════════════════════════════════════\n");
                    printf("  GATEWAY DOWN DETECTED - RECONNECTING\n");
                    printf("═══════════════════════════════════════════════════════\n");
                    printf("  Consecutive failures: %lu\n", consecutive_failures);
                    printf("  Gateway appears offline or unreachable.\n");
                    printf("  Initiating reconnection sequence...\n\n");
                    
                    // Disconnect and attempt reconnection
                    mqtt_connected = false;
                    consecutive_failures = 0;  // Reset for fresh start
                    
                    printf("  Step 1: Reinitializing MQTT-SN connection...\n");
                    sleep_ms(1000);
                    
                    const char *gateway_ip = "172.20.10.14";
                    uint16_t gateway_port = 1884;
                    
                    if (mqttsn_init(gateway_ip, gateway_port) != MQTTSN_OK) {
                        printf("  [FAILED] MQTT-SN init failed\n");
                    } else {
                        int ret = mqttsn_connect("PicoW_Client", 60);
                        if (ret != MQTTSN_OK) {
                            printf("  [FAILED] Reconnection failed (will retry later)\n");
                        } else {
                            printf("  [SUCCESS] Reconnected to gateway!\n");
                            
                            // Re-subscribe to topics
                            printf("  Step 2: Resubscribing to topics...\n");
                            mqttsn_set_message_callback(on_message_received);
                            mqttsn_subscribe("pico/test", 0);
                            mqttsn_subscribe("pico/command", 0);
                            mqttsn_subscribe("pico/chunks", 0);
                            mqttsn_subscribe("pico/block", 0);
                            printf("  [SUCCESS] Resubscribed to all topics\n");
                            
                            mqtt_connected = true;
                        }
                    }
                    
                    printf("═══════════════════════════════════════════════════════\n\n");
                }
            }
            
            sequence_number++;
            last_publish = now;
        }
        
        // NOTE: Keep-alive (PINGREQ/PINGRESP) is now handled automatically 
        // by mqttsn_poll() - no need for custom heartbeat messages
        
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
        
        // ═══════════════════════════════════════════════════════
        // Periodic WiFi Connection Check (every 5 seconds)
        // ═══════════════════════════════════════════════════════
        if (wifi_initialized && (now - last_wifi_check > WIFI_CHECK_INTERVAL_MS)) {
            last_wifi_check = now;
            
            // Check if WiFi is still connected
            if (!wifi_is_connected()) {
                printf("\n[%lu ms] ⚠️  WiFi DISCONNECTION DETECTED!\n", now);
                printf("Attempting to reconnect to WiFi...\n");
                
                // Mark as disconnected
                wifi_initialized = false;
                mqtt_connected = false;
                
                // Attempt reconnection with retries
                int reconnect_attempts = 0;
                #define MAX_WIFI_RECONNECT_ATTEMPTS 3
                
                const char *ssid = "jer";
                const char *password = "jeraldgoh";
                const char *gateway_ip = "172.20.10.14";
                uint16_t gateway_port = 1884;
                
                while (reconnect_attempts < MAX_WIFI_RECONNECT_ATTEMPTS) {
                    reconnect_attempts++;
                    printf("WiFi reconnection attempt %d/%d...\n", 
                           reconnect_attempts, MAX_WIFI_RECONNECT_ATTEMPTS);
                    
                    if (wifi_connect(ssid, password) == WIFI_OK) {
                        printf("[SUCCESS] WiFi reconnected!\n");
                        wifi_initialized = true;
                        
                        // Reinitialize UDP
                        if (wifi_udp_create(1884) == WIFI_OK) {
                            printf("[SUCCESS] UDP socket reinitialized on port 1884\n");
                            
                            // Try to connect UDP to remote gateway
                            if (wifi_udp_connect_remote(gateway_ip, gateway_port) == WIFI_OK) {
                                printf("[SUCCESS] UDP connected to gateway %s:%d\n", gateway_ip, gateway_port);
                            } else {
                                printf("[WARNING] UDP connect failed, will use sendto mode\n");
                            }
                            
                            // Reconnect to MQTT-SN
                            printf("Reconnecting to MQTT-SN gateway...\n");
                            if (mqttsn_connect("PicoW_Client", 60) == MQTTSN_OK) {
                                printf("[SUCCESS] MQTT-SN reconnected!\n");
                                mqtt_connected = true;
                                consecutive_failures = 0;  // Reset failure counter
                                
                                // Re-subscribe to topics
                                mqttsn_set_message_callback(on_message_received);
                                mqttsn_subscribe("pico/test", 0);
                                mqttsn_subscribe("pico/command", 0);
                                mqttsn_subscribe("pico/chunks", 0);
                                mqttsn_subscribe("pico/block", 0);
                            } else {
                                printf("[FAILED] MQTT-SN reconnection failed\n");
                            }
                        } else {
                            printf("[FAILED] UDP reinitialization failed\n");
                        }
                        break;
                    } else {
                        printf("[FAILED] WiFi reconnection attempt %d failed\n", reconnect_attempts);
                        if (reconnect_attempts < MAX_WIFI_RECONNECT_ATTEMPTS) {
                            printf("Waiting 5 seconds before retry...\n");
                            sleep_ms(5000);
                        }
                    }
                }
                
                if (!wifi_initialized) {
                    printf("[FAILED] WiFi reconnection failed after %d attempts\n", MAX_WIFI_RECONNECT_ATTEMPTS);
                    printf("Will retry in %d seconds...\n", WIFI_CHECK_INTERVAL_MS / 1000);
                }
                
                // Reset the check timer
                last_wifi_check = to_ms_since_boot(get_absolute_time());
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

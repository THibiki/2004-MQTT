#include "pico/stdlib.h"
#include "wifi_driver.h"
#include "mqtt_sn_client.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// SD Card pins configuration
#define SD_SPI_PORT spi1
#define SD_PIN_MISO 12
#define SD_PIN_CS   13
#define SD_PIN_SCK  14
#define SD_PIN_MOSI 15

// SD Card constants  
#define SD_SECTOR_SIZE 512
#define SD_MAX_FILENAME 128

// Simple file system simulation in memory (for demonstration)
#define MAX_FILES 10
#define MAX_FILE_SIZE 12288  // Increased to 12KB to accommodate larger test files

typedef struct {
    char filename[SD_MAX_FILENAME];
    uint8_t data[MAX_FILE_SIZE];
    size_t size;
    bool in_use;
} simulated_file_t;

static simulated_file_t simulated_files[MAX_FILES];
static bool sd_card_mounted = false;
static bool sd_card_was_mounted = false;
static uint32_t last_sd_check = 0;
static bool waiting_for_sd_message_shown = false;

// SD card detection interval (check every 2 seconds)
#define SD_CHECK_INTERVAL_MS 2000

// Block transfer constants
#define BLOCK_CHUNK_SIZE 128        // Size of each chunk (adjust based on MQTT-SN packet limits)
#define BLOCK_MAX_CHUNKS 100        // Maximum number of chunks per block
#define BLOCK_BUFFER_SIZE 10240     // 10KB buffer for large messages

// Block transfer header structure
typedef struct {
    uint16_t block_id;      // Unique block identifier
    uint16_t part_num;      // Current part number (1-based)
    uint16_t total_parts;   // Total number of parts
    uint16_t data_len;      // Length of data in this chunk
} block_header_t;

// Block reassembly structure
typedef struct {
    uint16_t block_id;
    uint16_t total_parts;
    uint16_t received_parts;
    bool *received_mask;    // Track which parts we've received
    uint8_t *data_buffer;   // Buffer to store reassembled data
    uint32_t total_length;  // Total length of complete message
    uint32_t last_update;   // Timestamp of last received part
} block_assembly_t;

// Global variables for block transfer
static block_assembly_t current_block = {0};
static uint16_t next_block_id = 1;

// Generate sample 10KB text data
void generate_large_message(char *buffer, size_t size) {
    snprintf(buffer, size, "=== LARGE MESSAGE BLOCK TRANSFER TEST ===\n");
    size_t offset = strlen(buffer);
    
    for (int i = 0; i < 200 && offset < size - 100; i++) {
        int written = snprintf(buffer + offset, size - offset,
            "Line %03d: This is a test line with some data to make the message larger. "
            "Block transfer allows us to send messages bigger than MQTT-SN packet limits. "
            "Each chunk contains sequence information for proper reassembly.\n", i + 1);
        if (written > 0) {
            offset += written;
        }
    }
    
    // Add footer
    snprintf(buffer + offset, size - offset, "\n=== END OF LARGE MESSAGE ===\n");
}

// Send a large message using block transfer
int send_block_transfer(const char *topic, const uint8_t *data, size_t data_len) {
    if (data_len > BLOCK_BUFFER_SIZE) {
        printf("Error: Message too large (%zu bytes, max %d)\n", data_len, BLOCK_BUFFER_SIZE);
        return -1;
    }
    
    // Calculate number of chunks needed
    size_t chunk_data_size = BLOCK_CHUNK_SIZE - sizeof(block_header_t);
    uint16_t total_parts = (data_len + chunk_data_size - 1) / chunk_data_size;
    
    if (total_parts > BLOCK_MAX_CHUNKS) {
        printf("Error: Too many chunks needed (%d, max %d)\n", total_parts, BLOCK_MAX_CHUNKS);
        return -1;
    }
    
    uint16_t block_id = next_block_id++;
    printf("\n=== Starting block transfer ===\n");
    printf("Block ID: %d, Data size: %zu bytes, Chunks: %d\n", block_id, data_len, total_parts);
    
    // Send each chunk
    for (uint16_t part = 1; part <= total_parts; part++) {
        size_t offset = (part - 1) * chunk_data_size;
        size_t chunk_len = (offset + chunk_data_size > data_len) ? 
                          (data_len - offset) : chunk_data_size;
        
        // Create packet with header + data
        uint8_t packet[BLOCK_CHUNK_SIZE];
        block_header_t *header = (block_header_t*)packet;
        
        header->block_id = block_id;
        header->part_num = part;
        header->total_parts = total_parts;
        header->data_len = chunk_len;
        
        // Copy chunk data after header
        memcpy(packet + sizeof(block_header_t), data + offset, chunk_len);
        
        size_t packet_size = sizeof(block_header_t) + chunk_len;
        
        printf("Sending chunk %d/%d (%zu bytes)\n", part, total_parts, packet_size);
        
        // Send chunk
        int ret = mqttsn_publish(topic, packet, packet_size, 0);
        if (ret != MQTTSN_OK) {
            printf("Failed to send chunk %d/%d\n", part, total_parts);
            return -1;
        }
        
        // Small delay between chunks to avoid overwhelming the receiver
        sleep_ms(100);
    }
    
    printf("Block transfer completed: %d chunks sent\n", total_parts);
    return 0;
}

// Initialize block reassembly
int init_block_assembly(uint16_t block_id, uint16_t total_parts) {
    // Clean up previous block if exists
    if (current_block.received_mask) {
        free(current_block.received_mask);
    }
    if (current_block.data_buffer) {
        free(current_block.data_buffer);
    }
    
    // Initialize new block
    current_block.block_id = block_id;
    current_block.total_parts = total_parts;
    current_block.received_parts = 0;
    current_block.total_length = 0;
    current_block.last_update = to_ms_since_boot(get_absolute_time());
    
    // Allocate tracking arrays
    current_block.received_mask = (bool*)calloc(total_parts, sizeof(bool));
    current_block.data_buffer = (uint8_t*)malloc(BLOCK_BUFFER_SIZE);
    
    if (!current_block.received_mask || !current_block.data_buffer) {
        printf("Error: Failed to allocate memory for block assembly\n");
        return -1;
    }
    
    printf("Initialized block assembly: ID=%d, parts=%d\n", block_id, total_parts);
    return 0;
}

// Process received block chunk
void process_block_chunk(const uint8_t *data, size_t len) {
    if (len < sizeof(block_header_t)) {
        printf("Error: Packet too small for block header\n");
        return;
    }
    
    block_header_t *header = (block_header_t*)data;
    const uint8_t *chunk_data = data + sizeof(block_header_t);
    size_t chunk_data_len = header->data_len;
    
    printf("Received chunk: Block=%d, Part=%d/%d, Data=%d bytes\n", 
           header->block_id, header->part_num, header->total_parts, header->data_len);
    
    // Initialize block assembly if this is a new block
    if (current_block.block_id != header->block_id) {
        if (init_block_assembly(header->block_id, header->total_parts) != 0) {
            return;
        }
    }
    
    // Validate part number
    if (header->part_num < 1 || header->part_num > header->total_parts) {
        printf("Error: Invalid part number %d (total %d)\n", header->part_num, header->total_parts);
        return;
    }
    
    // Check if we already received this part
    uint16_t part_index = header->part_num - 1;
    if (current_block.received_mask[part_index]) {
        printf("Warning: Duplicate chunk %d ignored\n", header->part_num);
        return;
    }
    
    // Calculate offset in reassembly buffer
    size_t chunk_data_size = BLOCK_CHUNK_SIZE - sizeof(block_header_t);
    size_t buffer_offset = part_index * chunk_data_size;
    
    // Store chunk data
    if (buffer_offset + chunk_data_len <= BLOCK_BUFFER_SIZE) {
        memcpy(current_block.data_buffer + buffer_offset, chunk_data, chunk_data_len);
        current_block.received_mask[part_index] = true;
        current_block.received_parts++;
        current_block.last_update = to_ms_since_boot(get_absolute_time());
        
        // Update total length (for the last chunk, it might be partial)
        if (header->part_num == header->total_parts) {
            current_block.total_length = buffer_offset + chunk_data_len;
        }
        
        printf("Stored chunk %d/%d (progress: %d/%d)\n", 
               header->part_num, header->total_parts, 
               current_block.received_parts, current_block.total_parts);
        
        // Check if block is complete
        if (current_block.received_parts == current_block.total_parts) {
            printf("\n=== BLOCK TRANSFER COMPLETE ===\n");
            printf("Block ID: %d\n", current_block.block_id);
            printf("Total size: %d bytes\n", current_block.total_length);
            printf("Reassembled message:\n");
            printf("--- BEGIN MESSAGE ---\n");
            printf("%.*s", current_block.total_length, current_block.data_buffer);
            printf("\n--- END MESSAGE ---\n");
            
            // Save received block to SD card
            if (sd_card_mounted) {
                char received_filename[64];
                snprintf(received_filename, sizeof(received_filename), 
                        "received/block_%d_%lu.txt", 
                        current_block.block_id, 
                        to_ms_since_boot(get_absolute_time()) / 1000);
                
                // Create received directory if it doesn't exist (simulated - just a note)
                printf("Saving to simulated 'received' directory\n");
                
                if (sd_card_save_block(received_filename, current_block.data_buffer, current_block.total_length) == 0) {
                    printf("Block saved to SD card: %s\n", received_filename);
                } else {
                    printf("Failed to save block to SD card\n");
                }
            }
            
            // Publish completion notification
            char complete_msg[100];
            snprintf(complete_msg, sizeof(complete_msg), 
                    "BLOCK_COMPLETE: ID=%d, SIZE=%d, PARTS=%d", 
                    current_block.block_id, current_block.total_length, current_block.total_parts);
            
            mqttsn_publish("pico/block", (uint8_t*)complete_msg, strlen(complete_msg), 0);
            printf("Published completion notification to 'pico/block'\n");
            
            // Reset for next block
            current_block.block_id = 0;
        }
    } else {
        printf("Error: Chunk data would overflow buffer\n");
    }
}

// SD Card file system object (simulated)
static int file_count = 0;

// Find a file by name
static simulated_file_t* find_file(const char *filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (simulated_files[i].in_use && strcmp(simulated_files[i].filename, filename) == 0) {
            return &simulated_files[i];
        }
    }
    return NULL;
}

// Find a free file slot
static simulated_file_t* find_free_slot(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!simulated_files[i].in_use) {
            return &simulated_files[i];
        }
    }
    return NULL;
}

// Initialize SD card (simulated)
int sd_card_init(void) {
    printf("Initializing SD card (Maker Pi Pico built-in slot)...\n");
    
    // Note: Maker Pi Pico has built-in SD card slot
    // No need to manually configure SPI pins - they're handled by the board
    
    // For real SD card implementation on Maker Pi Pico, you would:
    // 1. Use the SD card library (like FatFS)
    // 2. The hardware connections are already done on the board
    // 3. Just mount the filesystem
    
    // For now, we'll use the simulated approach for testing
    printf("Using simulated SD card (built-in slot ready for real implementation)\n");
    
    // Initialize simulated file system
    memset(simulated_files, 0, sizeof(simulated_files));
    file_count = 0;
    
    sd_card_mounted = true;
    printf("SD card (simulated) mounted successfully\n");
    
    // List existing files
    printf("Current files in simulated SD card:\n");
    if (file_count == 0) {
        printf("  (no files)\n");
    } else {
        for (int i = 0; i < MAX_FILES; i++) {
            if (simulated_files[i].in_use) {
                printf("  %s (%zu bytes)\n", simulated_files[i].filename, simulated_files[i].size);
            }
        }
    }
    
    return 0;
}

// Write data to SD card file (simulated)
int sd_card_write_file(const char *filename, const uint8_t *data, size_t size) {
    if (!sd_card_mounted) {
        printf("SD card not mounted\n");
        return -1;
    }
    
    if (size > MAX_FILE_SIZE) {
        printf("File too large (%zu bytes, max %d)\n", size, MAX_FILE_SIZE);
        return -1;
    }
    
    printf("Writing %zu bytes to SD card file: %s\n", size, filename);
    
    // Find existing file or create new one
    simulated_file_t *file = find_file(filename);
    if (!file) {
        file = find_free_slot();
        if (!file) {
            printf("No free file slots (max %d files)\n", MAX_FILES);
            return -1;
        }
        strncpy(file->filename, filename, SD_MAX_FILENAME - 1);
        file->filename[SD_MAX_FILENAME - 1] = '\0';
        file->in_use = true;
        file_count++;
    }
    
    // Copy data
    memcpy(file->data, data, size);
    file->size = size;
    
    printf("Successfully wrote %zu bytes to %s (slot %d)\n", size, filename, 
           (int)(file - simulated_files));
    return 0;
}

// Read data from SD card file (simulated)
int sd_card_read_file(const char *filename, uint8_t *buffer, size_t max_size, size_t *actual_size) {
    if (!sd_card_mounted) {
        printf("SD card not mounted\n");
        return -1;
    }
    
    simulated_file_t *file = find_file(filename);
    if (!file) {
        printf("File not found: %s\n", filename);
        return -1;
    }
    
    printf("Reading file: %s (%zu bytes)\n", filename, file->size);
    
    if (file->size > max_size) {
        printf("File too large (%zu bytes, max %zu)\n", file->size, max_size);
        return -1;
    }
    
    memcpy(buffer, file->data, file->size);
    *actual_size = file->size;
    
    printf("Successfully read %zu bytes from %s\n", file->size, filename);
    return 0;
}

// Send SD card file via block transfer
int sd_card_send_file(const char *filename, const char *topic) {
    if (!sd_card_mounted) {
        printf("SD card not mounted\n");
        return -1;
    }
    
    // Allocate buffer for file data
    uint8_t *file_buffer = (uint8_t*)malloc(BLOCK_BUFFER_SIZE);
    if (!file_buffer) {
        printf("Failed to allocate file buffer\n");
        return -1;
    }
    
    size_t file_size;
    int ret = sd_card_read_file(filename, file_buffer, BLOCK_BUFFER_SIZE, &file_size);
    if (ret != 0) {
        free(file_buffer);
        return ret;
    }
    
    printf("\n=== Sending SD card file via block transfer ===\n");
    printf("File: %s, Size: %zu bytes\n", filename, file_size);
    
    // Send file using block transfer
    ret = send_block_transfer(topic, file_buffer, file_size);
    
    free(file_buffer);
    return ret;
}

// Save received block transfer to SD card
int sd_card_save_block(const char *filename, const uint8_t *data, size_t size) {
    printf("\n=== Saving block transfer to SD card ===\n");
    printf("File: %s, Size: %zu bytes\n", filename, size);
    
    return sd_card_write_file(filename, data, size);
}

// Create a test file on SD card
int sd_card_create_test_file(const char *filename) {
    if (!sd_card_mounted) {
        printf("SD card not mounted\n");
        return -1;
    }
    
    printf("Creating test file: %s\n", filename);
    
    // Generate test content
    char *test_content = (char*)malloc(BLOCK_BUFFER_SIZE);
    if (!test_content) {
        printf("Failed to allocate test content buffer\n");
        return -1;
    }
    
    // Create meaningful test content
    snprintf(test_content, BLOCK_BUFFER_SIZE, 
        "=== SD CARD TEST FILE ===\n"
        "Created: %lu ms since boot\n"
        "Device: Raspberry Pi Pico W\n"
        "Feature: SD Card + Block Transfer Integration\n\n"
        "This file demonstrates the ability to:\n"
        "✓ Write files to SD card\n"
        "✓ Read files from SD card\n" 
        "✓ Send files via MQTT-SN block transfer\n"
        "✓ Receive and save block transfers to SD card\n\n",
        to_ms_since_boot(get_absolute_time()));
    
    size_t base_len = strlen(test_content);
    
    // Add more content to make it substantial
    for (int i = 0; i < 50 && base_len < BLOCK_BUFFER_SIZE - 200; i++) {
        int written = snprintf(test_content + base_len, BLOCK_BUFFER_SIZE - base_len,
            "Test line %02d: SD card provides persistent storage for sensor data, "
            "configuration files, logs, and any data that needs to survive power cycles. "
            "Combined with block transfer, this enables reliable data archival.\n", i + 1);
        if (written > 0) {
            base_len += written;
        }
    }
    
    // Add footer
    snprintf(test_content + base_len, BLOCK_BUFFER_SIZE - base_len, 
        "\n=== END TEST FILE (Total: %zu bytes) ===\n", strlen(test_content));
    
    int ret = sd_card_write_file(filename, (uint8_t*)test_content, strlen(test_content));
    free(test_content);
    return ret;
}

// Check if SD card is physically present (simulated detection for Maker Pi Pico)
bool sd_card_is_present(void) {
    // For Maker Pi Pico with built-in SD card slot:
    // In a real implementation, this would check if SD card is inserted
    // using the board's SD card detection mechanism
    
    // For simulation, we'll create a time-based test
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Simulate card removal for 10 seconds every 2 minutes for demo
    static uint32_t sim_remove_time = 0;
    if (sim_remove_time == 0) {
        sim_remove_time = now + 120000; // Remove after 2 minutes
    }
    
    if (now > sim_remove_time && now < sim_remove_time + 10000) {
        // Simulate card removed for 10 seconds
        return false;
    } else if (now > sim_remove_time + 10000) {
        // Reset timer for next removal
        sim_remove_time = now + 120000;
    }
    
    return true; // Card present most of the time
}

// Initialize SD card with presence detection
int sd_card_init_with_detection(void) {
    if (!sd_card_is_present()) {
        printf("No SD card detected\n");
        sd_card_mounted = false;
        return -1;
    }
    
    return sd_card_init();
}

// Check SD card status and handle removal/insertion
void sd_card_check_status(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Only check every SD_CHECK_INTERVAL_MS
    if (now - last_sd_check < SD_CHECK_INTERVAL_MS) {
        return;
    }
    last_sd_check = now;
    
    bool card_present = sd_card_is_present();
    
    // Handle card removal
    if (sd_card_mounted && !card_present) {
        printf("\n*** SD CARD REMOVED ***\n");
        sd_card_mounted = false;
        sd_card_was_mounted = true;
        waiting_for_sd_message_shown = false;
        
        // Clear simulated file system
        memset(simulated_files, 0, sizeof(simulated_files));
        file_count = 0;
        
        printf("SD card unmounted - files cleared\n");
    }
    
    // Handle waiting for card
    if (!sd_card_mounted && sd_card_was_mounted) {
        if (!waiting_for_sd_message_shown) {
            printf("\n🔄 Waiting for SD card...\n");
            waiting_for_sd_message_shown = true;
        }
        
        // Show periodic waiting message
        static uint32_t last_waiting_msg = 0;
        if (now - last_waiting_msg > 5000) { // Every 5 seconds
            printf("⏳ Still waiting for SD card insertion...\n");
            last_waiting_msg = now;
        }
    }
    
    // Handle card insertion
    if (!sd_card_mounted && card_present) {
        if (sd_card_was_mounted || !waiting_for_sd_message_shown) {
            printf("\n*** SD CARD DETECTED ***\n");
        }
        
        if (sd_card_init() == 0) {
            printf("✅ SD card reinitialized successfully!\n");
            
            // Recreate test file after reinsertion
            if (sd_card_create_test_file("test_data.txt") == 0) {
                printf("Test file recreated\n");
            }
            
            waiting_for_sd_message_shown = false;
            sd_card_was_mounted = false;
        } else {
            printf("❌ Failed to reinitialize SD card\n");
        }
    }
}

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

    // Initialize SD card with presence detection
    printf("Setting up SD card with detection...\n");
    if (sd_card_init_with_detection() == 0) {
        printf("SD card ready!\n");
        
        // Create a test file
        if (sd_card_create_test_file("test_data.txt") == 0) {
            printf("Test file created successfully\n");
        }
        
        // Create sensor data directory (simulated - just a note)
        printf("Using simulated 'sensor_data' directory\n");
    } else {
        printf("No SD card detected - will monitor for insertion\n");
        sd_card_was_mounted = false; // Never was mounted, so don't show waiting message initially
    }

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
    
    // Subscribe to test topics
    printf("Subscribing to topics...\n");
    mqttsn_subscribe("pico/test", 0);      // Regular test messages
    mqttsn_subscribe("pico/chunks", 0);    // Block transfer chunks
    mqttsn_subscribe("pico/block", 0);     // Block completion notifications
    
    // Main loop
    uint32_t last_publish = 0;
    uint32_t last_block_transfer = 0;
    uint32_t last_sd_operation = 0;
    int counter = 0;
    
    printf("\n=== Starting main loop ===\n");
    printf("Publishing to 'pico/data' every 5 seconds\n");
    printf("SD card operations every 45 seconds\n");
    printf("Sending block transfer every 30 seconds\n");
    printf("Listening for messages and block chunks\n\n");
    
    while (true) {
        // Poll for incoming MQTT-SN messages
        mqttsn_poll();
        
        // Check SD card status (insertion/removal)
        sd_card_check_status();
        
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Regular publish every 5 seconds
        if (now - last_publish > 5000) {
            char message[64];
            snprintf(message, sizeof(message), "Hello from Pico W! Count: %d", counter++);
            
            printf("Publishing: %s\n", message);
            mqttsn_publish("pico/data", (uint8_t*)message, strlen(message), 0);
            
            last_publish = now;
        }
        
        // Block transfer every 30 seconds (alternates between generated and SD card content)
        if (now - last_block_transfer > 30000) {
            printf("\n=== Preparing block transfer ===\n");
            
            // Alternate between sending generated content and SD card files
            static bool send_sd_file = false;
            
            if (send_sd_file && sd_card_mounted) {
                // Send SD card file via block transfer
                printf("Sending SD card file via block transfer\n");
                if (sd_card_send_file("test_data.txt", "pico/chunks") == 0) {
                    printf("SD card file transfer initiated successfully\n");
                } else {
                    printf("SD card file transfer failed\n");
                }
            } else {
                // Generate and send large message
                char *large_buffer = (char*)malloc(BLOCK_BUFFER_SIZE);
                if (large_buffer) {
                    generate_large_message(large_buffer, BLOCK_BUFFER_SIZE);
                    size_t msg_len = strlen(large_buffer);
                    
                    printf("Generated message: %zu bytes\n", msg_len);
                    
                    // Send as block transfer
                    if (send_block_transfer("pico/chunks", (uint8_t*)large_buffer, msg_len) == 0) {
                        printf("Block transfer initiated successfully\n");
                    } else {
                        printf("Block transfer failed\n");
                    }
                    
                    free(large_buffer);
                } else {
                    printf("Failed to allocate buffer for large message\n");
                }
            }
            
            send_sd_file = !send_sd_file; // Alternate for next time
            last_block_transfer = now;
        }
        
        // SD card operations every 45 seconds  
        if (sd_card_mounted && (now - last_sd_operation > 45000)) {
            printf("\n=== SD Card Operations ===\n");
            
            // Save sensor data to SD card
            char sensor_filename[64];
            snprintf(sensor_filename, sizeof(sensor_filename), 
                    "sensor_data/sensor_%lu.txt", now / 1000);
            
            char sensor_data[512];
            snprintf(sensor_data, sizeof(sensor_data),
                    "Timestamp: %lu ms\n"
                    "Counter: %d\n"
                    "WiFi Status: Connected\n"
                    "MQTT-SN Status: Connected\n"
                    "Memory Free: %d KB\n"
                    "Temperature: 23.5°C\n"
                    "Humidity: 65%%\n"
                    "Light Level: 340 lux\n"
                    "Battery: 3.7V\n",
                    now, counter, 
                    (BLOCK_BUFFER_SIZE - strlen(sensor_data)) / 1024);
            
            if (sd_card_write_file(sensor_filename, (uint8_t*)sensor_data, strlen(sensor_data)) == 0) {
                printf("Sensor data logged to SD card: %s\n", sensor_filename);
            }
            
            last_sd_operation = now;
        }
        
        // Check for block assembly timeout (30 seconds)
        if (current_block.block_id != 0 && 
            (now - current_block.last_update) > 30000) {
            printf("Block assembly timeout for block %d (received %d/%d parts)\n",
                   current_block.block_id, current_block.received_parts, current_block.total_parts);
            current_block.block_id = 0; // Reset
        }
        
        // Poll WiFi stack
        cyw43_arch_poll();
        sleep_ms(10);
    }
    
    return 0;
}

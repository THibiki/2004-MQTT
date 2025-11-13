#include "block_transfer.h"
#include "mqttsn_client.h"
#include "sd_card.h"
#include "ff.h"  // FatFs library for directory operations
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


// Global variables for block transfer
static block_assembly_t current_block = {0};
static uint16_t next_block_id = 1;

// map current calls of mqttsn_publish() to new mqttsn publish call method (mqttsn_demo_publish_name)
// This function respects the QoS parameter by temporarily setting current_qos
static int mqttsn_publish(const char *topic, const uint8_t *data, size_t len, uint8_t qos){
    // Save current QoS and set to requested QoS
    int saved_qos = mqttsn_get_qos();
    mqttsn_set_qos(qos);
    
    // Publish with the requested QoS
    int result = (mqttsn_demo_publish_name(topic, data, (int)len) == 0) ? MQTTSN_OK : MQTTSN_ERROR;
    
    // Restore original QoS
    mqttsn_set_qos(saved_qos);
    
    return result;
}

int block_transfer_init(void) {
    memset(&current_block, 0, sizeof(current_block));
    next_block_id = 1;
    printf("Block transfer system initialized\n");
    return 0;
}

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
        
        // Send with QoS 1 - will wait for PUBACK, retry if timeout
        int max_retries = 3;
        int ret = MQTTSN_ERROR;
        
        for (int attempt = 1; attempt <= max_retries; attempt++) {
            ret = mqttsn_publish(topic, packet, packet_size, 1); // QoS 1
            
            if (ret == MQTTSN_OK) {
                break; // Success - PUBACK received
            } else if (attempt < max_retries) {
                printf("  Retry %d/%d for chunk %d (no PUBACK)\n", attempt, max_retries, part);
                sleep_ms(100); // Small delay before retry
            }
        }
        
        if (ret != MQTTSN_OK) {
            printf("Failed to send chunk %d/%d after %d attempts\n", part, total_parts, max_retries);
            return -1;
        }
        
        // Print progress every 10 chunks
        if (part % 10 == 0 || part == total_parts) {
            printf("  Progress: %d/%d chunks sent (%.1f%%)\n", 
                   part, total_parts, (float)part * 100.0 / total_parts);
        }
        
        // Small delay between chunks
        sleep_ms(20);
    }
    
    printf("Block transfer completed: %d chunks sent\n", total_parts);
    return 0;
}

// Send a large message using block transfer with configurable QoS
int send_block_transfer_qos(const char *topic, const uint8_t *data, size_t data_len, uint8_t qos) {
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
    printf("\n=== Starting block transfer (QoS %d) ===\n", qos);
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
        
        int ret;
        if (qos == 1) {
            // QoS 1 - will wait for PUBACK, retry if timeout
            int max_retries = 3;
            ret = MQTTSN_ERROR;
            
            for (int attempt = 1; attempt <= max_retries; attempt++) {
                ret = mqttsn_publish(topic, packet, packet_size, 1);
                
                if (ret == MQTTSN_OK) {
                    break; // Success - PUBACK received
                } else if (attempt < max_retries) {
                    printf("  Retry %d/%d for chunk %d (no PUBACK)\n", attempt, max_retries, part);
                    sleep_ms(100); // Small delay before retry
                }
            }
            
            if (ret != MQTTSN_OK) {
                printf("Failed to send chunk %d/%d after %d attempts\n", part, total_parts, max_retries);
                return -1;
            }
        } else if (qos == 2) {
            // QoS 2 - will wait for PUBREC/PUBREL/PUBCOMP handshake, retry if timeout
            int max_retries = 3;
            ret = MQTTSN_ERROR;
            
            for (int attempt = 1; attempt <= max_retries; attempt++) {
                ret = mqttsn_publish(topic, packet, packet_size, 2);
                
                if (ret == MQTTSN_OK) {
                    break; // Success - PUBREC/PUBREL/PUBCOMP completed
                } else if (attempt < max_retries) {
                    printf("  Retry %d/%d for chunk %d (QoS 2 handshake failed)\n", attempt, max_retries, part);
                    sleep_ms(100); // Small delay before retry
                }
            }
            
            if (ret != MQTTSN_OK) {
                printf("Failed to send chunk %d/%d after %d attempts (QoS 2)\n", part, total_parts, max_retries);
                return -1;
            }
        } else if (qos == 0) {
            // QoS 0 - fire and forget (no acknowledgment, may lose packets)
            ret = mqttsn_publish(topic, packet, packet_size, 0);
            if (ret != MQTTSN_OK) {
                printf("Failed to send chunk %d/%d (QoS 0)\n", part, total_parts);
                return -1;
            }
            // Note: QoS 0 returns success immediately after UDP send
            // This does NOT guarantee the packet was received by the gateway
        } else {
            // Invalid QoS level
            printf("Error: Invalid QoS level %d (must be 0, 1, or 2)\n", qos);
            return -1;
        }
        
        // Print progress every 10 chunks
        if (part % 10 == 0 || part == total_parts) {
            printf("  Progress: %d/%d chunks sent (%.1f%%)\n", 
                   part, total_parts, (float)part * 100.0 / total_parts);
        }
        
        // Small delay between chunks
        sleep_ms(20);
    }
    
    printf("Block transfer completed: %d chunks sent\n", total_parts);
    return 0;
}

// Send an image file from SD card using block transfer
int send_image_file(const char *topic, const char *filename) {
    return send_image_file_qos(topic, filename, mqttsn_get_qos());
}

// Send an image file from SD card using block transfer with configurable QoS
// This sends images from SD card to GitHub repo via MQTT-SN
int send_image_file_qos(const char *topic, const char *filename, uint8_t qos) {
    printf("\n=== Sending image from SD card to GitHub repo (QoS %d) ===\n", qos);
    printf("📁 Reading from SD card: %s\n", filename);
    
    // Check if SD card is mounted
    if (!sd_card_is_mounted()) {
        printf("❌ Error: SD card not mounted\n");
        return -1;
    }
    
    // First, get the file size to allocate the right buffer size
    FIL file;
    FRESULT res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        printf("❌ Error: Failed to open file '%s' (error %d)\n", filename, res);
        return -1;
    }
    
    // Get file size
    FSIZE_t file_size_fs = f_size(&file);
    f_close(&file);
    
    // Convert FSIZE_t to size_t (handle both 32-bit and 64-bit)
    size_t file_size = (size_t)file_size_fs;
    
    if (file_size == 0) {
        printf("❌ Error: File '%s' is empty\n", filename);
        return -1;
    }
    
    printf("📊 File size: %zu bytes (%.2f MB)\n", file_size, file_size / (1024.0 * 1024.0));
    
    // Reject files that are too large for Pico W's limited RAM
    if (file_size > MAX_SUPPORTED_FILE_SIZE) {
        printf("❌ Error: File too large!\n");
        printf("   File size: %zu bytes (%.2f MB)\n", file_size, file_size / (1024.0 * 1024.0));
        printf("   Maximum supported: %d bytes (%.2f MB)\n", 
               MAX_SUPPORTED_FILE_SIZE, MAX_SUPPORTED_FILE_SIZE / (1024.0 * 1024.0));
        printf("   Pico W has limited RAM (~264KB total)\n");
        printf("   Please use a smaller image file (under %.2f MB)\n", 
               MAX_SUPPORTED_FILE_SIZE / (1024.0 * 1024.0));
        return -1;
    }
    
    if (file_size > BLOCK_BUFFER_SIZE) {
        printf("⚠️  Warning: File size (%zu bytes, %.2f MB) exceeds buffer size (%d bytes, %.2f MB)\n", 
               file_size, file_size / (1024.0 * 1024.0), 
               BLOCK_BUFFER_SIZE, BLOCK_BUFFER_SIZE / (1024.0 * 1024.0));
        printf("   File will be truncated to %d bytes\n", BLOCK_BUFFER_SIZE);
    }
    
    // Allocate buffer (use file size or buffer size, whichever is smaller)
    size_t buffer_size = (file_size > BLOCK_BUFFER_SIZE) ? BLOCK_BUFFER_SIZE : file_size;
    
    printf("💾 Allocating buffer: %zu bytes (%.2f MB)...\n", buffer_size, buffer_size / (1024.0 * 1024.0));
    
    uint8_t *image_buffer = malloc(buffer_size);
    if (!image_buffer) {
        printf("❌ Error: Failed to allocate image buffer (%zu bytes, %.2f MB)\n", 
               buffer_size, buffer_size / (1024.0 * 1024.0));
        printf("   Out of memory! Pico W has limited RAM (~264KB total)\n");
        printf("   Try using a smaller image file\n");
        return -1;
    }
    
    printf("✅ Buffer allocated successfully\n");
    
    size_t image_size = 0;
    int ret = sd_card_read_file(filename, image_buffer, buffer_size, &image_size);
    
    if (ret != 0) {
        printf("❌ Error: Failed to read image file '%s' from SD card\n", filename);
        free(image_buffer);
        return -1;
    }
    
    printf("✅ Image loaded from SD card: %zu bytes (%.2f MB)\n", 
           image_size, image_size / (1024.0 * 1024.0));
    printf("📤 Sending to topic '%s' (will be saved to repo/received/)\n", topic);
    
    // Send via block transfer with specified QoS
    ret = send_block_transfer_qos(topic, image_buffer, image_size, qos);
    
    free(image_buffer);
    
    if (ret == 0) {
        printf("✅ Image transfer completed - saved to GitHub repo\n");
    } else {
        printf("❌ Image transfer failed\n");
    }
    
    return ret;
}

// Initialize block reassembly
static int init_block_assembly(uint16_t block_id, uint16_t total_parts) {
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
            
            // Detect file type from data signature
            const char *file_ext = ".bin";  // Default extension
            if (current_block.total_length >= 2) {
                uint8_t byte0 = current_block.data_buffer[0];
                uint8_t byte1 = current_block.data_buffer[1];
                
                // JPEG: FF D8
                if (byte0 == 0xFF && byte1 == 0xD8) {
                    file_ext = ".jpg";
                }
                // PNG: 89 50 4E 47
                else if (byte0 == 0x89 && byte1 == 0x50 && 
                         current_block.total_length >= 4 &&
                         current_block.data_buffer[2] == 0x4E && 
                         current_block.data_buffer[3] == 0x47) {
                    file_ext = ".png";
                }
                // GIF: 47 49 46 38
                else if (byte0 == 0x47 && byte1 == 0x49 &&
                         current_block.total_length >= 4 &&
                         current_block.data_buffer[2] == 0x46 &&
                         current_block.data_buffer[3] == 0x38) {
                    file_ext = ".gif";
                }
            }
            
            // Save received block to SD card
            if (sd_card_is_mounted()) {
                // Create received directory if it doesn't exist
                DIR dir;
                FRESULT dir_res = f_opendir(&dir, "received");
                if (dir_res == FR_NO_PATH || dir_res == FR_NO_FILE) {
                    // Directory doesn't exist, create it
                    dir_res = f_mkdir("received");
                    if (dir_res == FR_OK) {
                        printf("📁 Created 'received' directory\n");
                    } else if (dir_res == FR_EXIST) {
                        printf("📁 Directory 'received' already exists\n");
                    } else {
                        printf("⚠️  Failed to create 'received' directory (error %d)\n", dir_res);
                    }
                } else if (dir_res == FR_OK) {
                    // Directory exists, close the handle
                    f_closedir(&dir);
                    printf("📁 Using existing 'received' directory\n");
                }
                
                // Generate filename with timestamp
                char received_filename[64];
                uint32_t timestamp_sec = to_ms_since_boot(get_absolute_time()) / 1000;
                snprintf(received_filename, sizeof(received_filename), 
                        "received/block_%d_%lu%s", 
                        current_block.block_id, 
                        timestamp_sec,
                        file_ext);
                
                printf("💾 Saving received block to SD card: %s\n", received_filename);
                
                if (sd_card_save_block(received_filename, current_block.data_buffer, current_block.total_length) == 0) {
                    printf("✅ Block saved to SD card: %s (%d bytes)\n", received_filename, current_block.total_length);
                } else {
                    printf("❌ Failed to save block to SD card\n");
                }
            } else {
                printf("⚠️  SD card not mounted, skipping save\n");
            }
            
            // Publish completion notification
            char complete_msg[150];
            uint32_t timestamp_sec = to_ms_since_boot(get_absolute_time()) / 1000;
            snprintf(complete_msg, sizeof(complete_msg), 
                    "BLOCK_RECEIVED: ID=%d, SIZE=%d, PARTS=%d, TYPE=%s, TIME=%lu", 
                    current_block.block_id, 
                    current_block.total_length, 
                    current_block.total_parts,
                    file_ext,
                    timestamp_sec);
            
            mqttsn_publish("pico/block", (uint8_t*)complete_msg, strlen(complete_msg), 0);
            printf("📬 Published metadata to 'pico/block'\n");
            
            // Reset for next block
            current_block.block_id = 0;
        }
    } else {
        printf("Error: Chunk data would overflow buffer\n");
    }
}

bool block_transfer_is_active(void) {
    return current_block.block_id != 0;
}

void block_transfer_check_timeout(void) {
    // Check for block assembly timeout (30 seconds)
    if (current_block.block_id != 0) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if ((now - current_block.last_update) > 30000) {
            printf("Block assembly timeout for block %d (received %d/%d parts)\n",
                   current_block.block_id, current_block.received_parts, current_block.total_parts);
            current_block.block_id = 0; // Reset
        }
    }
}
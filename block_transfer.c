#include "block_transfer.h"
#include "mqtt_sn_client.h"
#include "sd_card.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Global variables for block transfer
static block_assembly_t current_block = {0};
static uint16_t next_block_id = 1;

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
        } else {
            // QoS 0 - fire and forget
            ret = mqttsn_publish(topic, packet, packet_size, 0);
            if (ret != MQTTSN_OK) {
                printf("Failed to send chunk %d/%d\n", part, total_parts);
                return -1;
            }
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
    return send_image_file_qos(topic, filename, 1);  // Default QoS 1
}

// Send an image file from SD card using block transfer with configurable QoS
int send_image_file_qos(const char *topic, const char *filename, uint8_t qos) {
    printf("\n=== Starting image file transfer (QoS %d) ===\n", qos);
    printf("File: %s\n", filename);
    
    // Read image file from SD card
    uint8_t *image_buffer = malloc(BLOCK_BUFFER_SIZE);
    if (!image_buffer) {
        printf("Error: Failed to allocate image buffer\n");
        return -1;
    }
    
    size_t image_size = 0;
    int ret = sd_card_read_file(filename, image_buffer, BLOCK_BUFFER_SIZE, &image_size);
    
    if (ret != 0) {
        printf("Error: Failed to read image file '%s'\n", filename);
        free(image_buffer);
        return -1;
    }
    
    printf("Image loaded: %zu bytes\n", image_size);
    
    // Send via block transfer with specified QoS
    ret = send_block_transfer_qos(topic, image_buffer, image_size, qos);
    
    free(image_buffer);
    
    if (ret == 0) {
        printf("Image transfer completed successfully\n");
    } else {
        printf("Image transfer failed\n");
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
            
            // Save received block to SD card
            if (sd_card_is_mounted()) {
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
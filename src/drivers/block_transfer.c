#include "drivers/block_transfer.h"
#include "protocol/mqttsn/mqttsn_client.h"
#include "drivers/sd_card.h"
#include "ff.h"  // FatFs library for directory operations
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


// Global variables for block transfer
static block_assembly_t current_block = {0};
static uint16_t next_block_id = 1;

// Publisher-side: Store last sent block for retransmission
typedef struct {
    uint16_t block_id;
    uint8_t *data;          // Changed to non-const (we own this memory)
    size_t data_len;
    uint16_t total_parts;
    const char *topic;
    uint8_t qos;
    bool active;
    bool owns_data;         // Track if we allocated the data buffer
} sender_block_cache_t;

static sender_block_cache_t sender_cache = {0};

// Free cached block data
static void sender_cache_free(void) {
    if (sender_cache.active && sender_cache.owns_data && sender_cache.data) {
        free(sender_cache.data);
        sender_cache.data = NULL;
    }
    sender_cache.active = false;
    sender_cache.owns_data = false;
}

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
            printf("Failed to send chunk %d/%d\n", part, total_parts);
            return -1;
        }
        
        // Print progress every 50 chunks
        if (part % 50 == 0 || part == total_parts) {
            printf("Progress: %d/%d (%.1f%%)\n", 
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
    
    // Free any previously cached block data
    sender_cache_free();
    
    // Cache block data for potential retransmission
    // NOTE: We store the pointer but don't own it yet
    // The caller will set owns_data=true if we should keep it
    sender_cache.block_id = block_id;
    sender_cache.data = (uint8_t*)data;  // Cast away const
    sender_cache.data_len = data_len;
    sender_cache.total_parts = total_parts;
    sender_cache.topic = topic;
    sender_cache.qos = qos;
    sender_cache.active = true;
    sender_cache.owns_data = false;  // Caller will set this
    
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
        
        int ret;
        if (qos == 1) {
            // // QoS 1 - will wait for PUBACK, retry if timeout
            // int max_retries = 3;
            // ret = MQTTSN_ERROR;
            
            // for (int attempt = 1; attempt <= max_retries; attempt++) {
            //     ret = mqttsn_publish(topic, packet, packet_size, 1);
                
            //     if (ret == MQTTSN_OK) {
            //         break; // Success - PUBACK received
            //     } else if (attempt < max_retries) {
            //         printf("  Retry %d/%d for chunk %d (no PUBACK)\n", attempt, max_retries, part);
            //         sleep_ms(100); // Small delay before retry
            //     }
            // }
            
            // if (ret != MQTTSN_OK) {
            //     printf("Failed to send chunk %d/%d\n", part, total_parts);
            //     return -1;
            // }
            
            // Send with QoS 1 - will wait for PUBACK, retry if timeout
            int max_retries = -1; // New value: -1 signifies infinite retries
            int ret = MQTTSN_ERROR;

            // The loop condition is changed to:
            // (max_retries < 0) - keeps the loop going if max_retries is negative (i.e., -1)
            // OR (attempt <= max_retries) - runs for a finite count if max_retries is positive
            for (int attempt = 1; max_retries < 0 || attempt <= max_retries; attempt++) {
                ret = mqttsn_publish(topic, packet, packet_size, 1); // QoS 1
    
                if (ret == MQTTSN_OK) {
                    break; // Success - PUBACK received
                } else if (max_retries < 0 || attempt < max_retries) {
                    // Only print retry message if we are retrying (either infinitely or not yet done)
                    if (max_retries < 0) {
                        printf("  Retry %d (infinite mode) for chunk %d (no PUBACK)\n", attempt, part);
                    } else {
                        printf("  Retry %d/%d for chunk %d (no PUBACK)\n", attempt, max_retries, part);
                    }
                    sleep_ms(100); // Small delay before retry
                }
            }
            // The failure block is still reachable if 'ret' never becomes MQTTSN_OK
            // (i.e., if mqttsn_publish fails consistently and returns an error other than timeout)
            if (ret != MQTTSN_OK) {
                printf("Failed to send chunk %d/%d after %s retries\n", part, total_parts, max_retries < 0 ? "infinite" : "all allowed");
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
        
        // Print progress every 50 chunks
        if (part % 50 == 0 || part == total_parts) {
            printf("Progress: %d/%d (%.1f%%)\n", 
                   part, total_parts, (float)part * 100.0 / total_parts);
        }
        
        // Check for incoming messages every 20 chunks (including RETX requests)
        if (part % 20 == 0) {
            // Quick check for incoming messages without blocking
            extern int mqttsn_check_incoming_messages(void);
            mqttsn_check_incoming_messages();
        }
        
        // Minimal delay between chunks for faster transmission
        sleep_ms(5);
    }
    
    printf("Block transfer completed: %d chunks sent\n", total_parts);
    printf("[PUBLISHER] ℹ️  Block data cached for potential retransmission requests\n");
    return 0;
}

// Parse and handle retransmission request from subscriber
// Message format: "RETX:BLOCK=1,CHUNKS=10-15,20,25-30"
int block_transfer_handle_retransmit_request(const char *request_msg) {
    if (!sender_cache.active) {
        printf("[RETX] No active block to retransmit\n");
        return -1;
    }
    
    // Parse the message
    int requested_block_id = 0;
    const char *chunks_str = NULL;
    
    // Extract block ID
    if (sscanf(request_msg, "RETX:BLOCK=%d,CHUNKS=", &requested_block_id) != 1) {
        printf("[RETX] Failed to parse request message\n");
        return -2;
    }
    
    if (requested_block_id != sender_cache.block_id) {
        printf("[RETX] Block ID mismatch: requested=%d, cached=%d\n", 
               requested_block_id, sender_cache.block_id);
        return -3;
    }
    
    // Find the chunks list
    chunks_str = strstr(request_msg, "CHUNKS=");
    if (!chunks_str) {
        printf("[RETX] No chunks specified in request\n");
        return -4;
    }
    chunks_str += 7;  // Skip "CHUNKS="
    
    printf("\n========================================\n");
    printf("[RETX] 🔄 RETRANSMISSION REQUEST RECEIVED\n");
    printf("[RETX] Block ID: %d (cached: %d)\n", requested_block_id, sender_cache.block_id);
    printf("[RETX] Cached data available: %s (%zu bytes)\n", 
           sender_cache.data ? "YES" : "NO", sender_cache.data_len);
    printf("[RETX] Topic: %s, QoS: %d\n", sender_cache.topic, sender_cache.qos);
    printf("[RETX] Missing chunks: %s\n", chunks_str);
    printf("========================================\n");
    
    // Parse chunk ranges and resend (e.g., "10-15,20,25-30")
    char chunks_copy[256];
    strncpy(chunks_copy, chunks_str, sizeof(chunks_copy) - 1);
    chunks_copy[sizeof(chunks_copy) - 1] = '\0';
    
    size_t chunk_data_size = BLOCK_CHUNK_SIZE - sizeof(block_header_t);
    int chunks_resent = 0;
    
    char *token = strtok(chunks_copy, ",");
    while (token != NULL) {
        int start, end;
        
        // Check if it's a range (e.g., "10-15")
        if (strchr(token, '-')) {
            if (sscanf(token, "%d-%d", &start, &end) == 2) {
                // Resend range
                for (int part = start; part <= end && part <= sender_cache.total_parts; part++) {
                    size_t offset = (part - 1) * chunk_data_size;
                    size_t chunk_len = (offset + chunk_data_size > sender_cache.data_len) ? 
                                      (sender_cache.data_len - offset) : chunk_data_size;
                    
                    // Create packet with header + data
                    uint8_t packet[BLOCK_CHUNK_SIZE];
                    block_header_t *header = (block_header_t*)packet;
                    
                    header->block_id = sender_cache.block_id;
                    header->part_num = part;
                    header->total_parts = sender_cache.total_parts;
                    header->data_len = chunk_len;
                    
                    memcpy(packet + sizeof(block_header_t), sender_cache.data + offset, chunk_len);
                    
                    size_t packet_size = sizeof(block_header_t) + chunk_len;
                    
                    // Use QoS 0 for retransmission to avoid blocking
                    if (mqttsn_publish(sender_cache.topic, packet, packet_size, 0) == MQTTSN_OK) {
                        chunks_resent++;
                        // Only log every 10th chunk to reduce spam
                        if (chunks_resent % 10 == 0 || chunks_resent == 1) {
                            printf("[RETX] ✓ Sent %d chunks (last: %d)...\n", chunks_resent, part);
                        }
                    } else {
                        printf("[RETX] ✗ Failed to send chunk %d\n", part);
                    }
                    
                    // Increase delay to avoid overwhelming gateway/network
                    sleep_ms(15);
                }
            }
        } else {
            // Single chunk
            if (sscanf(token, "%d", &start) == 1) {
                int part = start;
                if (part >= 1 && part <= sender_cache.total_parts) {
                    size_t offset = (part - 1) * chunk_data_size;
                    size_t chunk_len = (offset + chunk_data_size > sender_cache.data_len) ? 
                                      (sender_cache.data_len - offset) : chunk_data_size;
                    
                    uint8_t packet[BLOCK_CHUNK_SIZE];
                    block_header_t *header = (block_header_t*)packet;
                    
                    header->block_id = sender_cache.block_id;
                    header->part_num = part;
                    header->total_parts = sender_cache.total_parts;
                    header->data_len = chunk_len;
                    
                    memcpy(packet + sizeof(block_header_t), sender_cache.data + offset, chunk_len);
                    
                    size_t packet_size = sizeof(block_header_t) + chunk_len;
                    
                    // Use QoS 0 for retransmission to avoid blocking
                    if (mqttsn_publish(sender_cache.topic, packet, packet_size, 0) == MQTTSN_OK) {
                        chunks_resent++;
                        // Only log every 10th chunk to reduce spam
                        if (chunks_resent % 10 == 0 || chunks_resent == 1) {
                            printf("[RETX] ✓ Sent %d chunks (last: %d)...\n", chunks_resent, part);
                        }
                    } else {
                        printf("[RETX] ✗ Failed to send chunk %d\n", part);
                    }
                    
                    // Increase delay to avoid overwhelming gateway/network
                    sleep_ms(15);
                }
            }
        }
        
        token = strtok(NULL, ",");
    }
    
    printf("\n[RETX] ========================================\n");
    printf("[RETX] ✅ RETRANSMISSION COMPLETE: %d chunks resent (QoS 0)\n", chunks_resent);
    printf("[RETX] ========================================\n\n");
    return chunks_resent;
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
    
    // DON'T free the buffer yet - we need it for retransmission!
    // The sender_cache now owns this buffer and will free it when:
    // 1. A new block transfer starts (replaces cache)
    // 2. Transfer is confirmed complete
    // For now, just mark that sender_cache owns this memory
    sender_cache.owns_data = true;
    
    if (ret == 0) {
        printf("✅ Image transfer completed - saved to GitHub repo\n");
        printf("📦 Block data cached in memory for retransmission (%.2f KB)\n", 
               image_size / 1024.0);
    } else {
        printf("❌ Image transfer failed\n");
        // On failure, free the buffer immediately
        free(image_buffer);
        sender_cache.active = false;
        sender_cache.owns_data = false;
    }
    
    return ret;
}

// Initialize block reassembly
static int init_block_assembly(uint16_t block_id, uint16_t total_parts) {
    // Clean up previous block if exists
    if (current_block.received_mask) {
        free(current_block.received_mask);
        current_block.received_mask = NULL;
    }
    if (current_block.data_buffer) {
        free(current_block.data_buffer);
        current_block.data_buffer = NULL;
    }
    
    // Initialize new block
    current_block.block_id = block_id;
    current_block.total_parts = total_parts;
    current_block.received_parts = 0;
    current_block.total_length = 0;
    current_block.last_update = to_ms_since_boot(get_absolute_time());
    current_block.transfer_finished = false;  // Start in active transfer mode
    
    // Allocate tracking arrays
    current_block.received_mask = (bool*)calloc(total_parts, sizeof(bool));
    if (!current_block.received_mask) {
        printf("ERR: Mask alloc\n");
        return -1;
    }
    
    current_block.data_buffer = (uint8_t*)malloc(BLOCK_BUFFER_SIZE);
    if (!current_block.data_buffer) {
        printf("ERR: Buf alloc\n");
        free(current_block.received_mask);
        current_block.received_mask = NULL;
        return -1;
    }
    
    printf("Block %d init: %d parts\n", block_id, total_parts);
    return 0;
}

// Process received block chunk
void process_block_chunk(const uint8_t *data, size_t len) {
    if (len < sizeof(block_header_t)) {
        printf("Error: Packet too small for block header\n");
        return;
    }
    
    // CRITICAL: Use memcpy to avoid unaligned access (ARM hard fault)
    block_header_t header;
    memcpy(&header, data, sizeof(block_header_t));
    
    const uint8_t *chunk_data = data + sizeof(block_header_t);
    size_t chunk_data_len = header.data_len;
    
    // Initialize block assembly if this is a new block
    if (current_block.block_id != header.block_id) {
        if (init_block_assembly(header.block_id, header.total_parts) != 0) {
            return;
        }
    }
    
    // Validate part number
    if (header.part_num < 1 || header.part_num > header.total_parts) {
        printf("Error: Invalid part number %d (total %d)\n", header.part_num, header.total_parts);
        return;
    }
    
    // Check if we already received this part
    uint16_t part_index = header.part_num - 1;
    if (current_block.received_mask[part_index]) {
        // Silently ignore duplicate
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
        if (header.part_num == header.total_parts) {
            current_block.total_length = buffer_offset + chunk_data_len;
        }
        
        // Print progress every 50 chunks or at milestones
        if (current_block.received_parts % 50 == 0 || current_block.received_parts == current_block.total_parts) {
            uint16_t missing = current_block.total_parts - current_block.received_parts;
            printf("📊 Progress: %d/%d (%.1f%%) | Missing: %d chunks\n", 
                   current_block.received_parts, current_block.total_parts,
                   (float)current_block.received_parts * 100.0 / current_block.total_parts,
                   missing);
        }
        
        // Check if block is complete
        if (current_block.received_parts == current_block.total_parts) {
            printf("\n=== ✅ BLOCK TRANSFER COMPLETE ===\n");
            printf("Block ID: %d\n", current_block.block_id);
            printf("Status: SUCCESS - All chunks received\n");
            printf("Chunks: %d/%d (100%%)\n", current_block.received_parts, current_block.total_parts);
            printf("Total size: %d bytes (%.2f KB)\n", current_block.total_length, 
                   current_block.total_length / 1024.0);
            
            // Show first 32 bytes as hex preview (safe for binary data)
            printf("Data preview (first 32 bytes hex): ");
            int preview_len = (current_block.total_length < 32) ? current_block.total_length : 32;
            for (int i = 0; i < preview_len; i++) {
                printf("%02X ", current_block.data_buffer[i]);
            }
            printf("\n");
            
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
    // Check for block assembly timeout
    // Mark transfer as finished to enable retransmission only when appropriate
    if (current_block.block_id != 0 && !current_block.transfer_finished) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        uint32_t elapsed = now - current_block.last_update;
        
        // Calculate expected time: ~50ms per chunk with faster transmission
        uint32_t expected_time = current_block.total_parts * 50;
        uint32_t min_wait_time = (expected_time > 20000) ? expected_time : 20000;
        
        // Only mark as finished if:
        // 1. No progress for 10+ seconds AND
        // 2. We've waited at least as long as expected transfer time AND
        // 3. We've received at least 50% of chunks (to avoid premature triggers)
        if (elapsed > 10000 && 
            elapsed > min_wait_time && 
            current_block.received_parts >= (current_block.total_parts / 2)) {
            
            uint16_t missing_chunks = current_block.total_parts - current_block.received_parts;
            printf("\n=== ⚠️  INITIAL TRANSFER COMPLETE (TIMEOUT) ===\n");
            printf("Block ID: %d\n", current_block.block_id);
            printf("Status: Initial transmission finished\n");
            printf("Chunks received: %d/%d (%.1f%%)\n",
                   current_block.received_parts, current_block.total_parts,
                   (float)current_block.received_parts * 100.0 / current_block.total_parts);
            printf("Missing chunks: %d\n", missing_chunks);
            
            current_block.transfer_finished = true;  // Enable retransmission requests
            
            // List missing chunk numbers (up to first 20)
            printf("Missing chunk IDs: ");
            int shown = 0;
            for (uint16_t i = 0; i < current_block.total_parts && shown < 20; i++) {
                if (!current_block.received_mask[i]) {
                    printf("%d ", i + 1);
                    shown++;
                }
            }
            if (missing_chunks > 20) {
                printf("... (%d more)", missing_chunks - 20);
            }
            printf("\n");
            printf("🔄 Will continue requesting retransmission...\n");
            
            // DON'T reset - keep trying to get missing chunks
            // Update last_update to prevent spam
            current_block.last_update = now;
        }
    }
}

void block_transfer_print_status(void) {
    if (current_block.block_id == 0) {
        printf("[Block Transfer] No active transfer\n");
        return;
    }
    
    uint16_t missing = current_block.total_parts - current_block.received_parts;
    uint32_t elapsed_sec = (to_ms_since_boot(get_absolute_time()) - current_block.last_update) / 1000;
    
    printf("\n[Block Transfer Status]\n");
    printf("  Block ID: %d\n", current_block.block_id);
    printf("  Progress: %d/%d chunks (%.1f%%)\n", 
           current_block.received_parts, current_block.total_parts,
           (float)current_block.received_parts * 100.0 / current_block.total_parts);
    printf("  Missing: %d chunks\n", missing);
    printf("  Time since last chunk: %lu seconds\n", elapsed_sec);
    
    if (missing > 0 && missing <= 20) {
        printf("  Missing chunk IDs: ");
        for (uint16_t i = 0; i < current_block.total_parts; i++) {
            if (!current_block.received_mask[i]) {
                printf("%d ", i + 1);
            }
        }
        printf("\n");
    }
}

int block_transfer_get_missing_count(void) {
    if (current_block.block_id == 0) {
        return 0;
    }
    return current_block.total_parts - current_block.received_parts;
}

// Request retransmission of missing chunks
// Returns 0 on success, negative on error
int block_transfer_request_missing_chunks(void) {
    if (current_block.block_id == 0) {
        printf("[RETX] No active transfer\n");
        return -1;
    }
    
    if (!current_block.transfer_finished) {
        // Don't request retransmission during initial transfer
        return 0;
    }
    
    uint16_t missing = current_block.total_parts - current_block.received_parts;
    if (missing == 0) {
        printf("[RETX] No missing chunks\n");
        return 0;
    }
    
    // Build retransmit request message with missing chunk ranges
    char retx_msg[256];
    int offset = snprintf(retx_msg, sizeof(retx_msg), 
                         "RETX:BLOCK=%d,CHUNKS=", current_block.block_id);
    
    // Find missing chunks and encode as ranges (e.g., "10-15,20,25-30")
    int chunks_added = 0;
    uint16_t range_start = 0;
    uint16_t range_end = 0;
    bool in_range = false;
    
    for (uint16_t i = 0; i < current_block.total_parts && chunks_added < 50; i++) {
        if (!current_block.received_mask[i]) {
            if (!in_range) {
                range_start = i + 1;
                range_end = i + 1;
                in_range = true;
            } else {
                range_end = i + 1;
            }
        } else {
            if (in_range) {
                // End of range, add to message
                if (range_start == range_end) {
                    offset += snprintf(retx_msg + offset, sizeof(retx_msg) - offset,
                                     "%d,", range_start);
                } else {
                    offset += snprintf(retx_msg + offset, sizeof(retx_msg) - offset,
                                     "%d-%d,", range_start, range_end);
                }
                chunks_added++;
                in_range = false;
            }
        }
    }
    
    // Handle final range
    if (in_range) {
        if (range_start == range_end) {
            offset += snprintf(retx_msg + offset, sizeof(retx_msg) - offset,
                             "%d", range_start);
        } else {
            offset += snprintf(retx_msg + offset, sizeof(retx_msg) - offset,
                             "%d-%d", range_start, range_end);
        }
    } else if (offset > 0 && retx_msg[offset - 1] == ',') {
        retx_msg[offset - 1] = '\0';  // Remove trailing comma
    }
    
    printf("[RETX] Requesting %d missing chunks: %s\n", missing, retx_msg);
    
    // Send retransmit request with QoS 0 (fire-and-forget) to avoid blocking
    int ret = mqttsn_publish("pico/retransmit", (uint8_t*)retx_msg, strlen(retx_msg), 0);
    
    if (ret == MQTTSN_OK) {
        printf("[RETX] Request sent successfully\n");
        return 0;
    } else {
        printf("[RETX] Failed to send request\n");
        return -2;
    }
}
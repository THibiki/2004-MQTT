// block_transfer.c
// Simple, clean block transfer implementation for MQTT-SN QoS 0/1/2
// Sends images from SD card to SD card between Raspberry Pi Pico W devices

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "drivers/block_transfer.h"
#include "drivers/sd_card.h"
#include "protocol/mqttsn/mqttsn_adapter.h"

// External MQTT-SN functions
extern int mqttsn_demo_publish_name(const char *topicname, const uint8_t *payload, int payloadlen);
extern int mqttsn_get_qos(void);
extern void mqttsn_set_qos(int qos);
extern int mqttsn_check_incoming_messages(void);  // Process NACK during send

// ============================================================================
// SENDER STATE
// ============================================================================

static struct {
    uint16_t block_id;
    uint8_t *data;
    size_t data_len;
    uint16_t total_chunks;
    bool active;
} sender = {0};

// ============================================================================
// RECEIVER STATE
// ============================================================================

static struct {
    uint16_t block_id;
    uint16_t total_chunks;
    uint16_t received_count;
    uint16_t highest_chunk_received;  // Highest chunk number seen (tracks send progress)
    bool *chunk_mask;        // Track which chunks received
    uint8_t *buffer;         // Reassembly buffer
    uint32_t buffer_size;
    bool active;
    uint32_t last_update;    // Timestamp of last chunk
} receiver = {0};

// ============================================================================
// INITIALIZATION
// ============================================================================

int block_transfer_init(void) {
    printf("[BLOCK] Block transfer system initialized\n");
    return 0;
}

// ============================================================================
// SENDER: SEND IMAGE FILE FROM SD CARD WITH QoS
// ============================================================================

void block_transfer_reset_sender(void) {
    if (sender.data) {
        free(sender.data);
        sender.data = NULL;
    }
    sender.active = false;
    sender.data_len = 0;
    sender.total_chunks = 0;
    sender.block_id = 0;
    printf("[SENDER] ✓ Reset complete, ready for new transfer\n");
}

int send_image_file_qos(const char *topic, const char *filename, uint8_t qos) {
    if (sender.active) {
        printf("[SENDER] ✗ Another transfer in progress\n");
        return -1;
    }
    
    // Allocate buffer for file data
    uint8_t *file_buffer = malloc(BLOCK_BUFFER_SIZE);
    if (!file_buffer) {
        printf("[SENDER] ✗ Failed to allocate buffer\n");
        return -2;
    }
    
    // Read file from SD card
    size_t actual_size = 0;
    int rc = sd_card_read_file(filename, file_buffer, BLOCK_BUFFER_SIZE, &actual_size);
    if (rc != 0) {
        printf("[SENDER] ✗ Failed to read file '%s' (err=%d)\n", filename, rc);
        free(file_buffer);
        return -3;
    }
    
    if (actual_size == 0 || actual_size > MAX_SUPPORTED_FILE_SIZE) {
        printf("[SENDER] ✗ Invalid file size: %zu bytes\n", actual_size);
        free(file_buffer);
        return -4;
    }
    
    printf("[SENDER] ✓ Read file '%s': %zu bytes\n", filename, actual_size);
    
    // This allows NACK retransmissions to work during the transfer
    sender.data = file_buffer;
    sender.data_len = actual_size;
    sender.active = true;
    printf("[SENDER] ✓ Sender state activated, ready for transfer and retransmissions\n");
    
    // Set QoS level
    int prev_qos = mqttsn_get_qos();
    mqttsn_set_qos(qos);
    
    // Send block transfer
    int result = send_block_transfer_qos(topic, file_buffer, actual_size, qos);
    
    // Restore previous QoS
    mqttsn_set_qos(prev_qos);
    
    // On failure, clean up sender state
    if (result != 0) {
        printf("[SENDER] ✗ Transfer failed, cleaning up\n");
        free(file_buffer);
        sender.data = NULL;
        sender.data_len = 0;
        sender.active = false;
    } else {
        printf("[SENDER] ✓ Transfer complete, keeping buffer for retransmissions\n");
    }
    
    return result;
}

int send_image_file(const char *topic, const char *filename) {
    return send_image_file_qos(topic, filename, mqttsn_get_qos());
}

// ============================================================================
// SENDER: SEND BLOCK TRANSFER WITH QoS
// ============================================================================

int send_block_transfer_qos(const char *topic, const uint8_t *data, size_t data_len, uint8_t qos) {
    if (data_len > MAX_SUPPORTED_FILE_SIZE) {
        printf("[SENDER] ✗ Data too large: %zu bytes (max %d)\n", data_len, MAX_SUPPORTED_FILE_SIZE);
        return -1;
    }
    
    // Calculate number of chunks
    uint16_t total_chunks = (data_len + BLOCK_CHUNK_SIZE - 1) / BLOCK_CHUNK_SIZE;
    uint16_t block_id = (uint16_t)(to_ms_since_boot(get_absolute_time()) & 0xFFFF);
    
    printf("[SENDER] Starting transfer: BlockID=%u, Size=%zu bytes, Chunks=%u, QoS=%d\n",
           block_id, data_len, total_chunks, qos);
    
    // Store sender state for retransmissions
    sender.block_id = block_id;
    sender.total_chunks = total_chunks;
    
    // Prepare buffer for ONE chunk (header + data)
    uint8_t chunk_buffer[sizeof(block_header_t) + BLOCK_CHUNK_SIZE];
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    
    for (uint16_t chunk_num = 1; chunk_num <= total_chunks; chunk_num++) {
        // Build chunk header
        block_header_t *header = (block_header_t *)chunk_buffer;
        header->block_id = block_id; // Which transfer
        header->part_num = chunk_num; // THIS IS CHUNK #Nnumber
        header->total_parts = total_chunks; //Total chunks
        
        // Calculate data length for this chunk
        size_t offset = (chunk_num - 1) * BLOCK_CHUNK_SIZE;
        size_t remaining = data_len - offset;
        size_t chunk_data_len = (remaining < BLOCK_CHUNK_SIZE) ? remaining : BLOCK_CHUNK_SIZE;
        header->data_len = chunk_data_len;
        
        // Copy chunk data
        memcpy(chunk_buffer + sizeof(block_header_t), data + offset, chunk_data_len);
        size_t total_len = sizeof(block_header_t) + chunk_data_len;
        
        // Send via MQTT-SN with limited retries (let NACK handle failures)
        int rc = mqttsn_demo_publish_name(topic, chunk_buffer, total_len);
        int retry_count = 0;
        while (rc != 0 && retry_count < 3) {
            retry_count++;
            printf("[SENDER] ⚠️  Chunk %u/%u failed (attempt %d/3), retrying...\n", 
                   chunk_num, total_chunks, retry_count);
            sleep_ms(50);
            rc = mqttsn_demo_publish_name(topic, chunk_buffer, total_len);
        }
        
        if (rc != 0) {
            printf("[SENDER] ⚠️  Chunk %u/%u failed after 3 attempts, continuing (will be retransmitted via NACK)\n", 
                   chunk_num, total_chunks);
            // let NACK retransmission handle missing chunks
        }
        
        // Progress indicator with ETA (at 25, 75, 125... to avoid collision with receiver at 50, 100, 150...)
        if ((chunk_num % 50 == 25) || chunk_num == total_chunks) {
            uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start_time;
            float progress = (chunk_num * 100.0f) / total_chunks;
            uint32_t eta = 0;
            if (chunk_num > 0) {
                eta = (elapsed * (total_chunks - chunk_num)) / chunk_num;
            }
            printf("[SENDER] %u/%u (%.1f%%) | %lus elapsed, ~%lus remaining\n", 
                   chunk_num, total_chunks, progress, elapsed/1000, eta/1000);
        }
        
        // QoS 2 handshake (~200-300ms) provides natural flow control
        sleep_ms(10);  // Small delay to prevent overwhelming UDP buffers
        
        // Extra pause every 20 chunks to let subscriber process and drain buffers
        if (chunk_num % 20 == 0) {
            sleep_ms(50);  // Let gateway/subscriber catch up
        }
    }
    
    uint32_t total_time = to_ms_since_boot(get_absolute_time()) - start_time;
    float speed_kbps = (data_len * 8.0f) / total_time;
    
    printf("[SENDER] ✓ Transfer complete: %zu bytes in %lu ms (%.2f kbps)\n",
           data_len, total_time, speed_kbps);
    
    return 0;
}

int send_block_transfer(const char *topic, const uint8_t *data, size_t data_len) {
    return send_block_transfer_qos(topic, data, data_len, mqttsn_get_qos());
}

// ============================================================================
// RECEIVER: PROCESS INCOMING CHUNK
// ============================================================================

void process_block_chunk(const uint8_t *data, size_t len) {
    // Validate input
    if (!data || len < sizeof(block_header_t) || len > 512) {
        return;  // Silently drop invalid chunks
    }
    
    // Parse header safely (avoid alignment issues)
    block_header_t header;
    memcpy(&header, data, sizeof(block_header_t));
    
    uint16_t block_id = header.block_id;
    uint16_t part_num = header.part_num;
    uint16_t total_parts = header.total_parts;
    uint16_t data_len = header.data_len;
    
    // Validate
    if (part_num == 0 || part_num > total_parts || total_parts > BLOCK_MAX_CHUNKS) {
        printf("[RECEIVER] ✗ Invalid chunk: part=%u, total=%u\n", part_num, total_parts);
        return;
    }
    
    if (data_len > BLOCK_CHUNK_SIZE || len < sizeof(block_header_t) + data_len) {
        printf("[RECEIVER] ✗ Invalid data length: %u bytes\n", data_len);
        return;
    }
    
    // New transfer
    if (!receiver.active || receiver.block_id != block_id) {
        // Validate buffer size first
        size_t required_buffer = (size_t)total_parts * BLOCK_CHUNK_SIZE;
        size_t required_mask = total_parts * sizeof(bool);
        size_t total_required = required_buffer + required_mask;
        
        printf("[RECEIVER] New transfer: BlockID=%u, TotalChunks=%u\n", block_id, total_parts);
        printf("[RECEIVER] Memory required: %zu bytes (buffer=%zu, mask=%zu)\n",
               total_required, required_buffer, required_mask);
        
        // Maximum safe allocation is 55KB to leave headroom
        if (total_required > 55000) {
            printf("[RECEIVER] ✗ Transfer too large: %zu bytes (max 55KB)\n", total_required);
            printf("[RECEIVER] ✗ Reduce file size to ~430 chunks max\n");
            return;
        }
        
        // Warn if approaching limits
        if (total_required > 50000) {
            printf("[RECEIVER] ⚠️  Large transfer: %zu bytes (close to 55KB limit)\n", total_required);
        }
        
        // Clean up old transfer
        if (receiver.chunk_mask) {
            free(receiver.chunk_mask);
            receiver.chunk_mask = NULL;
        }
        if (receiver.buffer) {
            free(receiver.buffer);
            receiver.buffer = NULL;
        }
        
        // Initialize receiver
        receiver.block_id = block_id;
        receiver.total_chunks = total_parts;
        receiver.received_count = 0;
        receiver.highest_chunk_received = 0;
        receiver.buffer_size = required_buffer;
        
        // Test allocation first (catches heap exhaustion early)
        void *test_ptr = malloc(required_buffer);
        if (!test_ptr) {
            printf("[RECEIVER] ✗ Heap exhausted - cannot allocate %zu bytes\n", required_buffer);
            printf("[RECEIVER] ✗ Try: Smaller file, reboot Pico, or disable debug logs\n");
            receiver.active = false;
            return;
        }
        free(test_ptr);
        printf("[RECEIVER] ✓ Heap check passed\n");
        
        // Allocate chunk mask first (smaller)
        receiver.chunk_mask = calloc(total_parts, sizeof(bool));
        if (!receiver.chunk_mask) {
            printf("[RECEIVER] ✗ Failed to allocate chunk mask (%zu bytes)\n", required_mask);
            receiver.active = false;
            return;
        }
        printf("[RECEIVER] ✓ Chunk mask allocated: %zu bytes\n", required_mask);
        
        // Allocate buffer
        receiver.buffer = malloc(receiver.buffer_size);
        if (!receiver.buffer) {
            printf("[RECEIVER] ✗ Failed to allocate buffer (%zu bytes)\n", receiver.buffer_size);
            printf("[RECEIVER] ✗ Heap fragmented or exhausted\n");
            free(receiver.chunk_mask);
            receiver.chunk_mask = NULL;
            receiver.active = false;
            return;
        }
        
        // Zero the buffer to ensure clean state (also tests write access)
        printf("[RECEIVER] Zeroing buffer...\n");
        memset(receiver.buffer, 0, receiver.buffer_size);
        printf("[RECEIVER] ✓ Buffer allocated and zeroed: %zu bytes\n", receiver.buffer_size);
        printf("[RECEIVER] ✓ Ready to receive chunks\n");
        
        receiver.active = true;
        receiver.last_update = to_ms_since_boot(get_absolute_time());
    }
    
    // Skip duplicates
    if (receiver.chunk_mask[part_num - 1]) {
        return;
    }
    
    // Store chunk data with bounds check
    size_t offset = (part_num - 1) * BLOCK_CHUNK_SIZE;
    if (offset + data_len > receiver.buffer_size) {
        printf("[RECEIVER] ✗ Buffer overflow prevented: offset=%zu, data_len=%u, buffer_size=%zu\n",
               offset, data_len, receiver.buffer_size);
        return;
    }
    
    // Copy chunk data
    memcpy(receiver.buffer + offset, data + sizeof(block_header_t), data_len);
    receiver.chunk_mask[part_num - 1] = true;
    receiver.received_count++;
    
    // Track highest chunk number seen (indicates send progress)
    if (part_num > receiver.highest_chunk_received) {
        receiver.highest_chunk_received = part_num;
    }
    
    // Yield CPU to prevent watchdog timeout
    tight_loop_contents();
    receiver.last_update = to_ms_since_boot(get_absolute_time());
    
    // Progress indicator (show every 50 chunks for cleaner output)
    if (receiver.received_count % 50 == 0 || receiver.received_count == receiver.total_chunks) {
        float progress = (receiver.received_count * 100.0f) / receiver.total_chunks;
        uint16_t missing = receiver.total_chunks - receiver.received_count;
        printf("\n[BLOCK] %u/%u chunks (%.1f%%) | Missing: %u\n",
               receiver.received_count, receiver.total_chunks, progress, missing);
    }
    
    // Transfer complete
    if (receiver.received_count == receiver.total_chunks) {
        printf("\n========================================\n");
        printf("[SUCCESS] Transfer complete: %u chunks\n", receiver.total_chunks);
        
        // Calculate actual data size (last chunk may be partial)
        size_t total_size = ((receiver.total_chunks - 1) * BLOCK_CHUNK_SIZE) + data_len;
        
        // Detect file type and save to SD card
        const char *extension = ".bin";
        if (total_size >= 3) {
            if (receiver.buffer[0] == 0xFF && receiver.buffer[1] == 0xD8 && receiver.buffer[2] == 0xFF) {
                extension = ".jpg";
            } else if (receiver.buffer[0] == 0x89 && receiver.buffer[1] == 0x50 && 
                       receiver.buffer[2] == 0x4E && receiver.buffer[3] == 0x47) {
                extension = ".png";
            } else if (receiver.buffer[0] == 0x47 && receiver.buffer[1] == 0x49 && 
                       receiver.buffer[2] == 0x46) {
                extension = ".gif";
            }
        }
        
        // Generate filename
        char filename[64];
        snprintf(filename, sizeof(filename), "block_%u%s", block_id, extension);
        
        // Write to SD card
        int rc = sd_card_write_file(filename, receiver.buffer, total_size);
        if (rc == 0) {
            printf("[SD CARD] Saved: %s (%zu bytes)\n", filename, total_size);
            printf("========================================\n\n");
        } else {
            printf("[ERROR] SD card save failed (err=%d)\n", rc);
            printf("========================================\n\n");
        }
        
        // Clean up
        free(receiver.chunk_mask);
        receiver.chunk_mask = NULL;
        free(receiver.buffer);
        receiver.buffer = NULL;
        receiver.active = false;
    }
}

// ============================================================================
// RETRANSMISSION: NACK-BASED MISSING CHUNKS
// ============================================================================

int block_transfer_get_missing_count(void) {
    if (!receiver.active) return 0;
    return receiver.total_chunks - receiver.received_count;
}

int block_transfer_request_missing_chunks(void) {
    if (!receiver.active) {
        return -1;
    }
    
    if (receiver.received_count == receiver.total_chunks) {
        return -2;  // Already complete
    }
    
    // Wait for publisher to finish sending before requesting retransmissions
    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t time_since_last_chunk = now - receiver.last_update;
    
    if (time_since_last_chunk < 3000) {
        // Publisher still actively sending chunks, don't spam with NACKs yet
        // Wait until there's a 3-second gap (indicating send complete or stall)
        return -4;  // Not an error, just waiting
    }
    
    // Only request chunks up to highest received (don't ask for chunks not sent yet)
    uint16_t request_up_to = receiver.highest_chunk_received;
    if (request_up_to == 0) {
        printf("[RECEIVER] No chunks received yet, waiting...\n");
        return -3;
    }
    
    // Build NACK message: "NACK:BLOCK=123,CHUNKS=1,3,5-8,10"
    char nack_msg[256];
    int pos = snprintf(nack_msg, sizeof(nack_msg), "NACK:BLOCK=%u,CHUNKS=", receiver.block_id);
    
    int count = 0;
    int range_start = -1;
    int range_end = -1;
    
    // Only check chunks up to highest received
    for (uint16_t i = 0; i < request_up_to && i < receiver.total_chunks; i++) {
        if (!receiver.chunk_mask[i]) {
            uint16_t chunk_num = i + 1;
            
            if (range_start == -1) {
                range_start = chunk_num;
                range_end = chunk_num;
            } else if (chunk_num == range_end + 1) {
                range_end = chunk_num;
            } else {
                // Write previous range
                if (count > 0) pos += snprintf(nack_msg + pos, sizeof(nack_msg) - pos, ",");
                if (range_start == range_end) {
                    pos += snprintf(nack_msg + pos, sizeof(nack_msg) - pos, "%d", range_start);
                } else {
                    pos += snprintf(nack_msg + pos, sizeof(nack_msg) - pos, "%d-%d", range_start, range_end);
                }
                count++;
                range_start = chunk_num;
                range_end = chunk_num;
            }
        }
    }
    
    // Write last range
    if (range_start != -1) {
        if (count > 0) pos += snprintf(nack_msg + pos, sizeof(nack_msg) - pos, ",");
        if (range_start == range_end) {
            pos += snprintf(nack_msg + pos, sizeof(nack_msg) - pos, "%d", range_start);
        } else {
            pos += snprintf(nack_msg + pos, sizeof(nack_msg) - pos, "%d-%d", range_start, range_end);
        }
        count++;  // Count the last range
    }
    
    // Don't send empty NACK if no chunks are missing (within the range we're checking)
    if (count == 0) {
        printf("[NACK] No missing chunks in range 1-%u (still waiting for chunks %u-%u)\n",
               request_up_to, request_up_to + 1, receiver.total_chunks);
        return -5;  // Not an error, just nothing to request yet
    }
    
    // Send NACK via MQTT-SN QoS 0 (fast, non-blocking)
    int prev_qos = mqttsn_get_qos();
    mqttsn_set_qos(0);
    int rc = mqttsn_demo_publish_name("pico/retransmit", (uint8_t *)nack_msg, strlen(nack_msg));
    mqttsn_set_qos(prev_qos);
    
    if (rc == 0) {
        printf("\n[NACK] Requesting %u missing chunks (up to chunk %u): %s\n", 
               count, request_up_to, nack_msg);
    } else {
        printf("\n[ERROR] Failed to send NACK (err=%d)\n", rc);
    }
    
    return rc;
}

// ============================================================================
// SENDER: HANDLE RETRANSMISSION REQUEST
// ============================================================================

int block_transfer_handle_retransmit_request(const char *request_msg) {
    if (!sender.active) {
        printf("[SENDER] ✗ No active transfer for retransmission\n");
        return -1;
    }
    
    // Parse NACK message: "NACK:BLOCK=123,CHUNKS=1,3,5-8,10"
    uint16_t req_block_id = 0;
    const char *chunks_ptr = strstr(request_msg, "BLOCK=");
    if (chunks_ptr) {
        sscanf(chunks_ptr + 6, "%hu", &req_block_id);
    }
    
    if (req_block_id != sender.block_id) {
        printf("[SENDER] ✗ NACK for different block: %u (current=%u)\n", req_block_id, sender.block_id);
        return -2;
    }
    
    chunks_ptr = strstr(request_msg, "CHUNKS=");
    if (!chunks_ptr) {
        printf("[SENDER] ✗ Invalid NACK format\n");
        return -3;
    }
    chunks_ptr += 7;
    
    printf("[SENDER] 📥 Processing NACK: %s\n", request_msg);
    
    // Parse chunk numbers and ranges (need to copy string for strtok)
    char chunks_copy[256];
    strncpy(chunks_copy, chunks_ptr, sizeof(chunks_copy) - 1);
    chunks_copy[sizeof(chunks_copy) - 1] = '\0';
    
    uint8_t chunk_buffer[sizeof(block_header_t) + BLOCK_CHUNK_SIZE];
    int retx_count = 0;
    
    char *token = strtok(chunks_copy, ",");
    while (token) {
        int start, end;
        if (strchr(token, '-')) {
            // Range: "5-8"
            sscanf(token, "%d-%d", &start, &end);
        } else {
            // Single: "3"
            sscanf(token, "%d", &start);
            end = start;
        }
        
        // Retransmit chunks in range
        for (int chunk_num = start; chunk_num <= end; chunk_num++) {
            if (chunk_num < 1 || chunk_num > sender.total_chunks) {
                continue;
            }
            
            // Build chunk
            block_header_t *header = (block_header_t *)chunk_buffer;
            header->block_id = sender.block_id;
            header->part_num = chunk_num;
            header->total_parts = sender.total_chunks;
            
            size_t offset = (chunk_num - 1) * BLOCK_CHUNK_SIZE;
            size_t remaining = sender.data_len - offset;
            size_t chunk_data_len = (remaining < BLOCK_CHUNK_SIZE) ? remaining : BLOCK_CHUNK_SIZE;
            header->data_len = chunk_data_len;
            
            memcpy(chunk_buffer + sizeof(block_header_t), sender.data + offset, chunk_data_len);
            size_t total_len = sizeof(block_header_t) + chunk_data_len;
            
            // Retransmit with QoS 0 for speed
            int prev_qos = mqttsn_get_qos();
            mqttsn_set_qos(0);
            int rc = mqttsn_demo_publish_name("pico/chunks", chunk_buffer, total_len);
            mqttsn_set_qos(prev_qos);
            
            if (rc == 0) {
                retx_count++;
                // Light pacing for retransmissions (QoS 0 needs some delay)
                sleep_ms(5);
            } else {
                printf("[SENDER] ✗ Failed to retransmit chunk %d\n", chunk_num);
            }
        }
        
        token = strtok(NULL, ",");
    }
    
    printf("[SENDER] ✓ Retransmitted %d chunks\n", retx_count);
    return 0;
}

// ============================================================================
// STATUS AND UTILITIES
// ============================================================================

bool block_transfer_is_active(void) {
    return receiver.active;
}

void block_transfer_print_status(void) {
    if (receiver.active) {
        float progress = (receiver.received_count * 100.0f) / receiver.total_chunks;
        uint16_t missing = receiver.total_chunks - receiver.received_count;
        printf("[RECEIVER] Status: %u/%u chunks (%.1f%%) | Missing: %u\n",
               receiver.received_count, receiver.total_chunks, progress, missing);
    } else {
        printf("[RECEIVER] No active transfer\n");
    }
}

void block_transfer_check_timeout(void) {
    if (!receiver.active) return;
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t elapsed = now - receiver.last_update;
    
    // Timeout after 60 seconds of no chunks
    if (elapsed > 60000) {
        printf("[RECEIVER] ⚠ Transfer timeout: no chunks for %lu seconds\n", elapsed / 1000);
        
        // Clean up
        if (receiver.chunk_mask) free(receiver.chunk_mask);
        if (receiver.buffer) free(receiver.buffer);
        receiver.active = false;
    }
}

void generate_large_message(char *buffer, size_t size) {
    for (size_t i = 0; i < size - 1; i++) {
        buffer[i] = 'A' + (i % 26);
    }
    buffer[size - 1] = '\0';
}

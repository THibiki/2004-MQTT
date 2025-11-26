#ifndef BLOCK_TRANSFER_H
#define BLOCK_TRANSFER_H

#include "pico/stdlib.h"

// Block transfer constants
#define BLOCK_CHUNK_SIZE 128        // Size of each chunk (adjust based on MQTT-SN packet limits)
#define BLOCK_MAX_CHUNKS 1000       // Maximum number of chunks per block
#define BLOCK_BUFFER_SIZE 60000     // 60KB buffer - safe for Pico W's 264KB RAM
#define MAX_SUPPORTED_FILE_SIZE 58000  // Maximum file size we can handle (58KB)

// Block transfer header structure (packed to avoid alignment issues)
typedef struct __attribute__((packed)) {
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
    bool transfer_finished; // True when initial transfer completes (timeout or all received)
} block_assembly_t;

// Block transfer functions
int block_transfer_init(void);
int send_block_transfer(const char *topic, const uint8_t *data, size_t data_len);
int send_block_transfer_qos(const char *topic, const uint8_t *data, size_t data_len, uint8_t qos);
int send_image_file(const char *topic, const char *filename);
int send_image_file_qos(const char *topic, const char *filename, uint8_t qos);
void process_block_chunk(const uint8_t *data, size_t len);
void generate_large_message(char *buffer, size_t size);
bool block_transfer_is_active(void);
void block_transfer_check_timeout(void);
void block_transfer_print_status(void);
int block_transfer_request_missing_chunks(void);
int block_transfer_get_missing_count(void);
int block_transfer_handle_retransmit_request(const char *request_msg);
void block_transfer_reset_sender(void);

#endif // BLOCK_TRANSFER_H
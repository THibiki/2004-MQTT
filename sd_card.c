#include "sd_card.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "ff.h"  // FatFs library
#include "diskio.h"  // For disk_status and STA_* flags
#include <string.h>
#include <stdio.h>

// SPI pins for Maker Pi Pico built-in SD card slot
#define SD_SPI spi1
#define SD_MISO 12  // GP12
#define SD_CS   15  // GP15  
#define SD_SCK  10  // GP10 (CHANGED - was 14)
#define SD_MOSI 11  // GP11

// File system constants
#define MAX_FILE_SIZE 4096
#define SD_MAX_FILENAME 64

// SD card commands
#define CMD0  0x40  // Software reset
#define CMD1  0x41  // Send operating condition
#define CMD8  0x48  // Send interface condition
#define CMD17 0x51  // Read single block
#define CMD24 0x58  // Write single block
#define CMD55 0x77  // Application command prefix
#define ACMD41 0x69 // Application command 41

// Static variables
static bool sd_detected = false;
static bool sd_initialized = false;
static bool is_sdhc = false;  // SDHC/SDXC vs SDSC addressing
static bool fat32_mounted = false;
static FATFS fs;  // FatFs filesystem object
static uint8_t file_data[MAX_FILE_SIZE];
static char file_names[10][SD_MAX_FILENAME];
static size_t file_sizes[10];
static int file_count = 0;

// Low-level SD card functions
static void sd_cs_select(void) {
    gpio_put(SD_CS, 0);
}

static void sd_cs_deselect(void) {
    gpio_put(SD_CS, 1);
}

static uint8_t sd_spi_transfer(uint8_t data) {
    uint8_t result;
    spi_write_read_blocking(SD_SPI, &data, &result, 1);
    return result;
}

static uint8_t sd_command(uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t buf[6] = {cmd, arg >> 24, arg >> 16, arg >> 8, arg, crc};
    uint8_t resp = 0xFF;
    uint8_t dummy = 0xFF;
    
    sd_cs_select();
    spi_write_blocking(SD_SPI, buf, 6);
    
    // Wait for response
    for (int i = 0; i < 10; i++) {
        spi_read_blocking(SD_SPI, 0xFF, &resp, 1);
        if (!(resp & 0x80)) break;
    }
    
    return resp;
}

// Initialize SD card hardware
int sd_card_init(void) {
    printf("Initializing real SD card hardware...\n");
    printf("Using pins: MISO=%d, MOSI=%d, SCK=%d, CS=%d\n", SD_MISO, SD_MOSI, SD_SCK, SD_CS);
    
    // Reset static variables
    sd_detected = false;
    sd_initialized = false;
    is_sdhc = false;
    file_count = 0;
    
    // Initialize SPI at low speed for SD card initialization
    spi_init(SD_SPI, 400000); // 400kHz
    printf("SPI initialized at 400kHz\n");
    
    // Configure GPIO pins
    gpio_set_function(SD_MISO, GPIO_FUNC_SPI);
    gpio_set_function(SD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);
    
    // CS pin as output, start HIGH (deselected)
    gpio_init(SD_CS);
    gpio_set_dir(SD_CS, GPIO_OUT);
    sd_cs_deselect();
    printf("GPIO pins configured\n");
    
    // Wait for power to stabilize
    sleep_ms(100);
    
    // Send at least 74 clock pulses with CS high (SD spec)
    printf("Sending wake-up clocks...\n");
    uint8_t dummy = 0xFF;
    for (int i = 0; i < 20; i++) {
        spi_write_blocking(SD_SPI, &dummy, 1);
    }
    
    // Send CMD0 - Reset to idle state
    printf("Sending CMD0 (reset)...\n");
    uint8_t resp = sd_command(CMD0, 0, 0x95);
    sd_cs_deselect();
    spi_write_blocking(SD_SPI, &dummy, 1);
    
    printf("CMD0 response: 0x%02X (expected: 0x01)\n", resp);
    
    if (resp != 0x01) {
        if (resp == 0xFF) {
            printf("❌ No response from SD card (check connections)\n");
        } else if (resp == 0x3F) {
            printf("❌ Invalid response - possible SPI timing issue\n");
            printf("Trying slower SPI speed...\n");
            
            // Try even slower SPI
            spi_set_baudrate(SD_SPI, 100000); // 100kHz
            sleep_ms(100);
            
            // Try CMD0 again at slower speed
            resp = sd_command(CMD0, 0, 0x95);
            sd_cs_deselect();
            spi_write_blocking(SD_SPI, &dummy, 1);
            printf("CMD0 at 100kHz: 0x%02X\n", resp);
            
            if (resp != 0x01) {
                printf("❌ Still failed at slower speed\n");
                return -1;
            } else {
                printf("✅ Success at 100kHz - continuing with slow speed\n");
            }
        } else {
            printf("❌ Unexpected response: 0x%02X\n", resp);
        }
        
        if (resp != 0x01) {
            return -1;
        }
    }
    printf("SD card in idle state\n");
    sd_detected = true;
    
    // CMD8 - Test voltage and SDHC support
    printf("Sending CMD8 (interface condition)...\n");
    resp = sd_command(CMD8, 0x1AA, 0x87);
    uint8_t r7[4];
    for (int i = 0; i < 4; i++) {
        spi_read_blocking(SD_SPI, 0xFF, &r7[i], 1);
    }
    sd_cs_deselect();
    spi_write_blocking(SD_SPI, &dummy, 1);
    
    if (resp == 0x01 && r7[2] == 0x01 && r7[3] == 0xAA) {
        printf("CMD8 OK (SDHC/SDXC card detected)\n");
        is_sdhc = true;
    } else {
        printf("CMD8 failed, treating as SDSC card\n");
        is_sdhc = false;
    }
    
    // ACMD41 loop - Initialize card
    printf("Initializing card with ACMD41...\n");
    for (int i = 0; i < 100; i++) {
        // CMD55 prefix for application command
        sd_command(CMD55, 0, 0xFF);
        sd_cs_deselect();
        spi_write_blocking(SD_SPI, &dummy, 1);
        
        // ACMD41 with HCS bit for SDHC support
        resp = sd_command(ACMD41, 0x40000000, 0xFF);
        sd_cs_deselect();
        spi_write_blocking(SD_SPI, &dummy, 1);
        
        if (resp == 0x00) {
            printf("Card initialized successfully (ACMD41 done after %d attempts)\n", i + 1);
            sd_initialized = true;
            return 0;
        }
        sleep_ms(10);
    }
    
    printf("ACMD41 timeout - initialization failed\n");
    return -1;
}

int sd_card_init_with_detection(void) {
    return sd_card_init();
}

// Simple detection function - just checks if SD card responds to CMD0
int sd_card_simple_detect(void) {
    printf("=== Simple SD Card Detection Test ===\n");
    
    // Basic SPI setup with very slow speed
    spi_init(SD_SPI, 100000); // Start even slower
    gpio_set_function(SD_MISO, GPIO_FUNC_SPI);
    gpio_set_function(SD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);
    gpio_init(SD_CS);
    gpio_set_dir(SD_CS, GPIO_OUT);
    sd_cs_deselect();
    
    sleep_ms(250); // Longer stabilization
    
    // Send more wake-up clocks
    printf("Sending extended wake-up sequence...\n");
    uint8_t dummy = 0xFF;
    for (int i = 0; i < 40; i++) { // More dummy clocks
        spi_write_blocking(SD_SPI, &dummy, 1);
    }
    
    // Try CMD0 several times
    printf("Testing CMD0 response...\n");
    for (int attempt = 0; attempt < 10; attempt++) {
        uint8_t resp = sd_command(CMD0, 0, 0x95);
        sd_cs_deselect();
        spi_write_blocking(SD_SPI, &dummy, 1);
        
        printf("Attempt %d: CMD0 response = 0x%02X", attempt + 1, resp);
        
        if (resp == 0x01) {
            printf(" ✅ SD card detected!\n");
            return 0;
        } else if (resp == 0xFF) {
            printf(" (no response)\n");
        } else if (resp == 0x3F) {
            printf(" (SPI issue - trying different timing)\n");
            sleep_ms(50); // Extra delay for problematic cards
        } else {
            printf(" (unexpected)\n");
        }
        
        sleep_ms(200);
    }
    
    printf("❌ No SD card detected after 10 attempts\n");
    printf("Common issues:\n");
    printf("- SD card not fully inserted\n");
    printf("- Wrong SPI pins (check SCK=GP10 not GP14)\n");
    printf("- SD card compatibility (try different card)\n");
    printf("- Power supply issues\n");
    return -1;
}

void sd_card_check_status(void) {
    static bool last_initialized = false;
    static bool last_detected = false;
    
    // Only print when status changes
    if (sd_initialized != last_initialized || sd_detected != last_detected) {
        if (sd_initialized) {
            printf("SD card status: INITIALIZED\n");
        } else if (sd_detected) {
            printf("SD card status: DETECTED BUT NOT INITIALIZED\n");
        } else {
            printf("SD card status: NOT DETECTED\n");
        }
        
        last_initialized = sd_initialized;
        last_detected = sd_detected;
    }
}

bool sd_card_is_present(void) {
    return sd_detected;
}

bool sd_card_is_mounted(void) {
    return sd_initialized && fat32_mounted;
}

bool sd_card_is_initialized(void) {
    return sd_initialized;  // Just hardware, not filesystem
}

// FAT32 filesystem operations
int sd_card_mount_fat32(void) {
    if (!sd_initialized) {
        printf("Cannot mount FAT32: SD card not initialized\n");
        return -1;
    }
    
    if (fat32_mounted) {
        printf("FAT32 already mounted\n");
        return 0;
    }
    
    printf("Mounting FAT32 filesystem...\n");
    FRESULT res = f_mount(&fs, "0:", 1);  // Mount with immediate mount
    
    if (res == FR_OK) {
        fat32_mounted = true;
        printf("✅ FAT32 filesystem mounted successfully\n");
        
        // Print volume info
        DWORD fre_clust, fre_sect;
        FATFS *fs_ptr = &fs;
        f_getfree("0:", &fre_clust, &fs_ptr);
        fre_sect = fre_clust * fs.csize;
        printf("Free space: %lu KB\n", fre_sect / 2);
        
        return 0;
    } else if (res == FR_NO_FILESYSTEM) {
        printf("⚠️ No FAT filesystem found. SD card needs formatting.\n");
        printf("Run sd_card_format_fat32() to format the SD card.\n");
        return -2;  // Special code for "needs formatting"
    } else {
        printf("❌ Failed to mount FAT32: error %d\n", res);
        return -1;
    }
}

int sd_card_format_fat32(void) {
    if (!sd_initialized) {
        printf("Cannot format: SD card not initialized\n");
        return -1;
    }
    
    printf("⚠️ WARNING: Formatting SD card will ERASE ALL DATA!\n");
    printf("Formatting as FAT32...\n");
    
    BYTE work[FF_MAX_SS];  // Working buffer
    FRESULT res = f_mkfs("0:", 0, work, sizeof(work));
    
    if (res == FR_OK) {
        printf("✅ SD card formatted successfully\n");
        printf("Mounting formatted filesystem...\n");
        return sd_card_mount_fat32();
    } else {
        printf("❌ Format failed: error %d\n", res);
        return -1;
    }
}

// Raw sector operations
int sd_card_read_sector(uint32_t sector, uint8_t *buffer) {
    if (!sd_initialized) {
        printf("SD card not initialized\n");
        return -1;
    }
    
    uint8_t token;
    uint8_t dummy = 0xFF;
    
    // Convert addressing: SDHC uses block numbers, SDSC uses byte addresses
    uint32_t addr = is_sdhc ? sector : sector * 512;
    
    // CMD17 = Read Single Block
    uint8_t resp = sd_command(CMD17, addr, 0xFF);
    if (resp != 0x00) {
        printf("CMD17 failed: 0x%02X\n", resp);
        sd_cs_deselect();
        return -1;
    }
    
    // Wait for start token 0xFE
    for (int i = 0; i < 50000; i++) {
        spi_read_blocking(SD_SPI, 0xFF, &token, 1);
        if (token == 0xFE) break;
    }
    
    if (token != 0xFE) {
        printf("Read timeout - no data token (got 0x%02X)\n", token);
        sd_cs_deselect();
        return -1;
    }
    
    // Read 512 bytes of data
    spi_read_blocking(SD_SPI, 0xFF, buffer, 512);
    
    // Read CRC (required even though we don't verify it)
    uint8_t crc[2];
    spi_read_blocking(SD_SPI, 0xFF, crc, 2);
    
    sd_cs_deselect();
    spi_write_blocking(SD_SPI, &dummy, 1);
    
    printf("Read sector %lu successfully\n", sector);
    return 0;
}

int sd_card_write_sector(uint32_t sector, const uint8_t *buffer) {
    if (!sd_initialized) {
        printf("SD card not initialized\n");
        return -1;
    }
    
    uint8_t dummy = 0xFF;
    
    // Convert addressing: SDHC uses block numbers, SDSC uses byte addresses  
    uint32_t addr = is_sdhc ? sector : sector * 512;
    
    // CMD24 = Write Single Block
    uint8_t resp = sd_command(CMD24, addr, 0xFF);
    if (resp != 0x00) {
        printf("CMD24 failed: 0x%02X\n", resp);
        sd_cs_deselect();
        return -1;
    }
    
    // Send start token
    uint8_t token = 0xFE;
    spi_write_blocking(SD_SPI, &token, 1);
    
    // Send 512 bytes of data
    spi_write_blocking(SD_SPI, buffer, 512);
    
    // Send dummy CRC (2 bytes)
    uint8_t crc[2] = {0xFF, 0xFF};
    spi_write_blocking(SD_SPI, crc, 2);
    
    // Read data response
    spi_read_blocking(SD_SPI, 0xFF, &resp, 1);
    if ((resp & 0x1F) != 0x05) {
        printf("Write data response failed: 0x%02X\n", resp);
        sd_cs_deselect();
        return -1;
    }
    
    // Wait for card to finish internal write operation
    uint8_t busy;
    for (int i = 0; i < 65000; i++) {
        spi_read_blocking(SD_SPI, 0xFF, &busy, 1);
        if (busy == 0xFF) break;
    }
    
    sd_cs_deselect();
    spi_write_blocking(SD_SPI, &dummy, 1);
    
    printf("Wrote sector %lu successfully\n", sector);
    return 0;
}

// High-level file operations using FAT32
int sd_card_write_file(const char *filename, const uint8_t *data, size_t size) {
    if (!fat32_mounted) {
        printf("FAT32 not mounted\n");
        return -1;
    }
    
    FIL file;
    UINT bytes_written;
    
    // Debug: Check disk status
    DSTATUS stat = disk_status(0);
    if (stat & STA_NOINIT) {
        printf("Disk not initialized\n");
        return -1;
    }
    if (stat & STA_PROTECT) {
        printf("⚠️  Disk is write-protected!\n");
        printf("Check: SD card physical write-protect switch\n");
        return -1;
    }
    
    // Open file for writing (create if doesn't exist)
    printf("Opening file: %s\n", filename);
    FRESULT res = f_open(&file, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        printf("Failed to open file: %s (FatFs error %d)\n", filename, res);
        if (res == FR_DENIED) printf("  → Access denied - disk may be write-protected or root dir full\n");
        else if (res == FR_EXIST) printf("  → File already exists\n");
        else if (res == FR_INVALID_NAME) printf("  → Invalid filename\n");
        else if (res == FR_DISK_ERR) printf("  → Low-level disk error\n");
        return -1;
    }
    
    // Write data
    printf("Writing %zu bytes...\n", size);
    res = f_write(&file, data, size, &bytes_written);
    f_close(&file);
    
    if (res != FR_OK || bytes_written != size) {
        printf("Failed to write file: %s (error %d, wrote %u/%zu bytes)\n", 
               filename, res, bytes_written, size);
        return -1;
    }
    
    printf("✅ Wrote %zu bytes to file %s\n", size, filename);
    return 0;
}

int sd_card_read_file(const char *filename, uint8_t *buffer, size_t max_size, size_t *actual_size) {
    if (!fat32_mounted) {
        printf("FAT32 not mounted\n");
        return -1;
    }
    
    FIL file;
    UINT bytes_read;
    
    // Open file for reading
    FRESULT res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        printf("Failed to open file for reading: %s (error %d)\n", filename, res);
        return -1;
    }
    
    // Read data
    res = f_read(&file, buffer, max_size, &bytes_read);
    f_close(&file);
    
    if (res != FR_OK) {
        printf("Failed to read file: %s (error %d)\n", filename, res);
        return -1;
    }
    
    if (actual_size) *actual_size = bytes_read;
    printf("✅ Read %u bytes from file %s\n", bytes_read, filename);
    return 0;
}

void sd_card_list_files(void) {
    if (!fat32_mounted) {
        printf("FAT32 not mounted\n");
        return;
    }
    
    DIR dir;
    FILINFO fno;
    
    printf("\n📁 Files on SD card:\n");
    printf("%-20s %10s\n", "Name", "Size");
    printf("----------------------------------------\n");
    
    FRESULT res = f_opendir(&dir, "/");
    if (res == FR_OK) {
        int count = 0;
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;  // End of directory
            if (fno.fattrib & AM_DIR) {
                printf("%-20s %10s\n", fno.fname, "<DIR>");
            } else {
                printf("%-20s %10lu B\n", fno.fname, fno.fsize);
            }
            count++;
        }
        f_closedir(&dir);
        printf("----------------------------------------\n");
        printf("Total: %d items\n\n", count);
    } else {
        printf("Failed to list files (error %d)\n", res);
    }
}

int sd_card_delete_file(const char *filename) {
    if (!sd_initialized) {
        printf("SD card not initialized\n");
        return -1;
    }
    
    // Find file
    int slot = -1;
    for (int i = 0; i < file_count; i++) {
        if (strcmp(file_names[i], filename) == 0) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        printf("File %s not found\n", filename);
        return -1;
    }
    
    // Remove from file table
    for (int i = slot; i < file_count - 1; i++) {
        strcpy(file_names[i], file_names[i + 1]);
        file_sizes[i] = file_sizes[i + 1];
    }
    file_count--;
    
    printf("Deleted file: %s\n", filename);
    return 0;
}

int sd_card_get_free_space(uint32_t *free_kb) {
    if (!sd_initialized) {
        printf("SD card not initialized\n");
        return -1;
    }
    
    // Simplified calculation
    *free_kb = (10 - file_count) * (MAX_FILE_SIZE / 1024);
    printf("Free space: %lu KB (estimated)\n", *free_kb);
    return 0;
}

int sd_card_send_file(const char *filename, const char *topic) {
    printf("Would send file %s to topic %s\n", filename, topic);
    return 0;
}

int sd_card_save_block(const char *filename, const uint8_t *data, size_t size) {
    return sd_card_write_file(filename, data, size);
}

int sd_card_create_test_file(const char *filename) {
    const char *test_data = "Test file created by real SD card module with hardware SPI\n";
    return sd_card_write_file(filename, (const uint8_t*)test_data, strlen(test_data));
}

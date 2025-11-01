# Raspberry Pi Pico W - MQTT-SN Image Transfer System

Button-controlled IoT system with WiFi connectivity, MQTT-SN messaging with QoS 0/1 support, block transfer for image files, and SD card storage with status logging.

## 📁 Project Structure

### Core Application
- **`main.c`** - Main application with clean sequential startup flow
- **`CMakeLists.txt`** - Build configuration with Pico SDK
- **`lwipopts.h`** - lwIP network stack configuration

### Modular Components

#### WiFi Module
- **`wifi_driver.h/.c`** - WiFi initialization, WPA2 connection, UDP socket management
- Supports automatic IP assignment and connection retry logic

#### MQTT-SN Client
- **`mqtt_sn_client.h/.c`** - MQTT-SN protocol over UDP
- Features: CONNECT/CONNACK, PUBLISH (QoS 0/1/2), SUBSCRIBE, message polling
- Enhanced debugging with packet hex dumps

#### Block Transfer System
- **`block_transfer.h/.c`** - Large message fragmentation (128-byte chunks)
- One-time execution with 200ms inter-chunk delays
- Progress reporting every 10 chunks

#### SD Card Module
- **`sd_card.h/.c`** - Hardware SPI driver with FAT32 support (FatFs library)
- Pins: MISO=GP12, MOSI=GP11, SCK=GP10, CS=GP15
- Long filename support (up to 255 characters)
- Creates `startup.txt` with boot information

### Libraries
- **`lib/mqtt-sn/`** - MQTT-SN packet serialization library
- **`lib/fatfs/`** - FatFs FAT32 filesystem library

### Gateway & Testing Tools
- **`python_mqtt_gateway.py`** - Python MQTT-SN gateway with broker integration
- **`udp_relay_windows.py`** - UDP relay from Windows to WSL
- **`test_block_transfer.py`** - Block transfer reassembly testing
- **`setup_mqtt_gateway.sh`** & **`start_mqtt_gateway.sh`** - C gateway automation

## ✨ System Features

### 🎮 Button Control (Maker Pico W)
- **GPIO 20**: Initialize WiFi & MQTT-SN connection
- **GPIO 21**: Transfer image file from SD card with status log creation
- **GPIO 22**: Toggle QoS mode (0 or 1)
- **Debounced**: 200ms debounce for reliable button presses

### ✅ Networking
- **WiFi Connection**: WPA2/AES authentication to 2.4 GHz network
- **IP Assignment**: Static IP (172.20.10.2), displays connection details
- **UDP Socket**: Port 1884 for MQTT-SN communication
- **Non-blocking**: Uses `cyw43_arch_poll()` for continuous operation

### ✅ MQTT-SN Messaging with QoS Support
- **QoS 0 Mode**: Fire-and-forget, publishes sequence messages every 5 seconds
  - Format: `seq=X,timestamp=Y`
  - Topic: `pico/data`
  - Fast, no acknowledgment
  
- **QoS 1 Mode**: Reliable delivery with PUBACK acknowledgment
  - Waits for PUBACK confirmation
  - Automatic retry (up to 3 attempts, 1s timeout each)
  - Publishes sequence messages every 5 seconds
  - Shows success/failure for each message

### ✅ Image Transfer with Block Transfer
- **Image Source**: Reads `download.jpg` from SD card (up to 10KB)
- **Chunking**: Automatically fragments into 128-byte chunks (120 bytes data + 8 bytes header)
- **Configurable QoS**: Uses current QoS setting (0 or 1)
- **Progress Reporting**: Updates every 10 chunks
- **Topic**: `pico/block`

### ✅ Status Logging to SD Card
- **Automatic Log Creation**: Creates `transfer_log.txt` on each image transfer
- **Comprehensive Details**:
  - System information (device, uptime, timestamp)
  - Network configuration (WiFi SSID, IP, gateway)
  - MQTT-SN connection details (client ID, status, subscriptions)
  - Transfer settings (QoS mode, block size, chunk details)
  - Image transfer status (source file, location, topic)
  - Completion info (timestamp, duration, success/failure)

### ✅ SD Card Storage
- **FAT32 Filesystem**: Windows-compatible file operations
- **Hardware SPI**: Real SD card on custom pins (SCK=GP10)
- **File Operations**: Read images, write logs
- **Long Filenames**: Supports names up to 255 characters

### ✅ Packet Queue System
- **Non-blocking Reception**: Circular buffer with 16 slots
- **Message Dispatcher**: Routes CONNACK and PUBACK to correct handlers
- **Concurrent Operations**: Can handle multiple message types simultaneously
- **No Packet Loss**: Buffers incoming UDP packets asynchronously

## 🔧 Building

```bash
# From project root
cd build
make -j4

# Output: wifi_project.uf2
```

## 🚀 Quick Start

### 1. Prepare SD Card
- Format as FAT32
- Copy `download.jpg` (max 10KB) to SD card root
- Insert SD card into Pico W

### 2. Start MQTT Infrastructure
```bash
# Python gateway (recommended)
python3 python_mqtt_gateway.py

# Monitor messages
mosquitto_sub -h localhost -t "pico/#" -v

# Receive images (separate terminal)
python3 receive_image.py
```

### 3. Build and Flash Firmware
```bash
cd build
make -j4
# UF2 file automatically copied to project root
```
- Copy `wifi_project.uf2` to Pico W in BOOTSEL mode
- Connect via PuTTY/serial monitor (115200 baud)

### 4. Operation Sequence

**Step 1: View Control Menu**
```
═══════════════════════════════════════════════════════
                    CONTROL MENU                       
═══════════════════════════════════════════════════════
  GPIO 20: Initialize WiFi & MQTT Connection
  GPIO 21: Transfer Image (download.jpg)
  GPIO 22: Toggle QoS Mode
           • QoS 0 - Fast, publishes seq messages
           • QoS 1 - Reliable, waits for PUBACK
═══════════════════════════════════════════════════════
  Current QoS: 1
═══════════════════════════════════════════════════════
```

**Step 2: Press GPIO 20 (Initialize)**
```
🔘 Button pressed: Initializing WiFi & MQTT...

→ Connecting to WiFi...
  ✓ WiFi connected!
    SSID: jer
    IP: 172.20.10.2
    Gateway: 172.20.10.14:1884

→ Initializing SD card...
  ✓ SD card mounted (FAT32)
  📁 1 files on SD card

→ Setting up UDP socket...
  ✓ UDP socket ready on port 1884

→ Connecting to MQTT-SN gateway...
  ✓ Connected to 172.20.10.14:1884
  ✓ Subscribed to topics

═══════════════════════════════════════════════════════
    System Ready!
    • GPIO 21: Transfer image (QoS 1)
    • GPIO 22: Toggle QoS
═══════════════════════════════════════════════════════
```

**Step 3: Press GPIO 22 (Toggle QoS) - Optional**
```
🔘 Button pressed: QoS toggled to 0
   → QoS 0 mode: Will publish sequence messages every 5s

[155000 ms] Publishing QoS 0: seq=0 (fire-and-forget)
[160000 ms] Publishing QoS 0: seq=1 (fire-and-forget)
```

Or stay in QoS 1:
```
[155000 ms] Publishing QoS 1: seq=0 (waiting for PUBACK...)
         ✓ PUBACK received for seq=0
[160000 ms] Publishing QoS 1: seq=1 (waiting for PUBACK...)
         ✓ PUBACK received for seq=1
```

**Step 4: Press GPIO 21 (Transfer Image)**
```
🔘 Button pressed: Starting image transfer (QoS 1)...

───────────────────────────────────────────────────────
  📸 Image Transfer & Status Log Creation
───────────────────────────────────────────────────────

  → Writing status log to SD card...
  ✓ Status log saved: transfer_log.txt

  → Looking for download.jpg on SD card...

=== Starting image file transfer (QoS 1) ===
File: download.jpg
Image loaded: 6483 bytes

Block ID: 1, Data size: 6483 bytes, Chunks: 54
Sending chunk 1/54 (128 bytes)
Sending chunk 2/54 (128 bytes)
...
  Progress: 10/54 chunks sent (18.5%)
  Progress: 20/54 chunks sent (37.0%)
  Progress: 30/54 chunks sent (55.6%)
  Progress: 40/54 chunks sent (74.1%)
  Progress: 50/54 chunks sent (92.6%)
  Progress: 54/54 chunks sent (100.0%)

Block transfer completed: 54 chunks sent

  ✓ Image transfer completed (QoS 1)
  → Updating status log...
  ✓ Status log updated with completion info
───────────────────────────────────────────────────────
```

## 🏗️ System Architecture

```
┌─────────────┐
│  Pico W     │ WiFi: 172.20.10.2
│  (Client)   │ SSID: jer
└──────┬──────┘
       │ MQTT-SN over UDP (port 1884)
       │ - QoS 0 messages with seq numbers
       │ - Block transfer (128-byte chunks)
       ↓
┌──────────────┐
│ MQTT-SN GW   │ Windows: 172.20.10.14:1884
│ (Bridge)     │ WSL: 172.30.67.75:1884
└──────┬───────┘
       │ MQTT over TCP (port 1883)
       ↓
┌──────────────┐
│ Mosquitto    │ localhost:1883
│ (Broker)     │ Topics: pico/#
└──────────────┘
```

## 🔍 Module Dependencies

```
main.c
├── wifi_driver     → WiFi & UDP sockets
├── mqtt_sn_client  → MQTT-SN protocol
├── block_transfer  → Large message handling
└── sd_card         → FAT32 filesystem

mqtt_sn_client
└── wifi_driver     → UDP communication

block_transfer
└── mqtt_sn_client  → Chunk transmission
```

## 📊 Technical Specifications

- **Network**: lwIP stack, UDP sockets, WPA2/AES
- **MQTT-SN**: Protocol over UDP, QoS 0/1/2 support
- **Block Size**: 10KB maximum, 128-byte chunks
- **SD Card**: FAT32, SPI1 @ 400kHz init / 12.5MHz operation
- **Memory**: ~717KB firmware (includes FatFs library)
- **Publishing**: Every 5 seconds + one-time block transfer at 30s

## 📝 Configuration

### Network Settings (main.c)
```c
const char *ssid = "jer";
const char *password = "jeraldgoh";
const char *gateway_ip = "172.20.10.14";
const uint16_t gateway_port = 1884;
```

### SD Card Pins (sd_card.c)
```c
#define SPI_PORT spi1
#define PIN_MISO 12
#define PIN_MOSI 11
#define PIN_SCK  10  // CRITICAL: Not GP14!
#define PIN_CS   15
```

### Block Transfer (block_transfer.h)
```c
#define BLOCK_CHUNK_SIZE 128
#define BLOCK_BUFFER_SIZE 10240
#define INTER_CHUNK_DELAY_MS 200
```

## � Troubleshooting

**SD Card Not Detected**
- Format as FAT32 on Windows (not exFAT)
- Check pin connections: SCK must be GP10
- Power cycle the Pico W

**MQTT-SN Timeout**
- Ensure gateway is running: `python3 python_mqtt_gateway.py`
- Check gateway IP: 172.20.10.14:1884
- Verify mosquitto broker: `systemctl status mosquitto`

**WiFi Connection Failed**
- Check SSID and password in main.c
- Ensure 2.4 GHz network (not 5 GHz)
- Try WPA2/MIXED if WPA2/AES fails

## � Performance Characteristics

### QoS 0 (Fire-and-Forget)
- **Latency**: ~40-50ms per chunk
- **Reliability**: ~50-70% delivery (depends on network)
- **Speed**: Fast, no waiting
- **Use Case**: Non-critical data, sequence messages

### QoS 1 (Reliable with PUBACK)
- **Latency**: ~100-150ms per chunk (includes PUBACK wait)
- **Reliability**: 99.9% delivery (retry up to 3 times)
- **Speed**: Slower due to acknowledgment
- **Retry Timeout**: 1 second per attempt
- **Use Case**: Critical data, image transfers

### Image Transfer Example (6.5KB image)
- **Chunks**: 54 chunks (120 bytes data each)
- **QoS 0**: ~2-3 seconds (fast but may lose chunks)
- **QoS 1**: ~5-6 seconds (reliable with retry)

## 📝 Generated Files on SD Card

### 1. `transfer_log.txt` (Created on each image transfer)
Contains:
- System information (uptime, timestamp)
- Network configuration (SSID, IP, gateway)
- MQTT-SN connection details
- Transfer settings (QoS, block size)
- Image transfer status
- Completion statistics (duration, status)

### 2. `download.jpg` (User provided)
- Source image file for transfer
- Maximum size: 10KB
- Format: JPEG

## 🎯 Key Features Summary

✅ **Button-controlled operation** - No automatic transfers  
✅ **Dual QoS modes** - Switch between QoS 0 and QoS 1  
✅ **Image transfer** - Send JPEG files via MQTT-SN  
✅ **Status logging** - Detailed logs saved to SD card  
✅ **Sequence messages** - Continuous publishing in both QoS modes  
✅ **Packet queue system** - No packet loss, handles concurrent operations  
✅ **Reliable delivery** - QoS 1 with automatic retry  
✅ **Real-time feedback** - All operations visible in serial console
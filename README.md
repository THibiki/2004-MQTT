# Raspberry Pi Pico W - MQTT-SN IoT System

Week 10 Demo - Complete IoT system with WiFi connectivity, MQTT-SN messaging, block transfer, and SD card storage.

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

## ✨ Week 10 Demo Features

### ✅ Networking End-to-End
- **WiFi Connection**: Connects to 2.4 GHz network with WPA2/AES authentication
- **IP Assignment**: Displays SSID, IP address (172.20.10.2), and gateway details
- **UDP Echo Test**: Measures round-trip time (RTT) to gateway
- **Non-blocking**: Uses `cyw43_arch_poll()` for continuous operation without crashes

### ✅ MQTT-SN Messaging
- **QoS 0 Publishing**: Fire-and-forget message delivery
- **Sequence Numbers**: Messages include `seq=X,timestamp=Y` format
- **Topics**: `pico/data`, `pico/chunks`, `pico/block`
- **Continuous Publishing**: Every 5 seconds with sequence counter
- **Debug Logging**: CONNECT/REGISTER/PUBLISH visible in serial output

### ✅ Block Transfer (Large Payloads)
- **10KB Messages**: Automatically fragments into 128-byte chunks
- **86 Chunks**: One-time execution at 30 seconds after boot
- **200ms Delays**: Inter-chunk spacing to prevent network flooding
- **Progress Reporting**: Updates every 10 chunks with progress bar

### ✅ SD Card Storage
- **FAT32 Filesystem**: Windows-compatible file operations
- **Hardware SPI**: Real SD card on custom pins (SCK=GP10)
- **Boot Logging**: Creates `startup.txt` with network configuration
- **Long Filenames**: Supports names up to 255 characters

## 🔧 Building

```bash
# From project root
cd build
make -j4

# Output: wifi_project.uf2
```

## 🚀 Quick Start

### 1. Start MQTT Infrastructure
```bash
# Option 1: Python gateway (recommended)
python3 python_mqtt_gateway.py

# Option 2: C gateway
bash start_mqtt_gateway.sh

# Monitor messages
mosquitto_sub -h localhost -t "pico/#" -v
```

### 2. Flash Firmware
- Copy `wifi_project.uf2` to Pico W in BOOTSEL mode
- Connect via serial monitor (115200 baud)

### 3. Expected Output
```
═══════════════════════════════════════════════════════
    Raspberry Pi Pico W - MQTT-SN System Test
═══════════════════════════════════════════════════════

→ Initializing block transfer system...
  ✓ Block transfer ready

→ Connecting to WiFi...
  ✓ WiFi connected!
    SSID: jer
    IP: 172.20.10.2
    Gateway: 172.20.10.14:1884

→ Initializing SD card...
  ✓ SD card mounted (FAT32)
  ✓ Created startup.txt
  📁 1 files on SD card

→ Testing UDP echo (RTT measurement)...
  ✓ UDP message sent, RTT: 15 ms

→ Connecting to MQTT-SN gateway...
  ✓ Connected to 172.20.10.14:1884
  ✓ Subscribed to topics

═══════════════════════════════════════════════════════
    System Ready! Starting operations...
═══════════════════════════════════════════════════════

[12808 ms] Publishing QoS 0: seq=0
[17815 ms] Publishing QoS 0: seq=1
[22820 ms] Publishing QoS 0: seq=2

───────────────────────────────────────────────────────
  Block Transfer Test (10KB data in 128-byte chunks)
───────────────────────────────────────────────────────
  Message size: 10239 bytes
  Starting transfer...
  [Progress: =========>         ] 50% (43/86 chunks)
  ✓ Block transfer completed
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

## 📚 Week 10 Success Criteria

✅ **IP acquired successfully** - Displays 172.20.10.2  
✅ **UDP messages work** - RTT measurement shown  
✅ **CONNECT/REGISTER/PUBLISH visible** - Full debug logs  
✅ **No blocking or crashes** - Non-blocking polling architecture  
✅ **QoS 0 with sequence numbers** - Format: `seq=X,timestamp=Y`  
✅ **Continuous publishing** - Every 5 seconds indefinitely
# INF2004 MQTT-SN Block Transfer Project

## Overview
The project aims to develop a lightweight MQTT-SN client on the Raspberry Pi Pico W that communicates over UDP. The Pico W will implement the MQTT-SN protocol stack, including essential message types and QoS levels, to enable reliable interaction with an MQTT-SN gateway running on a laptop. The gateway forwards all MQTT-SN traffic to a standard MQTT broker, forming a complete end-to-end communication path. The system supports both Pico-to-dashboard and Pico-to-Pico communication where multiple Pico W nodes exchange messages via the broker, demonstrating messages flowing from one Pico, through the gateway and broker, to other Picos and ultimately to a visualisation dashboard.

**Key Features:**
- MQTT-SN protocol over UDP for lightweight IoT communication
- Block transfer protocol to send files (e.g., images from SD card)
- Publisher and Subscriber Pico W implementations
- QoS levels 0, 1, and 2 support with button toggling
- Real-time web dashboard for monitoring publisher
- SD card integration for image storage/retrieval
- Python scripts for receiving and visualizing data

---

## Platform / Operating System

### Tested Platforms
- Windows 10/11 + WSL 2

**Hardware Requirements:**
- 2x Raspberry Pi Pico W (one publisher, one subscriber)
- SD cards
- WiFi network (2.4 GHz)

---

## Prerequisites

### Windows (Host Machine)
| Requirement | Version | Purpose |
|-------------|---------|---------|
| **Windows 10/11** | WSL 2 enabled | Run Linux tools in Windows |
| **Visual Studio Code** | Latest | IDE for development and flashing |
| **Pico SDK** | 1.5.0+ | Build Pico W firmware |
| **PowerShell** | 5.1+ (Admin) | Firewall and WSL configuration |
| **Git** | Latest | Clone repositories |
| **Python** | 3.7+ | Dashboard and data receiver scripts |


### Mosquitto setup in WSL
```bash
sudo apt update
sudo apt install -y build-essential cmake git mosquitto libssl-dev python3-pip
```

---

## Third-Party Software Setup

### 1. WSL 2 Configuration (Windows Only)

#### Enable Mirrored Networking
Edit the WSL config file to share the same IP between Windows and WSL:

```powershell
# Open PowerShell and edit config (create if doesn't exist)
notepad C:\Users\<your-username>\.wslconfig
```

Add the following content:
```ini
[wsl2]
networkingMode=mirrored
```

Restart WSL:
```powershell
wsl --shutdown
```

Verify networking inside WSL:
```bash
hostname -I  # Should match your Windows IP
```

---

### 2. Mosquitto MQTT Broker (WSL/Linux)

#### Installation
Already included in the prerequisites. Verify installation:
```bash
mosquitto -h
```

#### Configuration
Start and enable Mosquitto service:
```bash
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
sudo systemctl status mosquitto
```

View logs:
```bash
sudo tail -f /var/log/mosquitto/mosquitto.log
```

**Default settings:**
- TCP Port: `1883`
- Protocol: MQTT v3.1.1
- Listener: `localhost` (accessible from WSL and Windows via mirrored networking)

---

### 3. Eclipse Paho MQTT-SN Gateway (WSL/Linux)

The MQTT-SN Gateway bridges UDP (MQTT-SN) packets from Pico W to TCP (MQTT) for Mosquitto.

#### Clone and Build
```bash
cd lib
git clone https://github.com/eclipse-paho/paho.mqtt-sn.embedded-c.git
cd paho.mqtt-sn.embedded-c/MQTTSNGateway
```

#### Configure Gateway
Edit `gateway.conf`:
```conf
BrokerName=127.0.0.1        # Mosquitto broker address
GatewayPortNo=1884          # MUST match MQTTSN_GATEWAY_PORT in network_config.h
```

#### Build
```bash
chmod +x build.sh
./build.sh udp   # Build with UDP transport
```

#### Run Gateway
```bash
cd bin
./MQTT-SNGateway
```

**Expected output:**
```
MQTT-SN Gateway started
UDP transport listening on port 1884
Connected to broker at 127.0.0.1:1883
```

---

### 4. Python Environment (Host Machine)

#### Install Python Dependencies
Navigate to the project root and install packages:

**For Dashboard:**
```bash
cd dashboard
python -m venv venv
venv\Scripts\activate          # Windows
# source venv/bin/activate     # Linux/macOS
pip install -r requirements.txt
```

**For Block Receiver:**
```bash
cd received
pip install paho-mqtt
```

---

### 5. Windows Firewall Configuration (Windows Only)

**⚠️ Disable firewall temporarily during testing (remember to re-enable!):**

```powershell
# Disable firewall (PowerShell as Administrator)
Set-NetFirewallProfile -Profile Domain,Public,Private -Enabled False
```

**After testing, re-enable:**
```powershell
Set-NetFirewallProfile -Profile Domain,Public,Private -Enabled True
```

---

## Project Structure

```
2004-MQTT/
│
├── CMakeLists.txt              # Main build configuration
├── pico_sdk_import.cmake       # Pico SDK integration
├── README.md                   # Original setup guide
│
├── src/                        # Pico W firmware source code
│   ├── app/
│   │   ├── main.c              # Publisher application (sends images via SD)
│   │   └── subscriber_main.c   # Subscriber application (receives blocks)
│   │
│   ├── drivers/
│   │   ├── wifi_driver.c       # WiFi connection and reconnection
│   │   ├── udp_driver.c        # UDP socket wrapper for MQTT-SN
│   │   ├── sd_card.c           # SD card SPI driver + FatFs integration
│   │   └── block_transfer.c    # Block transfer protocol implementation
│   │
│   └── protocol/mqttsn/
│       ├── mqttsn_client.c     # MQTT-SN client (CONNECT, REGISTER, PUBLISH, SUBSCRIBE)
│       └── mqttsn_adapter.c    # Transport adapter (UDP send/receive)
│
├── include/                    # Header files
│   ├── config/
│   │   ├── network_config_base.h    # Template for WiFi/MQTT-SN settings
│   │   └── network_config.h         # Your credentials (gitignored)
│   │
│   ├── drivers/
│   │   ├── wifi_driver.h
│   │   ├── udp_driver.h
│   │   ├── sd_card.h
│   │   └── block_transfer.h
│   │
│   ├── net/
│   │   └── network_errors.h    # Error code definitions
│   │
│   └── protocol/mqttsn/
│       ├── mqttsn_client.h
│       └── mqttsn_adapter.h
│
├── lib/                        # Libraries
│   ├── paho.mqtt-sn.embedded-c/     # Eclipse Paho MQTT-SN (submodule)
│   └── fatfs/                       # FatFs filesystem library
│
├── build/                      # Build output (gitignored)
│   ├── picow_network.uf2       # Publisher firmware
│   └── picow_subscriber.uf2    # Subscriber firmware
│
├── dashboard/                  # Web dashboard (Python Flask + SocketIO)
│   ├── dashboard_server.py     # Backend server
│   ├── dashboard.html          # Frontend UI
│   ├── requirements.txt        # Python dependencies
│   └── DASHBOARD_README.md     # Dashboard setup guide
│
├── test/                       # Testing framework
│   ├── unit-tests/             # Unit tests for individual components
│   │   ├── test_config.py      # Test configuration and utilities
│   │   ├── test_ut1_udp_transmission.py      # UT1: UDP transmission tests
│   │   ├── test_ut2_timeout_handling.py      # UT2: Timeout handling tests
│   │   └── test_ut3_microsd_performance.py   # UT3: MicroSD performance tests
│   │
│   ├── intergrated-tests/      # Integration tests for system behavior
│   │   ├── test_it1_qos0_message_delivery.py              # IT1: QoS 0 message delivery
│   │   ├── test_it2_qos1_message_delivery.py              # IT2: QoS 1 with PUBACK
│   │   ├── test_it3_qos1_retry.py                         # IT3: QoS 1 retry with DUP flag
│   │   ├── test_it4_qos1_dropped_connection.py            # IT4: QoS 1 reconnect recovery
│   │   ├── test_it5_block_transfer.py                     # IT5: Block transfer reassembly
│   │   ├── test_it6_block_transfer_fragment_loss.py       # IT6: Fragment loss recovery
│   │   ├── test_it7_topic_registration.py                 # IT7: Topic registration
│   │   ├── test_it9_wifi_auto_reconnect.py                # IT9: WiFi auto-reconnect
│   │   ├── test_it10_topic_registration_after_disconnect.py # IT10: Re-registration
│   │   ├── test_it11_wifi_connection.py                   # IT11: WiFi connection on boot
│   │   ├── test_it12_sd_card_operations.py                # IT12: SD card operations
│   │   └── README.md                                       # Integration testing guide
│   │
│   └── Unit-Testing.md         # Unit testing documentation
│
│
└── received/                   # Python scripts for data collection
    └── receive_blocks.py       # Saves received images to disk
```

---

## File and Folder Descriptions

### Source Code (`src/`)

#### **`src/app/main.c`** - Publisher Application
- **Purpose:** Sends images from SD card via MQTT-SN using block transfer protocol
- **Features:**
  - Scans SD card for `.jpg` files and auto-selects the first one
  - Splits large images into chunks (header + 240 bytes per chunk)
  - Button on GP22 toggles QoS levels (0 → 1 → 2 → 0)
  - Button on GP21 triggers block transfer
  - Publishes to topics: `pico/chunks`, `pico/block`

#### **`src/app/subscriber_main.c`** - Subscriber Application
- **Purpose:** Receives MQTT-SN messages and reassembles blocks
- **Features:**
  - Subscribes to `pico/chunks` and `pico/block` topics
  - Reassembles received chunks into complete files
  - Saves reconstructed images to SD card
  - Supports QoS 0, 1, and 2 message handling

#### **`src/drivers/wifi_driver.c`**
- WiFi initialization and connection management
- Auto-reconnection on disconnection
- IP address retrieval (DHCP)
- Connection status monitoring

#### **`src/drivers/udp_driver.c`**
- UDP socket creation and binding
- Non-blocking send/receive with timeouts
- Semaphore-based packet reception signaling
- LWIP integration for network stack

#### **`src/drivers/sd_card.c`**
- SPI-based SD card communication
- FatFs filesystem integration
- File read/write operations
- Directory scanning for image files
- SD card detection and initialization

#### **`src/drivers/block_transfer.c`**
- Block transfer protocol implementation
- Chunk header format: `[BlockID(2) | PartNum(2) | TotalParts(2) | DataLen(2)]`
- Sender cache for retransmission support
- Receiver assembly buffer with duplicate detection
- Automatic reassembly and completion notification

#### **`src/protocol/mqttsn/mqttsn_client.c`**
- MQTT-SN client state machine
- CONNECT, REGISTER, PUBLISH, SUBSCRIBE packet handling
- QoS 0/1/2 support
- Topic ID management
- Message ID generation

#### **`src/protocol/mqttsn/mqttsn_adapter.c`**
- Transport abstraction layer
- Maps MQTT-SN operations to UDP driver calls
- Provides timing functions for timeouts

### Configuration (`include/config/`)

#### **`network_config_base.h`** (Template)
```c
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define MQTTSN_GATEWAY_IP "YOUR_LAPTOP_IP"
#define MQTTSN_GATEWAY_PORT 1884
```

#### **`network_config.h`** (Your Credentials - Gitignored)
Copy from `network_config_base.h` and fill in your actual values:
- Get your laptop IP: `ipconfig` (Windows) or `ip a` (Linux)
- Ensure `MQTTSN_GATEWAY_PORT` matches `gateway.conf`

### Libraries (`lib/`)

#### **`lib/paho.mqtt-sn.embedded-c/`**
- Eclipse Paho MQTT-SN implementation
- Provides packet serialization/deserialization
- Used by `mqttsn_client.c`

#### **`lib/fatfs/`**
- FatFs filesystem library for embedded systems
- Enables SD card file operations
- Used by `sd_card.c`

### Python Tools

#### **`dashboard/dashboard_server.py`**
- Flask + SocketIO web server
- Bridges MQTT (TCP) and WebSocket for browser
- Real-time message forwarding
- Block reassembly and image display
- **Ports:** 5000 (HTTP/WebSocket), 1883 (MQTT)

#### **`received/receive_blocks.py`**
- Standalone block receiver
- Saves received images to `received/` folder
- Runs independently of dashboard
- Useful for automated testing

### Build Outputs (`build/`)

#### **`picow_network.uf2`** - Publisher Firmware
Flash to Pico W that has SD card connected

#### **`picow_subscriber.uf2`** - Subscriber Firmware
Flash to Pico W that receives messages

---

## User Guide

### Publisher Pico W (Sends Images)

#### Hardware Setup
1. **Pico W with SD card adapter** (e.g., Maker Pi Pico)
2. **SD card formatted as FAT32** with at least one `.jpg` image
3. **Button connected to GP22** (QoS toggle)
4. **Button connected to GP21** (trigger block transfer)
5. **USB connection** for power and serial monitor

#### Software Configuration
1. Copy `network_config_base.h` to `network_config.h`:
   ```bash
   cp include/config/network_config_base.h include/config/network_config.h
   ```

2. Edit `network_config.h` with your credentials:
   ```c
   #define WIFI_SSID "YourWiFiName"
   #define WIFI_PASSWORD "YourPassword"
   #define MQTTSN_GATEWAY_IP "192.168.1.100"  // Your laptop IP
   #define MQTTSN_GATEWAY_PORT 1884
   ```

#### Operation
1. **Power on** - Pico W connects to WiFi
2. **SD card scanned** - First `.jpg` file auto-selected
3. **Press GP22** - Toggle QoS (0 → 1 → 2, indicated in serial output)
4. **Press GP21** - Start block transfer of selected image
5. **Monitor serial output** (115200 baud) for status

**Expected Serial Output:**
```
=== Initializing WiFi ===
[INFO] Connected to YourWiFiName
[INFO] IP: 192.168.1.150

📸 Scanning for image files...
[1] photo.jpg (51200 bytes)
✓ Auto-selected: photo.jpg

Press GP21 to send image via block transfer
Press GP22 to toggle QoS (current: 0)

[Button pressed - GP21]
=== Starting block transfer ===
Block ID: 1, Data size: 51200 bytes, Chunks: 214
Chunk 1/214 sent (240 bytes)
Chunk 2/214 sent (240 bytes)
...
✓ Block transfer complete!
```
---

## Notes
- MQTT-SN Gateway used **[Eclipse Paho MQTT-SN Embedded C](https://github.com/eclipse-paho/paho.mqtt-sn.embedded-c)**
- Mosquitto listens on TCP port 1883
# INF2004 MQTT-SN Block Transfer Project

## Overview
This project implements a **MQTT-SN (MQTT for Sensor Networks)** communication system using Raspberry Pi Pico W microcontrollers. It demonstrates bidirectional messaging, block transfer protocol for large data (images), and real-time monitoring via a web dashboard.

**Key Features:**
- ✅ MQTT-SN protocol over UDP for lightweight IoT communication
- ✅ Block transfer protocol to send large files (e.g., images from SD card)
- ✅ Publisher and Subscriber Pico W implementations
- ✅ QoS levels 0, 1, and 2 support with button toggling
- ✅ Real-time web dashboard for monitoring
- ✅ SD card integration for image storage/retrieval
- ✅ Python scripts for receiving and visualizing data

---

## Platform / Operating System

### Tested Platforms
| Platform | Status | Notes |
|----------|--------|-------|
| **Windows 10/11 + WSL 2** | ✅ Fully Supported | Primary development platform |
| **Linux (Ubuntu 20.04+)** | ✅ Supported | Native Linux environment |
| **macOS** | ⚠️ Not Tested | Should work with modifications |

**Hardware Requirements:**
- 2x Raspberry Pi Pico W (one publisher, one subscriber)
- SD card + SD card adapter (Maker Pi Pico or similar)
- USB cables for flashing and serial monitoring
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

### Linux (WSL 2 or Native)
| Tool | Purpose |
|------|---------|
| `build-essential` | GCC, G++, Make compilers |
| `cmake` | Build configuration tool |
| `git` | Version control |
| `mosquitto` | MQTT broker |
| `libssl-dev` | SSL/TLS support (optional) |
| `python3-pip` | Python package manager |

**Installation (Ubuntu/Debian):**
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
cd ~
git clone https://github.com/eclipse-paho/paho.mqtt-sn.embedded-c.git
cd paho.mqtt-sn.embedded-c/MQTTSNGateway
```

#### Configure Gateway
Edit `gateway.conf`:
```bash
nano gateway.conf
```

**Critical settings:**
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

**Alternative:** Create firewall rules for UDP port 1884 and TCP port 1883 instead of disabling entirely.

---

## Project Structure

```
2004-MQTT/
│
├── CMakeLists.txt              # Main build configuration
├── pico_sdk_import.cmake       # Pico SDK integration
├── README.md                   # Original setup guide
├── DRAFT.md                    # This comprehensive guide
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
├── lib/                        # Third-party libraries
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

### Third-Party Libraries (`lib/`)

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

3. Build the firmware:
   ```bash
   cd build
   cmake ..
   make picow_network
   ```

4. Flash `build/picow_network.uf2` to Pico W:
   - Hold BOOTSEL button while connecting USB
   - Drag `picow_network.uf2` to the Pico drive

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

### Subscriber Pico W (Receives Blocks)

#### Hardware Setup
1. **Pico W** (no SD card required, but supported)
2. **USB connection** for power and serial monitor

#### Software Configuration
1. Use the **same** `network_config.h` as the publisher

2. Build subscriber firmware:
   ```bash
   cd build
   cmake ..
   make picow_subscriber
   ```

3. Flash `build/picow_subscriber.uf2` to the second Pico W

#### Operation
1. **Power on** - Connects to WiFi and MQTT-SN Gateway
2. **Subscribes** to `pico/chunks` and `pico/block` topics
3. **Receives chunks** automatically when publisher sends
4. **Reassembles** complete image in memory
5. **Saves to SD card** (if connected)

**Expected Serial Output:**
```
=== MQTT-SN Pico W Subscriber Starting ===
[INFO] WiFi connected!
[SUBSCRIBER] Sending SUBSCRIBE to 'pico/chunks'...
[SUBSCRIBER] ✓ Subscribed (TopicID=5, QoS=2)

🆕 Receiving block 1: 214 parts
📦 10/214 chunks
📦 20/214 chunks
...
📦 214/214 chunks
✅ Block 1 complete (51200 bytes)
💾 Saved to SD: /block_1.jpg
```

---

## Compilation Instructions

### Prerequisites Check
```bash
# Verify Pico SDK is installed
echo $PICO_SDK_PATH

# If not set, install Pico SDK:
cd ~
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init
export PICO_SDK_PATH=~/pico-sdk
```

### Initial Setup
```bash
cd <project-root>

# Initialize submodules (Paho library)
git submodule update --init --recursive

# Create build directory
mkdir -p build
cd build
```

### Build Both Executables
```bash
# Configure with CMake
cmake ..

# Build publisher
make picow_network

# Build subscriber
make picow_subscriber

# Or build both at once
make -j4
```

### Build Outputs
- `build/picow_network.uf2` - Publisher firmware (311 KB approx)
- `build/picow_subscriber.uf2` - Subscriber firmware (298 KB approx)

### Clean Build (If Needed)
```bash
cd build
rm -rf *
cmake ..
make
```

---

## Running the Complete System

### Step-by-Step Startup Sequence

#### 1. Start Mosquitto Broker (WSL/Linux)
```bash
sudo systemctl start mosquitto
sudo systemctl status mosquitto  # Verify running
```

#### 2. Start MQTT-SN Gateway (WSL/Linux)
```bash
cd ~/paho.mqtt-sn.embedded-c/MQTTSNGateway/bin
./MQTT-SNGateway
```

**Leave this terminal open.**

#### 3. (Optional) Start Dashboard (Windows/Linux)
```bash
cd <project-root>/dashboard
venv\Scripts\activate  # Windows
# source venv/bin/activate  # Linux
python dashboard_server.py
```

Open browser: `http://localhost:5000`

#### 4. (Optional) Start Block Receiver (Windows/Linux)
```bash
cd <project-root>/received
python receive_blocks.py
```

#### 5. Flash and Power On Pico Ws
1. **Flash subscriber first** - `picow_subscriber.uf2`
2. **Flash publisher** - `picow_network.uf2`
3. **Connect USB for serial monitoring** (optional but recommended)

#### 6. Trigger Block Transfer
Press **GP21 button** on publisher Pico W

---

## Testing the Project

### Test 1: Basic Connectivity
**Goal:** Verify WiFi and MQTT-SN connection

1. Flash publisher Pico W
2. Monitor serial output for:
   ```
   [INFO] WiFi connected
   [MQTTSN] ✓ CONNECT accepted
   ```
3. Check gateway logs for connection

**Success Criteria:** No errors in serial output, gateway shows client connected

---

### Test 2: QoS Toggle
**Goal:** Verify QoS level switching

1. Press **GP22** on publisher
2. Observe serial output:
   ```
   [BUTTON] QoS changed: 0 → 1
   [BUTTON] QoS changed: 1 → 2
   [BUTTON] QoS changed: 2 → 0
   ```

**Success Criteria:** QoS cycles correctly

---

### Test 3: Small Block Transfer (QoS 0)
**Goal:** Test basic block transfer without retransmissions

1. Set QoS to 0 (press GP22 until "QoS=0")
2. Press **GP21** to send image
3. Monitor subscriber serial output for chunk count
4. Check dashboard for image display

**Success Criteria:** All chunks received, image reconstructed correctly

---

### Test 4: QoS 1 Transfer
**Goal:** Test at-least-once delivery with ACKs

1. Set QoS to 1
2. Send image
3. Observe PUBACK messages in serial output
4. Verify no duplicates in receiver

**Success Criteria:** Image received, ACKs exchanged

---

### Test 5: QoS 2 Transfer
**Goal:** Test exactly-once delivery with 4-way handshake

1. Set QoS to 2
2. Send image
3. Monitor serial for:
   - PUBREC (from subscriber)
   - PUBREL (from publisher)
   - PUBCOMP (from subscriber)
4. Verify no duplicates

**Success Criteria:** Complete 4-way handshake, no duplicate chunks

---

### Test 6: Dashboard Visualization
**Goal:** Verify real-time web monitoring

1. Start dashboard server
2. Open browser to `http://localhost:5000`
3. Send image from publisher
4. Observe:
   - Live chunk counter
   - Progress bar
   - Final image display

**Success Criteria:** Dashboard updates in real-time, image displays correctly

---

### Test 7: Receiver Script
**Goal:** Test standalone Python receiver

1. Run `receive_blocks.py`
2. Send image from Pico W
3. Check `received/` folder for saved `.jpg` file

**Success Criteria:** Image file saved with correct size and content

---

## Troubleshooting

### WiFi Connection Issues
**Symptom:** `[WARNING] WiFi disconnected!`

**Solutions:**
1. Verify SSID and password in `network_config.h`
2. Check 2.4 GHz band (Pico W doesn't support 5 GHz)
3. Move Pico W closer to router
4. Check WiFi router logs for MAC address blocking

---

### MQTT-SN Gateway Unreachable
**Symptom:** `[MQTTSN] CONNECT send failed`

**Solutions:**
1. Verify gateway is running: `ps aux | grep MQTT-SNGateway`
2. Check gateway IP matches `MQTTSN_GATEWAY_IP`
3. Verify port 1884 in both `gateway.conf` and `network_config.h`
4. **Firewall:** Temporarily disable Windows Firewall
5. Test connectivity: `ping <gateway-ip>` from Pico's network

---

### Chunks Missing or Corrupted
**Symptom:** `❌ Missing chunks: [12, 45, 78]`

**Solutions:**
1. Increase QoS level (try QoS 1 or 2)
2. Reduce WiFi congestion (fewer devices)
3. Check UDP packet loss with network tools
4. Ensure MQTT broker has sufficient buffer: edit `/etc/mosquitto/mosquitto.conf`:
   ```
   max_queued_messages 1000
   ```

---

### Dashboard Not Connecting
**Symptom:** Dashboard shows "Disconnected"

**Solutions:**
1. Verify Mosquitto is running: `sudo systemctl status mosquitto`
2. Check port 1883 is open: `netstat -an | grep 1883`
3. Review dashboard terminal for errors
4. Test with `mosquitto_sub -h localhost -t pico/#`

---

### Build Errors
**Symptom:** CMake or make fails

**Solutions:**
1. Initialize submodules: `git submodule update --init --recursive`
2. Clean build: `rm -rf build && mkdir build && cd build && cmake ..`
3. Verify Pico SDK path: `echo $PICO_SDK_PATH`
4. Update Pico SDK: `cd $PICO_SDK_PATH && git pull`

---

## Performance Metrics

### Typical Transfer Statistics
| Metric | QoS 0 | QoS 1 | QoS 2 |
|--------|-------|-------|-------|
| **50 KB Image** | 2.5 sec | 3.1 sec | 4.2 sec |
| **Chunks** | 214 | 214 | 214 |
| **Packet Loss** | ~1-2% | 0% | 0% |
| **Retransmissions** | None | ~5 | ~10 |

### Network Traffic
- UDP overhead: 8 bytes per chunk
- MQTT-SN header: 8 bytes per chunk
- Chunk payload: 240 bytes
- **Total per chunk:** 256 bytes

---

## Advanced Configuration

### Changing Chunk Size
Edit `include/drivers/block_transfer.h`:
```c
#define BLOCK_CHUNK_SIZE 256  // Increase for faster transfer (max ~512)
```
**Trade-off:** Larger chunks = faster transfer but higher packet loss risk

### Adjusting Timeouts
Edit `src/protocol/mqttsn/mqttsn_client.c`:
```c
#define MQTTSN_TIMEOUT_MS 5000  // Increase for slow networks
```

### Enabling Debug Logs
Uncomment debug prints in:
- `src/drivers/udp_driver.c`
- `src/protocol/mqttsn/mqttsn_client.c`
- `src/drivers/block_transfer.c`

---

## References

### Documentation
- [Pico W Datasheet](https://datasheets.raspberrypi.com/picow/pico-w-datasheet.pdf)
- [MQTT-SN Specification v1.2](https://www.oasis-open.org/committees/download.php/66091/MQTT-SN_spec_v1.2.pdf)
- [Eclipse Paho MQTT-SN](https://www.eclipse.org/paho/index.php?page=components/mqtt-sn-transparent-gateway/index.php)
- [FatFs Documentation](http://elm-chan.org/fsw/ff/00index_e.html)

### Tools
- [Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [Mosquitto](https://mosquitto.org/)
- [MQTT Explorer](http://mqtt-explorer.com/) - GUI for MQTT debugging

---

## Known Limitations

1. **Max image size:** 200 KB (BLOCK_BUFFER_SIZE limit)
2. **WiFi band:** 2.4 GHz only
3. **Simultaneous transfers:** One block at a time per Pico W
4. **SD card:** FAT32 only, max 32 GB
5. **Chunk size:** Fixed at 248 bytes (8-byte header + 240 data)

---

## Future Enhancements

- [ ] AES encryption for MQTT-SN packets
- [ ] Multi-file transfer queue
- [ ] Battery monitoring for portable deployments
- [ ] HTTPS for dashboard (TLS/SSL)
- [ ] Compression (JPEG quality adjustment)
- [ ] OTA (Over-The-Air) firmware updates

---

## License
This project is developed for educational purposes as part of INF2004 coursework.

**Third-party components:**
- Eclipse Paho MQTT-SN: [EPL/EDL License](https://www.eclipse.org/legal/epl-2.0/)
- FatFs: [BSD-style License](http://elm-chan.org/fsw/ff/doc/appnote.html#license)
- Pico SDK: [BSD 3-Clause License](https://github.com/raspberrypi/pico-sdk/blob/master/LICENSE.TXT)

---

## Support & Contact
For issues or questions:
1. Check the **Troubleshooting** section above
2. Review serial output logs (115200 baud)
3. Check MQTT-SN Gateway logs
4. Verify network connectivity with `ping` and `mosquitto_sub`

---

**Last Updated:** November 24, 2025
**Version:** 1.0.0

# MQTT-SN Client Testing Procedure for Pico W

## 📋 Prerequisites

### Hardware
- Raspberry Pi Pico W
- Micro-USB cable
- Stable WiFi network

### Software Required
1. **Raspberry Pi Pico SDK** (1.3.0 or later)
2. **CMake** (3.12 or later)
3. **Ninja** build system
4. **GCC ARM** toolchain
5. **MQTT-SN Gateway** (running on your network)
6. **MQTT Broker** (e.g., Mosquitto)

---

## 🔧 Step 1: Configure Your Settings

Edit `2004-MQTT/MQTT/mqtt_sn_client/main.c`:

```c
// Lines 8-9: Change WiFi SSID
#define WIFI_SSID "YourWiFiName"

// Lines 12-13: Change WiFi Password  
#define WIFI_PASSWORD "YourWiFiPassword"

// Lines 17-18: Change Gateway IP (your MQTT-SN gateway IP)
#define GATEWAY_IP "192.168.1.100"  // Change to your gateway IP
#define GATEWAY_PORT 1883            // Change if needed
```

---

## 🏗️ Step 2: Build the Firmware

### On Windows (PowerShell):

```powershell
cd C:\MQTT\2004-MQTT

# Clean previous build (if rebuilding)
Remove-Item -Recurse -Force .\build -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path .\build
cd .\build

# Configure CMake
cmake .. -G Ninja

# Build
ninja

# The .uf2 file will be at:
# build\MQTT\mqtt_sn_client\mqtt_sn_client.uf2
```

### Build Output Location:
The compiled `.uf2` file will be at:
```
2004-MQTT/build/MQTT/mqtt_sn_client/mqtt_sn_client.uf2
```

---

## 🚀 Step 3: Set Up MQTT-SN Gateway

You need an MQTT-SN gateway running on your network. Options:

### Option A: Use Eclipse Paho MQTT-SN Gateway
```bash
# Download from: https://github.com/eclipse/paho.mqtt-sn.embedded-c
# Run gateway with:
./MQTT-SNGateway -b <broker_ip>:1883 -p 1883
```

### Option B: Use Mosquitto with MQTT-SN Support
```bash
# Start broker
mosquitto -p 1883

# In another terminal, start MQTT-SN gateway
# (Install mosquitto_sn_gateway if available)
```

### Configure Gateway:
- **IP Address**: Should match what you set in `main.c` (line 17)
- **Port**: Default 1883 (or whatever you set in `main.c` line 18)
- **Network**: Should be on same WiFi as Pico W

---

## 💾 Step 4: Flash to Pico W

### Method 1: Manual Flash
1. **Hold the BOOTSEL button** on Pico W
2. Connect USB cable (while holding BOOTSEL)
3. Release BOOTSEL button
4. Pico W should appear as USB drive
5. Copy `mqtt_sn_client.uf2` to the USB drive
6. Pico W will automatically reboot

### Method 2: Using Picotool (if installed)
```powershell
# From PowerShell
cd C:\MQTT\2004-MQTT\build\MQTT\mqtt_sn_client

# Flash to Pico W (connected and in BOOTSEL mode)
picotool load mqtt_sn_client.uf2
picotool reset
```

---

## 📡 Step 5: Monitor and Test

### Connect Serial Monitor

The firmware outputs to USB Serial. Use a serial monitor:

**Option 1: Using PuTTY**
- Connection Type: **Serial**
- Serial Line: `COMx` (check Device Manager)
- Speed: `115200`
- Flow Control: **None**

**Option 2: Using Arduino IDE Serial Monitor**
- Tools → Port → Select COM port
- Set baud rate to 115200

**Option 3: Using PowerShell**
```powershell
# Install Windows Serial Port tool
# Then connect to COM port at 115200 baud
```

### Expected Output:

```
MQTT-SN Client for Pico W
========================
Connecting to WiFi: YourWiFiName
Connected to WiFi!
MQTT-SN client initialized
Gateway: 192.168.1.100:1883
Attempting to connect to MQTT-SN gateway...
Sent CONNECT message (X bytes)
Received UDP packet: X bytes from 192.168.1.100:1883
Parsed MQTT-SN message: type=0x05, length=X
MQTT-SN Connect callback: return_code=0
Successfully connected to MQTT-SN gateway!

Registering topic: test/topic
Sent REGISTER message for topic: test/topic (X bytes)
Received UDP packet: X bytes
MQTT-SN Register callback: topic_id=1, return_code=0
Successfully registered topic with ID: 1

Publishing message: Hello from Pico W! Message #1
Sent PUBLISH message (topic_id=1, data_len=X, X bytes)
```

---

## ✅ Step 6: Verify End-to-End Communication

### Test Publishing:
1. Messages publish every 10 seconds
2. Check your MQTT broker for messages on topic `test/topic`
3. Use MQTT Explorer or `mosquitto_sub`:

```bash
# Subscribe to MQTT topic
mosquitto_sub -h localhost -t test/topic -v
```

### Test Subscribing:
1. Pico W subscribes to `pico/commands`
2. Publish to this topic:

```bash
mosquitto_pub -h localhost -t pico/commands -m "LED_ON"
```

3. You should see in serial monitor:
```
Received PUBLISH: topic_id=X, data_len=Y
Received data: LED_ON
```

---

## 📊 Step 7: Check Latency Statistics

Every 30 seconds, the firmware prints statistics:

```
=== MQTT-SN Latency Statistics ===
Total messages: 25
Successful: 24
Timeouts: 1
Min latency: 45 ms
Max latency: 312 ms
Average latency: 89 ms
Success rate: 96.0%
===================================
```

---

## 🔍 Troubleshooting

### WiFi Connection Issues
**Symptom**: "Failed to connect to WiFi"
- Check SSID spelling (case-sensitive)
- Check password correctness
- Ensure router is on 2.4GHz (Pico W doesn't support 5GHz)
- Check WiFi security (WPA2-PSK required)

### Gateway Connection Issues
**Symptom**: "Failed to connect to MQTT-SN gateway"
- Verify gateway IP address is correct
- Check gateway is running and listening on UDP port 1883
- Ensure Pico W and gateway are on same network
- Check firewall isn't blocking UDP 1883

### No Messages Received
**Symptom**: "Timeout" in statistics
- Verify MQTT broker is running
- Check gateway is forwarding messages correctly
- Check topic names match between publish/subscribe

### Build Errors
**Symptom**: CMake or ninja build fails
- Ensure Pico SDK is installed correctly
- Verify SDK path in CMake configuration
- Check PICO_SDK_PATH environment variable

---

## 📝 Testing Checklist

- [ ] WiFi credentials configured in `main.c`
- [ ] Gateway IP address configured in `main.c`
- [ ] Project builds successfully with `ninja`
- [ ] Firmware flashed to Pico W
- [ ] Serial monitor connected at 115200 baud
- [ ] WiFi connection successful
- [ ] MQTT-SN connection successful
- [ ] Topic registration successful
- [ ] Messages publishing every 10 seconds
- [ ] Can receive MQTT messages from broker
- [ ] Can subscribe to topics
- [ ] Latency statistics displaying correctly

---

## 🎯 Advanced Testing

### Test QoS Levels
Edit `main.c` line 140 to change QoS:
```c
mqtt_sn_publish(&client, 1, (const uint8_t*)message, strlen(message), MQTT_SN_QOS_1);
```

### Test Multiple Topics
Modify to register multiple topics with different IDs.

### Test Auto-Reconnect
Temporarily disconnect gateway and observe reconnection attempts.

---

## 📖 Additional Resources

- Pico SDK Documentation: https://datasheets.raspberrypi.com/picow/connecting-to-the-internet-with-pico-w.pdf
- MQTT-SN Specification: https://www.oasis-open.org/committees/download.php/66091/MQTT-SN_spec_v1.2.pdf
- lwIP Documentation: https://www.nongnu.org/lwip/2_1_x/index.html

---

## 🆘 Support

If issues persist, check:
1. Serial monitor output for error messages
2. Gateway logs for incoming connections
3. MQTT broker logs for message processing
4. Network connectivity with `ping 192.168.1.100`

Good luck testing! 🚀


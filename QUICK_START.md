# Quick Start Guide - MQTT-SN Client on Pico W

## ⚡ 5-Minute Setup

### 1. Edit Configuration (2 minutes)

Edit `MQTT/mqtt_sn_client/main.c` and change these lines:

```c
// Line 9: Your WiFi name
#define WIFI_SSID "YourNetworkName"

// Line 13: Your WiFi password
#define WIFI_PASSWORD "YourPassword"

// Line 17: Your MQTT-SN gateway IP
#define GATEWAY_IP "192.168.1.100"
```

**Important**: Ensure your WiFi is 2.4GHz (Pico W doesn't support 5GHz)

---

### 2. Build Firmware (1 minute)

**Option A: Using the PowerShell Script**
```powershell
cd C:\MQTT\2004-MQTT
.\build_and_flash.ps1
```

**Option B: Manual Build**
```powershell
cd C:\MQTT\2004-MQTT
mkdir build
cd build
cmake .. -G Ninja
ninja
```

The `.uf2` file will be at: `build/MQTT/mqtt_sn_client/mqtt_sn_client.uf2`

---

### 3. Flash to Pico W (30 seconds)

**Before Starting:**
- Install a serial monitor tool (PuTTY, Arduino IDE Serial Monitor, or any terminal at 115200 baud)

**Flash Steps:**
1. **Hold BOOTSEL button** on Pico W
2. Connect USB cable
3. Release BOOTSEL
4. Windows: Copy `mqtt_sn_client.uf2` to the USB drive that appears
   - Or use the automated script above
5. Pico W automatically reboots

---

### 4. Set Up MQTT-SN Gateway (1 minute)

**You need a gateway running on your network:**

#### Quick Test Option (Python):
```bash
# On your computer/laptop on the same WiFi
pip install paho-mqtt-sn

# Run this gateway script
python mqttsn_gateway.py --broker_ip localhost --port 1883
```

#### Or Use Eclipse Paho Gateway:
Download from: https://github.com/eclipse/paho.mqtt-sn.embedded-c

---

### 5. Monitor & Test (30 seconds)

**Connect Serial Monitor:**
- **Port**: Check Device Manager for COM port (usually COM3, COM4, etc.)
- **Baud Rate**: 115200
- **Using PuTTY**: Connection Type = Serial, 115200, None flow control

**Expected Output:**
```
MQTT-SN Client for Pico W
========================
Connecting to WiFi: YourNetworkName
Connected to WiFi!
MQTT-SN client initialized
Gateway: 192.168.1.100:1883
Successfully connected to MQTT-SN gateway!
Successfully registered topic with ID: 1
Publishing message: Hello from Pico W! Message #1
```

**Verify Messages:**
```bash
# Subscribe to MQTT broker to see messages
mosquitto_sub -h localhost -t test/topic -v
```

---

## ✅ Verification Checklist

- [ ] WiFi credentials updated in `main.c`
- [ ] Gateway IP is correct in `main.c`
- [ ] MQTT-SN gateway is running on your network
- [ ] MQTT broker is running (Mosquitto)
- [ ] `.uf2` file flashed to Pico W
- [ ] Serial monitor showing connection messages
- [ ] Messages publishing every 10 seconds
- [ ] Latency stats printing every 30 seconds

---

## 🚨 Troubleshooting

| Problem | Solution |
|---------|----------|
| Can't connect to WiFi | Check SSID/password, ensure 2.4GHz network |
| Can't connect to gateway | Verify IP address, gateway running |
| No serial output | Check COM port, 115200 baud rate |
| Build errors | Install Pico SDK, check CMake version |
| Messages not appearing | Verify broker and gateway are running |

---

## 📊 What to Expect

**Normal Operation:**
- Connects to WiFi in ~2 seconds
- Connects to MQTT-SN gateway in ~1 second
- Registers topic successfully
- Publishes every 10 seconds
- Prints stats every 30 seconds

**Sample Stats Output:**
```
=== MQTT-SN Latency Statistics ===
Total messages: 50
Successful: 49
Timeouts: 1
Min latency: 23 ms
Max latency: 145 ms
Average latency: 67 ms
Success rate: 98.0%
===================================
```

---

## 🎓 Next Steps

1. **Test Subscribing**: Pico W subscribes to `pico/commands` - publish to it!
2. **Test QoS**: Change QoS level in publish calls
3. **Change Topics**: Modify topic names for your use case
4. **Add Sensors**: Integrate I2C/SPI sensors and publish readings

---

## 📝 Configuration Summary

File: `MQTT/mqtt_sn_client/main.c`
```c
WIFI_SSID            // Your WiFi network name
WIFI_PASSWORD        // Your WiFi password
GATEWAY_IP          // MQTT-SN gateway IP (e.g., "192.168.1.100")
GATEWAY_PORT        // Gateway UDP port (default 1883)
```

File: `MQTT/mqtt_sn_client/mqtt_sn_client.h`
- Message type definitions
- QoS levels (0, 1, 2)
- Topic ID types
- Client structure

File: `MQTT/mqtt_sn_client/lwipopts.h`
- lwIP network stack configuration
- Memory settings
- UDP/TCP options

---

## 🛠️ Prerequisites Reminder

Before building:
- [ ] Pico SDK installed (1.3.0+)
- [ ] CMake (3.12+)
- [ ] Ninja
- [ ] GCC ARM toolchain
- [ ] MQTT-SN gateway running
- [ ] MQTT broker running
- [ ] Serial monitor installed

---

## 📞 Need Help?

1. Check serial output for specific error messages
2. Verify all prerequisites are installed
3. Ensure MQTT-SN gateway is accessible from your network
4. Test network connectivity: `ping 192.168.1.100`
5. Review `TESTING_PROCEDURE.md` for detailed steps

Happy testing! 🚀


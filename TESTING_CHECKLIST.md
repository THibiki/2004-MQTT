# MQTT-SN Client Testing Checklist

## 📋 Pre-Testing Setup

### 1. Configuration Changes Needed
Edit `MQTT/mqtt_sn_client/main.c`:

```c
// Line 9: Change to your WiFi network
#define WIFI_SSID "YOUR_WIFI_NAME"

// Line 13: Change to your WiFi password
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Line 17: Change to your MQTT-SN gateway IP address
#define GATEWAY_IP "192.168.1.100"

// Line 18: Change if using non-standard port
#define GATEWAY_PORT 1883
```

### 2. Environment Setup
- [ ] Pico SDK installed (1.3.0+)
- [ ] CMake (3.12+)
- [ ] Ninja build system
- [ ] MQTT-SN Gateway running on your network
- [ ] MQTT Broker (Mosquitto) running
- [ ] Serial monitor tool installed (PuTTY, Arduino IDE, etc.)

---

## 🔨 Build Process

### 3. Build the Firmware
Choose one method:

**Method A: Automated Script**
```powershell
cd C:\MQTT\2004-MQTT
.\build_and_flash.ps1
```

**Method B: Manual Build**
```powershell
cd C:\MQTT\2004-MQTT
mkdir build -Force
cd build
cmake .. -G Ninja
ninja
```

Expected output location: `build/MQTT/mqtt_sn_client/mqtt_sn_client.uf2`

- [ ] Build completed successfully
- [ ] UF2 file exists at expected location

---

## 💾 Flashing Process

### 4. Flash to Pico W

**Steps:**
1. [ ] Hold BOOTSEL button on Pico W
2. [ ] Connect USB cable
3. [ ] Release BOOTSEL button
4. [ ] Copy `.uf2` file to USB drive (or script does it automatically)
5. [ ] Pico W reboots automatically

Flash method used: ☐ Automated script ☐ Manual copy

---

## 📡 Network Setup

### 5. Gateway Configuration
- [ ] MQTT-SN Gateway IP matches `main.c` configuration
- [ ] Gateway running and accessible
- [ ] Gateway listening on UDP port 1883 (or configured port)
- [ ] MQTT Broker running on same machine or accessible network

### 6. Network Connectivity
Test connectivity:
```powershell
ping <gateway_ip>
```

- [ ] Can ping gateway from computer
- [ ] Pico W on same WiFi network (2.4GHz)

---

## 🔍 Monitoring

### 7. Serial Monitor Setup
- [ ] Serial monitor tool opened
- [ ] Correct COM port selected
- [ ] Baud rate set to 115200
- [ ] Flow control: None
- [ ] Serial monitor showing output

---

## ✅ Runtime Checks

### 8. Connection Sequence
Watch for these events in serial output:

Timing | Event | Expected Output | Status
-------|-------|----------------|-------
0-2s | WiFi Init | "Connecting to WiFi: ..." | ☐
2-5s | WiFi Connected | "Connected to WiFi!" | ☐
5-7s | MQTT-SN Connect | "Successfully connected to MQTT-SN gateway!" | ☐
7-9s | Register Topic | "Successfully registered topic with ID: 1" | ☐
~10s | First Publish | "Publishing message: Hello from Pico W! Message #1" | ☐
~40s | First Stats | Latency statistics printed | ☐

### 9. Functional Tests

**Publishing Test:**
- [ ] Messages publish every 10 seconds
- [ ] Message content is readable
- [ ] Message counter increments

**Subscribing Test:**
- [ ] Pico W subscribes to `pico/commands` topic
- [ ] Can send message to Pico W using:
  ```bash
  mosquitto_pub -h localhost -t pico/commands -m "test"
  ```
- [ ] Message received in serial monitor

**Statistics:**
- [ ] Stats print every 30 seconds
- [ ] Shows: total, successful, timeouts, min/max/avg latency
- [ ] Success rate calculated correctly

---

## 🐛 Troubleshooting Log

| Time | Issue | Resolution |
|------|-------|------------|
|  |  |  |
|  |  |  |
|  |  |  |

---

## 📊 Performance Metrics

Record your results:

**Connection Time:**
- WiFi connection: _____ seconds
- Gateway connection: _____ seconds
- Total setup: _____ seconds

**Latency:**
- Min latency: _____ ms
- Max latency: _____ ms
- Avg latency: _____ ms

**Reliability:**
- Total messages: _____
- Successful: _____
- Timeouts: _____
- Success rate: _____ %

**Network:**
- WiFi signal strength: _____ dBm
- Gateway IP: _____
- Broker IP: _____

---

## 🎯 Advanced Tests

### 10. QoS Testing
- [ ] Test QoS 0 (fire-and-forget)
- [ ] Test QoS 1 (with acknowledgments)

### 11. Topic Management
- [ ] Register multiple topics
- [ ] Subscribe to multiple topics
- [ ] Test long topic names (64 chars)

### 12. Stress Testing
- [ ] Continuous operation for 5 minutes
- [ ] Network disconnect/reconnect handling
- [ ] High message frequency (1 per second)

---

## 📝 Test Results Summary

**Date:** _______________

**Tester:** _______________

**Pico W Firmware Version:** _______________

**Overall Status:** ☐ PASS ☐ FAIL ☐ PARTIAL

**Issues Found:**
1. _______________________________________
2. _______________________________________
3. _______________________________________

**Recommendations:**
1. _______________________________________
2. _______________________________________
3. _______________________________________

---

## 🔄 Next Steps

After successful testing:

1. [ ] Create test results commit
2. [ ] Merge changes back to main branch if approved
3. [ ] Update documentation with findings
4. [ ] Tag release version
5. [ ] Push to remote repository

---

## 🆘 Emergency Procedures

**If Pico W becomes unresponsive:**
1. Unplug USB cable
2. Hold BOOTSEL button
3. Plug in USB cable
4. Release BOOTSEL
5. Flash fresh firmware

**If Build Fails:**
1. Clean build directory: `Remove-Item -Recurse build`
2. Check CMake configuration
3. Verify Pico SDK version
4. Check for syntax errors

**If No Serial Output:**
1. Check COM port in Device Manager
2. Verify USB cable is working
3. Try different USB port
4. Check baud rate settings

---

**Checklist Version:** 1.0  
**Last Updated:** Created for testing/prepare-for-pico-test branch


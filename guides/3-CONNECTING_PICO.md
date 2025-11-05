# Getting Started Guide

Quick guide to get the MQTT-SN Gateway and Pico W client running.

## Prerequisites

- Gateway built (see `1-GATEWAY_SETUP.md` if not done)
- Pico W firmware flashed
- Windows machine with WSL (for gateway)

## Quick Start

### 1. Start the Gateway

**In WSL:**
```bash
cd paho.mqtt-sn.embedded-c/MQTTSNGateway/bin
./MQTT-SNGateway
```

**What to expect:**
- Gateway startup message
- ADVERTISE messages every ~100 seconds on multicast `225.1.1.1:1883`
- Logs appear in the terminal

**Stop gateway:** Press `Ctrl+C` or run `pkill -f MQTT-SNGateway`

### 2. WSL Multicast Issue -> (Use Bridge Scripts)

If gateway is in WSL and Pico W can't receive multicast packets, use bridge scripts:

**Option A: WSL Forwarder + Windows Bridge**
1. **In WSL** (run in separate terminal):
   ```bash
   cd bridge-scripts
   python3 wsl_multicast_forwarder.py
   ```

2. **In Windows PowerShell** (run in separate terminal):
   ```powershell
   cd bridge-scripts
   python unicast_receiver_bridge.py
   ```

**Option B: Direct Multicast Bridge (Windows Native)**
If gateway runs on Windows (not WSL):
```powershell
cd bridge-scripts
python multicast_to_unicast_bridge.py
```

### 3. Test Gateway Connectivity

**Test if gateway is sending ADVERTISE messages:**
```powershell
cd test-scripts
python check_network_connectivity.py
```

**Test unicast directly to Pico W:**
```powershell
cd test-scripts
python test_unicast_sender.py
```

### 4. Run Pico W Client

1. Flash firmware to Pico W (if not already done)
2. Open serial monitor (115200 baud)
3. Pico W will:
   - Connect to WiFi
   - Listen for gateway ADVERTISE messages on port 1883
   - Print gateway detection status

**Expected output:**
```
[INFO] WiFi initialized
[INFO] Connecting to: YourWiFiSSID
[INFO] UDP socket created on port 1883 for gateway detection
[GATEWAY] ✅ Gateway detected! ID: 1, Duration: 60 seconds
```

## Troubleshooting

**Gateway not starting?**
- Check if port 10000 is in use: `ss -uln | grep 10000`
- Kill existing processes: `pkill -9 -f MQTT-SNGateway`

**Pico W not detecting gateway?**
- Check network connectivity: `python test-scripts/check_network_connectivity.py`
- Verify multicast is working (WSL limitation - use bridge scripts)
- Check firewall allows UDP port 1883

**Need more details?**
- Gateway setup: See `1-GATEWAY_SETUP.md`
- Gateway running: See `2-GATEWAY_RUNNING.md`
- WSL multicast issues: See `test-scripts/WSL_MULTICAST_SOLUTION.md`


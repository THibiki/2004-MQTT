# MQTT-SN Pico W Project - Working Setup

## Overview
This project enables a Raspberry Pi Pico W to connect to an MQTT broker via MQTT-SN (MQTT for Sensor Networks) protocol.

## Architecture
```
Pico W (MQTT-SN) → UDP Relay (Windows) → MQTT-SN Gateway (Linux) → MQTT Broker (Mosquitto)
```

## Working Components

### 1. Pico W Firmware (`wifi_project.uf2`)
- **Location**: `wifi_driver/wifi_driver/`
- **Target**: 172.20.10.14:1884 (Windows Wi-Fi IP)
- **Features**: WiFi connection, MQTT-SN client, USB serial output
- **Topics**: Publishes to `pico/data`, subscribes to `pico/test`

### 2. UDP Relay (`udp_relay_windows.py`)
- **Runs on**: Windows (Python)
- **Function**: Forwards UDP traffic from Windows Wi-Fi interface to WSL
- **Route**: 172.20.10.14:1884 → 172.30.67.75:1884

### 3. MQTT-SN Gateway
- **Location**: `gateway/`
- **Runs on**: Linux/WSL
- **Function**: Translates MQTT-SN ↔ MQTT protocols
- **Listens**: UDP 0.0.0.0:1884
- **Connects to**: localhost:1883 (Mosquitto)

### 4. MQTT Test Tools
- **Publisher**: `paho_pub.c` - Send MQTT messages
- **Subscriber**: `paho_sub.c` - Receive MQTT messages

## Quick Start

### Build Pico Firmware
```bash
cd wifi_driver/wifi_driver
mkdir build && cd build
cmake -DPICO_SDK_PATH=/path/to/pico-sdk ..
cmake --build . -j$(nproc)
# Flash: Copy wifi_project.uf2 to RPI-RP2 drive
```

### Start Gateway (Linux/WSL)
```bash
cd gateway
mkdir -p ../build_gateway
cd ../build_gateway
cmake ../gateway
cmake --build .
./mqtt_sn_gateway &
```

### Start UDP Relay (Windows)
```cmd
python udp_relay_windows.py
```

### Test MQTT (Linux/WSL)
```bash
# Compile tools
gcc -o paho_pub paho_pub.c -lpaho-mqtt3c
gcc -o paho_sub paho_sub.c -lpaho-mqtt3c

# Subscribe to Pico messages
mosquitto_sub -h localhost -t "pico/data" -v

# Send message to Pico
mosquitto_pub -h localhost -t "pico/test" -m "Hello Pico!"
```

## Network Requirements
- iPhone hotspot "jer" (password: "jeraldgoh")
- Windows laptop connected to same hotspot
- WSL/Linux running gateway and broker

## Current Working Status
✅ Pico W connects to WiFi and publishes sensor data  
✅ MQTT-SN Gateway translates protocols  
✅ UDP Relay bridges network segments  
✅ Mosquitto broker receives messages  
✅ End-to-end communication working  
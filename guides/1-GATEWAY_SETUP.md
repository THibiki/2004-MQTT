# MQTT-SN Gateway Setup Guide

## Prerequisites

- WSL or bash terminal
- CMake and build tools installed

## Setup Steps

### 1. Clone Repository
```bash
cd "/mnt/c/Users/limxu/.a Git Clones/2004-MQTT-1"
git clone https://github.com/eclipse/paho.mqtt-sn.embedded-c.git
```

### Project Structure

After cloning, your project structure will look like:

```
2004-MQTT-1/
├── paho.mqtt-sn.embedded-c/          # Cloned Paho repository
│   ├── MQTTSNPacket/                 # MQTT-SN packet library (for client)
│   ├── MQTTSNGateway/                # Gateway source code
│   │   ├── build.sh                  # Build script
│   │   ├── gateway.conf              # Gateway configuration
│   │   └── bin/                      # Built executables (after build)
│   │       ├── MQTT-SNGateway        # Gateway executable
│   │       └── gateway.conf          # Gateway config (copied here)
│   └── MQTTSNClient/                 # C++ client (not used in this project)
├── main.c                            # Your Pico W main code
├── wifi_driver.c/h                   # WiFi driver
├── udp_driver.c/h                    # UDP driver
└── CMakeLists.txt                    # Pico W build config
```

**Key directories:**
- `paho.mqtt-sn.embedded-c/MQTTSNGateway/bin/` - Gateway executable location

### 2. Build Gateway
```bash
cd paho.mqtt-sn.embedded-c/MQTTSNGateway
sed -i 's/\r$//' build.sh  # Fix line endings if needed
bash build.sh udp
```

### 3. Verify Build
```bash
cd bin
ls -la MQTT-SNGateway
# Should show the executable
```

## Configuration

Gateway configuration is in `bin/gateway.conf`:
- **Gateway Port:** 10000 (UDP)
- **KeepAlive:** 60 seconds

## Gateway Location

```
paho.mqtt-sn.embedded-c/MQTTSNGateway/bin/
├── MQTT-SNGateway      # Main gateway executable
├── gateway.conf        # Configuration file
└── ...
```

## Troubleshooting

**Line Ending Issues:**
```bash
sed -i 's/\r$//' build.sh
```

**Permission Errors (First Run):**
Run with `sudo` only the first time:
```bash
sudo ./MQTT-SNGateway
```

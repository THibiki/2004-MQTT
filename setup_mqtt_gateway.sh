#!/bin/bash

echo "=== MQTT-SN Gateway Setup Script ==="
echo "Run this script on your WSL/Linux system (172.30.67.75)"
echo ""

# Check if running on correct system
echo "Current system info:"
hostname -I
echo ""

echo "Step 1: Installing MQTT broker..."
sudo apt update
sudo apt install -y mosquitto mosquitto-clients

echo ""
echo "Step 2: Downloading MQTT-SN Gateway..."
cd /tmp
wget https://github.com/eclipse/paho.mqtt-sn.embedded-c/archive/master.zip
unzip -o master.zip
cd paho.mqtt-sn.embedded-c-master/MQTTSNGateway

echo ""
echo "Step 3: Building the gateway..."
# Check if we need to install build tools
if ! command -v gcc &> /dev/null; then
    echo "Installing build tools..."
    sudo apt install -y build-essential
fi

# Build the gateway using the correct method
if [ -f "src/Makefile" ]; then
    cd src
    make
    cd ..
elif [ -f "Makefile" ]; then
    make
else
    echo "Building manually..."
    gcc -o MQTT-SNGateway src/*.c -lpthread
fi

# Check if binary was created
if [ ! -f "MQTT-SNGateway" ] && [ -f "src/MQTT-SNGateway" ]; then
    cp src/MQTT-SNGateway .
fi

if [ ! -f "MQTT-SNGateway" ]; then
    echo "❌ Failed to build MQTT-SNGateway"
    echo "Trying alternative build method..."
    # Try building all C files in src directory
    gcc -o MQTT-SNGateway src/linux/*.c src/MQTTSNGateway/*.c -lpthread -I./src
fi

echo ""
echo "Step 4: Creating configuration file..."
cat > gateway.conf << 'EOF'
GatewayID=1
GatewayName=LinuxGW
KeepAlive=900

# UDP Port for MQTT-SN
Port=5000

# MQTT Broker connection
BrokerName=localhost
BrokerPortNo=1883
BrokerSecurePortNo=8883

# Logging
SharedMemory=NO
EOF

echo ""
echo "Step 5: Starting MQTT broker..."
sudo systemctl start mosquitto
sudo systemctl enable mosquitto

echo ""
echo "Step 6: Testing MQTT broker..."
mosquitto_pub -h localhost -t test/topic -m "Hello MQTT" &
mosquitto_sub -h localhost -t test/topic -C 1

echo ""
echo "=== Setup Complete! ==="
echo ""
echo "To start the MQTT-SN Gateway, run:"
echo "cd /tmp/paho.mqtt-sn.embedded-c-master/MQTTSNGateway"
echo "./MQTT-SNGateway gateway.conf"
echo ""
echo "The gateway will listen on UDP port 5000 and forward to MQTT broker on port 1883"
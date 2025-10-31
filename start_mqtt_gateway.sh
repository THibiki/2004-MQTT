#!/bin/bash

echo "=== Starting MQTT-SN Gateway ==="
echo ""

# Check if gateway exists
GATEWAY_DIR="/tmp/paho.mqtt-sn.embedded-c-master/MQTTSNGateway"

if [ ! -d "$GATEWAY_DIR" ]; then
    echo "❌ MQTT-SN Gateway not found!"
    echo "Please run setup_mqtt_gateway.sh first"
    exit 1
fi

cd "$GATEWAY_DIR"

if [ ! -f "MQTT-SNGateway" ]; then
    echo "❌ MQTT-SNGateway binary not found!"
    echo "Please run setup_mqtt_gateway.sh first to build it"
    exit 1
fi

if [ ! -f "gateway.conf" ]; then
    echo "❌ gateway.conf not found!"
    echo "Please run setup_mqtt_gateway.sh first"
    exit 1
fi

echo "✅ Starting MQTT broker..."
sudo systemctl start mosquitto

echo "✅ Starting MQTT-SN Gateway on UDP port 1884..."
echo "   Forwarding to MQTT broker on localhost:1883"
echo ""
echo "Press Ctrl+C to stop"
echo "================================"

./MQTT-SNGateway gateway.conf
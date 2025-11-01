#!/bin/bash
# Start MQTT-SN Gateway and Image Receiver together

echo "================================================"
echo "🚀 Starting MQTT-SN Image Transfer System"
echo "================================================"
echo ""

# Check if mosquitto is running
if ! systemctl is-active --quiet mosquitto; then
    echo "⚠️  Mosquitto broker not running, starting it..."
    sudo systemctl start mosquitto
    sleep 1
fi

echo "✅ Mosquitto broker: RUNNING"
echo ""

# Get the directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "📡 Starting MQTT-SN Gateway..."
echo "   Gateway: 172.20.10.14:1884 (UDP)"
echo "   Broker:  localhost:1883 (TCP)"
echo ""

# Start gateway in background
python3 "$SCRIPT_DIR/python_mqtt_gateway.py" &
GATEWAY_PID=$!

# Wait for gateway to initialize
sleep 2

echo "🖼️  Starting Image Receiver..."
echo "   Listening on: pico/chunks, pico/block"
echo "   Press Ctrl+C to stop"
echo ""
echo "================================================"
echo ""

# Start image receiver (will run in foreground)
python3 "$SCRIPT_DIR/receive_image.py"

# When receiver stops (Ctrl+C), kill gateway
echo ""
echo "🛑 Stopping gateway..."
kill $GATEWAY_PID 2>/dev/null
echo "👋 Done!"

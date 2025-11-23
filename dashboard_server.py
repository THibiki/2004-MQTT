#!/usr/bin/env python3
"""
MQTT Dashboard Server - WebSocket Bridge
Connects to Mosquitto via TCP and serves dashboard via WebSocket
"""

from flask import Flask, render_template_string
from flask_socketio import SocketIO, emit
import paho.mqtt.client as mqtt
import struct
import threading
import sys

app = Flask(__name__)
app.config['SECRET_KEY'] = 'mqtt-dashboard-secret'
socketio = SocketIO(app, cors_allowed_origins="*")

# MQTT Configuration
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
mqtt_client = None
mqtt_connected = False

# Block transfer state
current_block = {
    'id': None,
    'total_parts': 0,
    'parts': {}
}

def on_mqtt_connect(client, userdata, flags, rc):
    """MQTT connection callback"""
    global mqtt_connected
    if rc == 0:
        print(f"✅ Connected to MQTT broker {MQTT_BROKER}:{MQTT_PORT}")
        mqtt_connected = True
        
        # Subscribe to topics
        client.subscribe("pico/chunks")
        client.subscribe("pico/test")
        client.subscribe("pico/block")
        print("📡 Subscribed to pico/chunks, pico/test, pico/block")
        
        # Notify all connected web clients
        socketio.emit('mqtt_status', {'connected': True})
    else:
        print(f"❌ MQTT connection failed: {rc}")
        mqtt_connected = False
        socketio.emit('mqtt_status', {'connected': False})

def on_mqtt_disconnect(client, userdata, rc):
    """MQTT disconnect callback"""
    global mqtt_connected
    mqtt_connected = False
    print("🔌 Disconnected from MQTT broker")
    socketio.emit('mqtt_status', {'connected': False})

def on_mqtt_message(client, userdata, msg):
    """MQTT message callback - forward to WebSocket clients"""
    try:
        if msg.topic == "pico/chunks":
            # Parse block transfer chunk
            if len(msg.payload) >= 8:
                # Parse header: block_id (2), part_num (2), total_parts (2), data_len (2)
                block_id, part_num, total_parts, data_len = struct.unpack('<HHHH', msg.payload[:8])
                chunk_data = msg.payload[8:8+data_len]
                
                # Send chunk to web clients
                socketio.emit('mqtt_message', {
                    'topic': msg.topic,
                    'block_id': block_id,
                    'part_num': part_num,
                    'total_parts': total_parts,
                    'data_len': data_len,
                    'data': list(chunk_data)  # Convert bytes to list for JSON
                })
        
        elif msg.topic == "pico/test":
            # Regular text message
            text = msg.payload.decode('utf-8', errors='ignore')
            socketio.emit('mqtt_message', {
                'topic': msg.topic,
                'text': text
            })
        
        elif msg.topic == "pico/block":
            # Block completion notification
            text = msg.payload.decode('utf-8', errors='ignore')
            socketio.emit('mqtt_message', {
                'topic': msg.topic,
                'text': text
            })
    
    except Exception as e:
        print(f"❌ Error processing message: {e}")

def start_mqtt_client():
    """Initialize and start MQTT client"""
    global mqtt_client
    
    mqtt_client = mqtt.Client()
    mqtt_client.on_connect = on_mqtt_connect
    mqtt_client.on_disconnect = on_mqtt_disconnect
    mqtt_client.on_message = on_mqtt_message
    
    try:
        print(f"📡 Connecting to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}...")
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_start()
    except Exception as e:
        print(f"❌ Failed to connect to MQTT broker: {e}")
        print(f"   Make sure Mosquitto is running on {MQTT_BROKER}:{MQTT_PORT}")

# WebSocket event handlers
@socketio.on('connect')
def handle_connect():
    """Client connected to WebSocket"""
    print('🌐 Web client connected')
    # Send current MQTT connection status
    emit('mqtt_status', {'connected': mqtt_connected})

@socketio.on('disconnect')
def handle_disconnect():
    """Client disconnected from WebSocket"""
    print('🌐 Web client disconnected')

# HTTP route - serve dashboard HTML
@app.route('/')
def index():
    """Serve the dashboard HTML"""
    with open('dashboard.html', 'r', encoding='utf-8') as f:
        html_content = f.read()
    return html_content

if __name__ == '__main__':
    print("="*60)
    print("📊 MQTT Dashboard Server")
    print("="*60)
    
    # Start MQTT client in background
    start_mqtt_client()
    
    # Start Flask-SocketIO server
    print(f"\n🌐 Dashboard server starting on http://localhost:5000")
    print("   Open your browser and navigate to: http://localhost:5000")
    print("\n⏳ Waiting for connections...\n")
    
    try:
        socketio.run(app, host='0.0.0.0', port=5000, debug=False)
    except KeyboardInterrupt:
        print("\n\n👋 Shutting down...")
        if mqtt_client:
            mqtt_client.loop_stop()
            mqtt_client.disconnect()

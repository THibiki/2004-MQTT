#!/usr/bin/env python3

import socket
import threading
import time
import paho.mqtt.client as mqtt

class MQTTSNGateway:
    def __init__(self, udp_port=5000, mqtt_host='localhost', mqtt_port=1883):
        self.udp_port = udp_port
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        self.udp_socket = None
        self.mqtt_client = None
        self.clients = {}  # Track connected MQTT-SN clients
        
    def start(self):
        print(f"🚀 Starting Python MQTT-SN Gateway")
        print(f"   UDP Port: {self.udp_port}")
        print(f"   MQTT Broker: {self.mqtt_host}:{self.mqtt_port}")
        print(f"   Press Ctrl+C to stop")
        print("=" * 50)
        
        # Initialize MQTT client
        self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
        self.mqtt_client.on_connect = self.on_mqtt_connect
        self.mqtt_client.on_message = self.on_mqtt_message
        
        try:
            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.mqtt_client.loop_start()
            print("✅ Connected to MQTT broker")
        except Exception as e:
            print(f"❌ Failed to connect to MQTT broker: {e}")
            return
        
        # Initialize UDP socket
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.udp_socket.bind(('0.0.0.0', self.udp_port))
        print(f"✅ Listening on UDP port {self.udp_port}")
        
        try:
            while True:
                data, addr = self.udp_socket.recvfrom(1024)
                threading.Thread(target=self.handle_mqttsn_packet, args=(data, addr)).start()
        except KeyboardInterrupt:
            print("\n🛑 Shutting down gateway...")
        finally:
            self.cleanup()
    
    def handle_mqttsn_packet(self, data, addr):
        if len(data) < 2:
            print(f"⚠️  Received packet from {addr} but too short ({len(data)} bytes)")
            return
            
        length = data[0]
        msg_type = data[1]
        
        print(f"📨 Received MQTT-SN packet from {addr}: length={length}, type={msg_type}")
        
        # Handle different MQTT-SN message types
        if msg_type == 0x04:  # CONNECT
            self.handle_connect(data, addr)
        elif msg_type == 0x0C:  # PUBLISH
            self.handle_publish(data, addr)
        elif msg_type == 0x12:  # SUBSCRIBE
            self.handle_subscribe(data, addr)
        else:
            print(f"   Unhandled message type: {msg_type}")
    
    def handle_connect(self, data, addr):
        print(f"🔗 CONNECT from {addr}")
        # Send CONNACK
        connack = bytes([3, 0x05, 0x00])  # Length=3, CONNACK, Return Code=0 (accepted)
        self.udp_socket.sendto(connack, addr)
        self.clients[addr] = {'connected': True, 'topics': set()}
        print(f"   ✅ CONNACK sent to {addr}")
    
    def handle_publish(self, data, addr):
        if addr not in self.clients:
            return
            
        # Simple PUBLISH parsing (this is simplified)
        if len(data) > 7:
            topic_id = int.from_bytes(data[5:7], 'big')
            payload = data[7:]
            
            # Map topic ID to topic name (simplified mapping)
            topic_map = {
                1: "pico/data",
                2: "pico/chunks", 
                3: "pico/block",
                4: "pico/test"
            }
            
            topic = topic_map.get(topic_id, f"topic/{topic_id}")
            
            print(f"📤 PUBLISH from {addr}: topic='{topic}', payload={len(payload)} bytes")
            
            # Forward to MQTT broker
            try:
                self.mqtt_client.publish(topic, payload)
                print(f"   ✅ Forwarded to MQTT broker")
            except Exception as e:
                print(f"   ❌ Failed to forward: {e}")
    
    def handle_subscribe(self, data, addr):
        print(f"📥 SUBSCRIBE from {addr}")
        # Send SUBACK
        suback = bytes([8, 0x13, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00])  # Simplified SUBACK
        self.udp_socket.sendto(suback, addr)
        print(f"   ✅ SUBACK sent to {addr}")
    
    def on_mqtt_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print("✅ Connected to MQTT broker successfully")
        else:
            print(f"❌ Failed to connect to MQTT broker: {rc}")
    
    def on_mqtt_message(self, client, userdata, msg):
        print(f"📨 MQTT message: {msg.topic} = {msg.payload}")
    
    def cleanup(self):
        if self.udp_socket:
            self.udp_socket.close()
        if self.mqtt_client:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()

if __name__ == "__main__":
    import sys
    import platform
    
    print("Python MQTT-SN Gateway")
    print("=" * 30)
    
    # Check if mosquitto is running (Windows/Linux compatible)
    import subprocess
    mosquitto_running = False
    
    if platform.system() == "Windows":
        # Windows: Check if mosquitto service is running
        try:
            result = subprocess.run(['sc', 'query', 'Mosquitto Broker'], 
                                  capture_output=True, text=True)
            if 'RUNNING' in result.stdout:
                mosquitto_running = True
                print("✅ Mosquitto broker service is running")
            else:
                print("⚠ Mosquitto broker service is not running")
                print("   Start it with: sc start 'Mosquitto Broker'")
                print("   Or install Mosquitto if not installed")
        except:
            print("⚠ Could not check Mosquitto status")
            print("   Note: Make sure mosquitto MQTT broker is running")
    else:
        # Linux/WSL: Use systemctl
        try:
            result = subprocess.run(['systemctl', 'is-active', 'mosquitto'], 
                                  capture_output=True, text=True)
            if result.stdout.strip() == 'active':
                mosquitto_running = True
                print("✅ Mosquitto broker is running")
            else:
                print("⚠ Mosquitto broker is not running")
                print("   Starting mosquitto MQTT broker...")
                subprocess.run(['sudo', 'systemctl', 'start', 'mosquitto'], check=False)
        except:
            print("Note: Make sure mosquitto MQTT broker is running")
    
    print("")
    gateway = MQTTSNGateway()
    gateway.start()
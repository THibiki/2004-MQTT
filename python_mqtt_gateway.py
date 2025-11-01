#!/usr/bin/env python3

import socket
import threading
import time
import paho.mqtt.client as mqtt

class MQTTSNGateway:
    def __init__(self, udp_port=1884, mqtt_host='localhost', mqtt_port=1883):
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
        self.mqtt_client = mqtt.Client()
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
            print(f"⚠️  Packet too short: {len(data)} bytes")
            return
            
        length = data[0]
        msg_type = data[1]
        
        print(f"📨 Received MQTT-SN packet from {addr}: length={length}, type=0x{msg_type:02X} ({msg_type})")
        
        # Show first 20 bytes for debugging
        hex_data = ' '.join(f'{b:02X}' for b in data[:min(20, len(data))])
        print(f"   Hex: {hex_data}")
        
        # Handle different MQTT-SN message types
        if msg_type == 0x04:  # CONNECT
            self.handle_connect(data, addr)
        elif msg_type == 0x0C:  # PUBLISH
            self.handle_publish(data, addr)
        elif msg_type == 0x12:  # SUBSCRIBE
            self.handle_subscribe(data, addr)
        elif msg_type == 0x49:  # Unknown, likely malformed - ignore silently
            pass
        else:
            print(f"   ⚠️  Unhandled message type: 0x{msg_type:02X} ({msg_type})")
    
    def handle_connect(self, data, addr):
        client_ip = addr[0]  # Track by IP only, ignore port
        print(f"🔗 CONNECT from {addr}")
        # Send CONNACK
        connack = bytes([3, 0x05, 0x00])  # Length=3, CONNACK, Return Code=0 (accepted)
        self.udp_socket.sendto(connack, addr)
        self.clients[client_ip] = {'connected': True, 'topics': set(), 'last_addr': addr}
        print(f"   ✅ CONNACK sent to {addr}")
    
    def handle_publish(self, data, addr):
        client_ip = addr[0]  # Track by IP only, ignore port
        if client_ip not in self.clients:
            print(f"   ⚠️  Client {client_ip} not connected")
            return
            
        # MQTT-SN PUBLISH format:
        # Byte 0: Length
        # Byte 1: MsgType (0x0C)
        # Byte 2: Flags
        # Byte 3+: Topic (name or ID) + MsgID (optional) + Payload
        
        if len(data) < 4:
            print(f"   ⚠️  PUBLISH packet too short: {len(data)} bytes")
            return
            
        flags = data[2]
        
        # Check topic type from flags
        topic_type = flags & 0x03  # Bottom 2 bits
        # 0x00 = Topic name, 0x01 = Pre-defined topic ID, 0x02 = Short topic name
        
        pos = 3
        
        if topic_type == 0x00:  # Topic name (string)
            # Find end of topic name (null-terminated or until payload)
            # Topic name ends when we hit non-printable ASCII or specific delimiters
            topic_end = pos
            while topic_end < len(data) and data[topic_end] >= 0x20 and data[topic_end] <= 0x7E:
                # Check if this might be start of block header (0x00 bytes)
                if topic_end < len(data) - 1 and data[topic_end:topic_end+2] == b'\x00\x00':
                    break
                topic_end += 1
            
            topic = data[pos:topic_end].decode('ascii', errors='ignore')
            pos = topic_end
            
            # Skip message ID if QoS > 0 (2 bytes)
            qos = (flags >> 5) & 0x03
            if qos > 0 and pos + 2 <= len(data):
                msg_id = int.from_bytes(data[pos:pos+2], 'big')
                pos += 2
            else:
                msg_id = 0
            
            payload = data[pos:]
        else:
            # Topic ID format (original code)
            topic_id = int.from_bytes(data[3:5], 'big')
            msg_id = int.from_bytes(data[5:7], 'big') if len(data) >= 7 else 0
            payload = data[7:]
            
            topic_map = {
                1: "pico/data",
                2: "pico/chunks", 
                3: "pico/block",
                4: "pico/test"
            }
            topic = topic_map.get(topic_id, f"topic/{topic_id}")
        
        print(f"📤 PUBLISH from {addr}: topic='{topic}', msgid={msg_id}, payload={len(payload)} bytes, QoS={qos}")
        
        # Forward to MQTT broker
        try:
            self.mqtt_client.publish(topic, payload)
            print(f"   ✅ Forwarded to MQTT broker")
            
            # Send PUBACK for QoS 1 messages
            if qos == 1 and msg_id > 0:
                # PUBACK format: Length, MsgType (0x0D), TopicID (2 bytes), MsgID (2 bytes), ReturnCode
                puback = bytes([7, 0x0D, 0x00, 0x00, (msg_id >> 8) & 0xFF, msg_id & 0xFF, 0x00])
                self.udp_socket.sendto(puback, addr)
                print(f"   ✅ PUBACK sent for msgid={msg_id}")
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
    
    print("Python MQTT-SN Gateway")
    print("=" * 30)
    
    # Check if mosquitto is running
    import subprocess
    try:
        result = subprocess.run(['systemctl', 'is-active', 'mosquitto'], 
                              capture_output=True, text=True)
        if result.stdout.strip() != 'active':
            print("Starting mosquitto MQTT broker...")
            subprocess.run(['sudo', 'systemctl', 'start', 'mosquitto'])
    except:
        print("Note: Make sure mosquitto MQTT broker is running")
    
    gateway = MQTTSNGateway()
    gateway.start()
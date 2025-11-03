#!/usr/bin/env python3

import socket
import threading
import time
import sys
import paho.mqtt.client as mqtt

# Force unbuffered output
sys.stdout = sys.stderr

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
        try:
            self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
        except:
            # Fallback for older paho-mqtt versions
            self.mqtt_client = mqtt.Client()
        
        self.mqtt_client.on_connect = self.on_mqtt_connect
        self.mqtt_client.on_message = self.on_mqtt_message
        
        try:
            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, 60)
            # Start MQTT loop in background thread
            mqtt_thread = threading.Thread(target=self.mqtt_client.loop_forever, daemon=True)
            mqtt_thread.start()
            print("✅ Connected to MQTT broker")
            # Give the client time to establish connection and subscribe
            time.sleep(1)
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
            print(f"   ⚠️  Client {client_ip} not in client list, adding now...")
            self.clients[client_ip] = {'connected': True, 'topics': set(), 'last_addr': addr}
        else:
            # Update last_addr for this client
            self.clients[client_ip]['last_addr'] = addr
            
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
            # Topic name must be followed by payload
            # The Pico sends: [Length][MsgType][Flags][Topic][Payload]
            # Topic contains only letters, numbers, and '/' character
            # Payload starts after the topic
            
            # Find where topic name ends (first non-topic character or known topic match)
            known_topics = [b'pico/data', b'pico/test', b'pico/command', b'pico/response', b'pico/chunks', b'pico/block']
            topic_bytes = None
            
            # Try to match known topics first
            for known_topic in known_topics:
                if data[pos:pos+len(known_topic)] == known_topic:
                    topic_bytes = known_topic
                    pos += len(known_topic)
                    break
            
            # If no known topic matched, find topic end (stops at non-alphanumeric or non-slash)
            if not topic_bytes:
                topic_end = pos
                while topic_end < len(data):
                    c = data[topic_end]
                    if not ((c >= ord('a') and c <= ord('z')) or 
                           (c >= ord('A') and c <= ord('Z')) or
                           (c >= ord('0') and c <= ord('9')) or
                           c == ord('/') or c == ord('_')):
                        break
                    topic_end += 1
                topic_bytes = data[pos:topic_end]
                pos = topic_end
            
            topic = topic_bytes.decode('ascii', errors='ignore')
            
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
        
        # Forward to MQTT broker with same QoS level
        try:
            self.mqtt_client.publish(topic, payload, qos=qos)
            print(f"   ✅ Forwarded to MQTT broker with QoS={qos}")
            
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
            # Subscribe only to command topics (not data topics that Pico publishes)
            result1, mid1 = client.subscribe("pico/test")
            result2, mid2 = client.subscribe("pico/command")
            print(f"✅ Subscribed to pico/test (result={result1}, mid={mid1})")
            print(f"✅ Subscribed to pico/command (result={result2}, mid={mid2})")
            print(f"   Callback set: on_message={client.on_message}")
        else:
            print(f"❌ Failed to connect to MQTT broker: {rc}")
    
    def on_mqtt_message(self, client, userdata, msg):
        print(f"\n📨 MQTT MESSAGE CALLBACK TRIGGERED!")
        print(f"   Topic: {msg.topic}")
        print(f"   Payload: {msg.payload.decode('utf-8', errors='ignore')}")
        print(f"   Debug: Known clients = {list(self.clients.keys())}")
        
        # Forward message to Pico W if topic starts with "pico/"
        if msg.topic.startswith("pico/"):
            print(f"   Topic starts with 'pico/', attempting to forward...")
            # Find the Pico client
            if len(self.clients) == 0:
                print(f"   ⚠️  No clients connected! Cannot forward.")
                return
                
            for client_ip, client_info in self.clients.items():
                print(f"   Checking client {client_ip}: connected={client_info.get('connected')}")
                if client_info.get('connected'):
                    pico_addr = client_info.get('last_addr')
                    print(f"   Found connected client at {pico_addr}")
                    if pico_addr:
                        # Build MQTT-SN PUBLISH packet
                        topic_bytes = msg.topic.encode('utf-8')
                        payload = msg.payload
                        
                        # MQTT-SN PUBLISH format: Length, MsgType (0x0C), Flags, Topic, [MsgID], Payload
                        flags = 0x00  # QoS 0, Topic name
                        packet_len = 3 + len(topic_bytes) + len(payload)
                        
                        packet = bytes([packet_len, 0x0C, flags]) + topic_bytes + payload
                        
                        # Debug: show packet details
                        hex_packet = ' '.join(f'{b:02X}' for b in packet[:min(30, len(packet))])
                        print(f"   Building packet: len={packet_len}, topic={msg.topic}, payload={len(payload)} bytes")
                        print(f"   Packet hex: {hex_packet}")
                        
                        self.udp_socket.sendto(packet, pico_addr)
                        print(f"   ✅ Forwarded {len(packet)} bytes to Pico at {pico_addr}")
                        break
    
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
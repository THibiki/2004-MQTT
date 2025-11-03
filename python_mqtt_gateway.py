#!/usr/bin/env python3
"""
MQTT-SN Gateway with Standard Protocol Support
Implements REGISTER/REGACK, PINGREQ/PINGRESP, and Topic ID mapping
"""

import socket
import threading
import time
import sys
import paho.mqtt.client as mqtt

# Force unbuffered output
sys.stdout = sys.stderr

# MQTT-SN Message Types
MQTTSN_ADVERTISE = 0x00
MQTTSN_SEARCHGW = 0x01
MQTTSN_GWINFO = 0x02
MQTTSN_CONNECT = 0x04
MQTTSN_CONNACK = 0x05
MQTTSN_WILLTOPICREQ = 0x06
MQTTSN_WILLTOPIC = 0x07
MQTTSN_WILLMSGREQ = 0x08
MQTTSN_WILLMSG = 0x09
MQTTSN_REGISTER = 0x0A
MQTTSN_REGACK = 0x0B
MQTTSN_PUBLISH = 0x0C
MQTTSN_PUBACK = 0x0D
MQTTSN_PUBCOMP = 0x0E
MQTTSN_PUBREC = 0x0F
MQTTSN_PUBREL = 0x10
MQTTSN_SUBSCRIBE = 0x12
MQTTSN_SUBACK = 0x13
MQTTSN_UNSUBSCRIBE = 0x14
MQTTSN_UNSUBACK = 0x15
MQTTSN_PINGREQ = 0x16
MQTTSN_PINGRESP = 0x17
MQTTSN_DISCONNECT = 0x18

# Flags
FLAG_QOS_0 = 0x00
FLAG_QOS_1 = 0x20
FLAG_QOS_2 = 0x40
FLAG_QOS_MASK = 0x60
FLAG_TOPIC_ID = 0x00
FLAG_TOPIC_PRE = 0x01
FLAG_TOPIC_SHORT = 0x02
FLAG_TOPIC_MASK = 0x03

class MQTTSNGateway:
    def __init__(self, udp_port=1884, mqtt_host='localhost', mqtt_port=1883):
        self.udp_port = udp_port
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        self.udp_socket = None
        self.mqtt_client = None
        self.clients = {}  # Track connected MQTT-SN clients
        self.topic_registry = {}  # topic_name -> topic_id
        self.topic_id_reverse = {}  # topic_id -> topic_name
        self.next_topic_id = 1
        self.lock = threading.Lock()
        
    def get_or_assign_topic_id(self, topic_name):
        """Get existing topic ID or assign a new one"""
        with self.lock:
            if topic_name in self.topic_registry:
                return self.topic_registry[topic_name]
            
            topic_id = self.next_topic_id
            self.next_topic_id += 1
            self.topic_registry[topic_name] = topic_id
            self.topic_id_reverse[topic_id] = topic_name
            print(f"   [REGISTRY] Assigned Topic ID {topic_id} to '{topic_name}'")
            return topic_id
    
    def get_topic_name(self, topic_id):
        """Get topic name from topic ID"""
        return self.topic_id_reverse.get(topic_id, None)
    
    def start(self):
        print(f"🚀 Starting Standard MQTT-SN Gateway (v2)")
        print(f"   UDP Port: {self.udp_port}")
        print(f"   MQTT Broker: {self.mqtt_host}:{self.mqtt_port}")
        print(f"   Protocol: MQTT-SN v1.2 with REGISTER/REGACK, PINGREQ/PINGRESP")
        print(f"   Press Ctrl+C to stop")
        print("=" * 60)
        
        # Initialize MQTT client
        try:
            self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
        except:
            self.mqtt_client = mqtt.Client()
        
        self.mqtt_client.on_connect = self.on_mqtt_connect
        self.mqtt_client.on_message = self.on_mqtt_message
        
        try:
            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, 60)
            mqtt_thread = threading.Thread(target=self.mqtt_client.loop_forever, daemon=True)
            mqtt_thread.start()
            print("✅ Connected to MQTT broker")
            time.sleep(1)
        except Exception as e:
            print(f"❌ Failed to connect to MQTT broker: {e}")
            return
        
        # Initialize UDP socket
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp_socket.bind(('0.0.0.0', self.udp_port))
        print(f"✅ Listening on UDP port {self.udp_port}")
        print("=" * 60)
        
        try:
            while True:
                data, addr = self.udp_socket.recvfrom(1024)
                threading.Thread(target=self.handle_mqttsn_packet, args=(data, addr), daemon=True).start()
        except KeyboardInterrupt:
            print("\n🛑 Shutting down gateway...")
        finally:
            self.cleanup()
    
    def handle_mqttsn_packet(self, data, addr):
        if len(data) < 2:
            return
            
        length = data[0]
        msg_type = data[1]
        
        msg_type_names = {
            MQTTSN_CONNECT: "CONNECT",
            MQTTSN_REGISTER: "REGISTER",
            MQTTSN_PUBLISH: "PUBLISH",
            MQTTSN_SUBSCRIBE: "SUBSCRIBE",
            MQTTSN_PINGREQ: "PINGREQ",
            MQTTSN_DISCONNECT: "DISCONNECT"
        }
        
        msg_name = msg_type_names.get(msg_type, f"0x{msg_type:02X}")
        print(f"\n📨 {msg_name} from {addr} ({length} bytes)")
        
        if msg_type == MQTTSN_CONNECT:
            self.handle_connect(data, addr)
        elif msg_type == MQTTSN_REGISTER:
            self.handle_register(data, addr)
        elif msg_type == MQTTSN_PUBLISH:
            self.handle_publish(data, addr)
        elif msg_type == MQTTSN_SUBSCRIBE:
            self.handle_subscribe(data, addr)
        elif msg_type == MQTTSN_PINGREQ:
            self.handle_pingreq(data, addr)
        elif msg_type == MQTTSN_DISCONNECT:
            self.handle_disconnect(data, addr)
    
    def handle_connect(self, data, addr):
        client_ip = addr[0]
        # Send CONNACK
        connack = bytes([3, MQTTSN_CONNACK, 0x00])  # Return Code=0 (accepted)
        self.udp_socket.sendto(connack, addr)
        self.clients[client_ip] = {'connected': True, 'last_addr': addr}
        print(f"   ✅ Client connected - CONNACK sent")
    
    def handle_register(self, data, addr):
        """Handle REGISTER message from client"""
        if len(data) < 6:
            return
        
        # Parse: [Length][MsgType][TopicID(2)][MsgID(2)][TopicName]
        topic_id_client = (data[2] << 8) | data[3]  # Should be 0x0000 from client
        msg_id = (data[4] << 8) | data[5]
        topic_name = data[6:].decode('utf-8', errors='ignore')
        
        # Assign topic ID
        topic_id = self.get_or_assign_topic_id(topic_name)
        
        print(f"   Topic: '{topic_name}' -> ID {topic_id}")
        
        # Send REGACK
        regack = bytes([
            7,  # Length
            MQTTSN_REGACK,
            (topic_id >> 8) & 0xFF,
            topic_id & 0xFF,
            (msg_id >> 8) & 0xFF,
            msg_id & 0xFF,
            0x00  # Return code: accepted
        ])
        self.udp_socket.sendto(regack, addr)
        print(f"   ✅ REGACK sent (Topic ID: {topic_id})")
    
    def handle_publish(self, data, addr):
        client_ip = addr[0]
        if client_ip not in self.clients:
            self.clients[client_ip] = {'connected': True, 'last_addr': addr}
        else:
            self.clients[client_ip]['last_addr'] = addr
        
        if len(data) < 7:
            return
        
        # Parse: [Length][MsgType][Flags][TopicID(2)][MsgID(2)][Payload]
        flags = data[2]
        topic_type = flags & FLAG_TOPIC_MASK
        qos = (flags & FLAG_QOS_MASK) >> 5
        
        if topic_type == FLAG_TOPIC_ID:
            # Normal topic ID
            topic_id = (data[3] << 8) | data[4]
            pos = 5
            
            # Message ID for QoS > 0
            msg_id = 0
            if qos > 0:
                msg_id = (data[pos] << 8) | data[pos + 1]
                pos += 2
            
            payload = data[pos:]
            topic_name = self.get_topic_name(topic_id)
            
            if not topic_name:
                print(f"   ⚠️  Unknown Topic ID {topic_id}")
                return
            
            print(f"   Topic ID {topic_id} -> '{topic_name}'")
            print(f"   QoS: {qos}, MsgID: {msg_id}, Payload: {len(payload)} bytes")
            
            # Forward to MQTT broker
            try:
                self.mqtt_client.publish(topic_name, payload, qos=qos)
                print(f"   ✅ Forwarded to MQTT broker")
                
                # Send PUBACK for QoS 1
                if qos == 1:
                    puback = bytes([
                        7,  # Length
                        MQTTSN_PUBACK,
                        (topic_id >> 8) & 0xFF,
                        topic_id & 0xFF,
                        (msg_id >> 8) & 0xFF,
                        msg_id & 0xFF,
                        0x00  # Return code: accepted
                    ])
                    self.udp_socket.sendto(puback, addr)
                    print(f"   ✅ PUBACK sent")
            except Exception as e:
                print(f"   ❌ Failed to forward: {e}")
    
    def handle_subscribe(self, data, addr):
        if len(data) < 5:
            return
        
        # Parse: [Length][MsgType][Flags][MsgID(2)][TopicName]
        flags = data[2]
        msg_id = (data[3] << 8) | data[4]
        topic_name = data[5:].decode('utf-8', errors='ignore')
        qos = (flags & FLAG_QOS_MASK) >> 5
        
        print(f"   Topic: '{topic_name}', QoS: {qos}")
        
        # Assign topic ID
        topic_id = self.get_or_assign_topic_id(topic_name)
        
        # Send SUBACK
        suback = bytes([
            8,  # Length
            MQTTSN_SUBACK,
            flags,  # Same flags
            (topic_id >> 8) & 0xFF,
            topic_id & 0xFF,
            (msg_id >> 8) & 0xFF,
            msg_id & 0xFF,
            0x00  # Return code: accepted
        ])
        self.udp_socket.sendto(suback, addr)
        print(f"   ✅ SUBACK sent (Topic ID: {topic_id})")
    
    def handle_pingreq(self, data, addr):
        # Send PINGRESP
        pingresp = bytes([2, MQTTSN_PINGRESP])
        self.udp_socket.sendto(pingresp, addr)
        print(f"   ✅ PINGRESP sent")
    
    def handle_disconnect(self, data, addr):
        client_ip = addr[0]
        if client_ip in self.clients:
            del self.clients[client_ip]
        print(f"   ✅ Client disconnected")
    
    def on_mqtt_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print("\n✅ MQTT broker connected")
            # Subscribe to topics that clients might subscribe to
            client.subscribe("pico/#")
            print("   Subscribed to pico/# for bidirectional communication")
    
    def on_mqtt_message(self, client, userdata, msg):
        """Handle messages from MQTT broker to forward to MQTT-SN clients"""
        print(f"\n📥 MQTT message: {msg.topic} ({len(msg.payload)} bytes)")
        
        # Get topic ID
        topic_id = self.topic_registry.get(msg.topic)
        if not topic_id:
            # Register this topic
            topic_id = self.get_or_assign_topic_id(msg.topic)
            print(f"   Auto-registered topic '{msg.topic}' with ID {topic_id}")
        
        # Forward to all connected clients
        for client_ip, client_info in list(self.clients.items()):
            if not client_info.get('connected'):
                continue
            
            addr = client_info.get('last_addr')
            if not addr:
                continue
            
            # Build PUBLISH packet with Topic ID
            flags = FLAG_TOPIC_ID | FLAG_QOS_0  # QoS 0 for simplicity
            packet = bytes([
                0,  # Length (will be set)
                MQTTSN_PUBLISH,
                flags,
                (topic_id >> 8) & 0xFF,
                topic_id & 0xFF
            ]) + msg.payload
            
            # Set length
            packet = bytes([len(packet)]) + packet[1:]
            
            try:
                self.udp_socket.sendto(packet, addr)
                print(f"   ✅ Forwarded to {addr}")
            except Exception as e:
                print(f"   ❌ Failed to forward to {addr}: {e}")
    
    def cleanup(self):
        if self.udp_socket:
            self.udp_socket.close()
        if self.mqtt_client:
            self.mqtt_client.disconnect()

if __name__ == '__main__':
    gateway = MQTTSNGateway()
    gateway.start()
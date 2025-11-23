"""
Shared configuration - Monitor MQTT broker for Pico messages
"""

import paho.mqtt.client as mqtt
import socket
import struct
import time
from typing import List, Tuple, Optional, Callable

class GatewayObserver:
    """
    Observes UDP packets FROM Pico W TO gateway
    Acts as a passive listener to verify Pico behavior
    """
    
    def __init__(self, listen_port: int = 1884):
        self.listen_port = listen_port
        self.sock = None
        self.packets_received = []
    
    def start_listening(self) -> bool:
        """Start listening for packets from Pico"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.sock.bind(('0.0.0.0', self.listen_port))
            self.sock.settimeout(1.0)
            return True
        except Exception as e:
            print(f"✗ Failed to start listening: {e}")
            return False
    
    def stop_listening(self):
        """Stop listening"""
        if self.sock:
            self.sock.close()
    
    def wait_for_packets(self, timeout_sec: int, expected_count: int = 1) -> List[dict]:
        """
        Wait for UDP packets from Pico
        Returns: List of packet info dicts
        """
        packets = []
        start_time = time.time()
        
        while (time.time() - start_time) < timeout_sec:
            try:
                data, addr = self.sock.recvfrom(1024)
                elapsed = time.time() - start_time
                
                packet_info = {
                    'data': data,
                    'addr': addr,
                    'time': elapsed,
                    'hex': data.hex(),
                    'length': len(data),
                    'timestamp': time.time()
                }
                
                packets.append(packet_info)
                
                # Send simple ACK back to Pico
                try:
                    self.sock.sendto(b'\x02\x03\x00\x00', addr)
                except:
                    pass
                
                if len(packets) >= expected_count:
                    return packets
            
            except socket.timeout:
                continue
            except Exception as e:
                print(f"Error receiving: {e}")
                break
        
        return packets

class MQTTBrokerMonitor:
    """
    Monitor MQTT broker for messages published by Pico (via gateway)
    """
    
    def __init__(self, broker_ip: str = "127.0.0.1", broker_port: int = 1883):
        self.broker_ip = broker_ip
        self.broker_port = broker_port
        self.client = None
        self.messages = []
        self.connected = False
    
    def connect(self) -> bool:
        """Connect to MQTT broker"""
        try:
            self.client = mqtt.Client()
            self.client.on_connect = self._on_connect
            self.client.on_message = self._on_message
            self.client.on_disconnect = self._on_disconnect
            
            print(f"Connecting to MQTT broker at {self.broker_ip}:{self.broker_port}...")
            self.client.connect(self.broker_ip, self.broker_port, keepalive=60)
            self.client.loop_start()
            
            # Wait for connection
            timeout = time.time() + 5
            while not self.connected and time.time() < timeout:
                time.sleep(0.1)
            
            if not self.connected:
                print("✗ Failed to connect to broker")
                return False
            
            print("✓ Connected to MQTT broker")
            return True
        
        except Exception as e:
            print(f"✗ Connection failed: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from broker"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
    
    def _on_connect(self, client, userdata, flags, rc):
        """MQTT connect callback"""
        if rc == 0:
            self.connected = True
            print("✓ MQTT connected")
            # Subscribe to all topics from Pico
            client.subscribe("pico/#", qos=1)
            client.subscribe("sensors/#", qos=1)
            client.subscribe("+/+", qos=1)
        else:
            print(f"✗ MQTT connection failed: {rc}")
    
    def _on_disconnect(self, client, userdata, rc):
        """MQTT disconnect callback"""
        self.connected = False
        if rc != 0:
            print(f"✗ Unexpected disconnect: {rc}")
    
    def _on_message(self, client, userdata, msg):
        """MQTT message callback"""
        message_info = {
            'topic': msg.topic,
            'payload': msg.payload.decode('utf-8', errors='ignore'),
            'qos': msg.qos,
            'timestamp': time.time(),
            'payload_bytes': len(msg.payload)
        }
        
        self.messages.append(message_info)
    
    def wait_for_messages(self, timeout_sec: int, expected_count: int = 1) -> List[dict]:
        """
        Wait for MQTT messages
        Returns list of messages received
        """
        start_time = time.time()
        initial_count = len(self.messages)
        
        while (time.time() - start_time) < timeout_sec:
            current_count = len(self.messages) - initial_count
            
            if current_count >= expected_count:
                return self.messages[initial_count:]
            
            time.sleep(0.1)
        
        return self.messages[initial_count:]
    
    def get_messages_by_topic(self, topic_prefix: str) -> List[dict]:
        """Get all messages for a specific topic prefix"""
        return [m for m in self.messages if m['topic'].startswith(topic_prefix)]

class PacketAnalyzer:
    """Analyze MQTT-SN packets from Pico"""
    
    MQTT_SN_TYPES = {
        0x00: 'ADVERTISE',
        0x01: 'SEARCHGW',
        0x02: 'GWINFO',
        0x04: 'CONNECT',
        0x05: 'CONNACK',
        0x0C: 'PUBLISH',
        0x0D: 'PUBACK',
        0x12: 'SUBSCRIBE',
        0x13: 'SUBACK',
        0x1E: 'PINGREQ',
        0x1F: 'PINGRESP',
    }
    
    @staticmethod
    def analyze_mqtt_message(topic: str, payload: str) -> dict:
        """Analyze MQTT message from Pico"""
        return {
            'topic': topic,
            'payload': payload,
            'payload_length': len(payload),
            'timestamp': time.time()
        }
    
    @staticmethod
    def analyze(packet_data: bytes) -> dict:
        """Analyze MQTT-SN packet structure"""
        if len(packet_data) < 2:
            return {'error': 'packet too short'}
        
        length = packet_data[0]
        msg_type = packet_data[1]
        payload = packet_data[2:]
        
        return {
            'length': length,
            'type': PacketAnalyzer.MQTT_SN_TYPES.get(msg_type, f'UNKNOWN(0x{msg_type:02x})'),
            'type_code': msg_type,
            'payload_length': len(payload),
            'hex': packet_data.hex(),
            'is_valid': len(packet_data) == length + 1
        }

class MQTTSNPacket:
    """MQTT-SN packet builder - Shared across all tests"""
    
    PROTOCOL_ID = 1
    msg_id_counter = 0
    
    @staticmethod
    def get_next_msg_id() -> int:
        """Get next message ID (increments)"""
        MQTTSNPacket.msg_id_counter += 1
        if MQTTSNPacket.msg_id_counter > 65535:
            MQTTSNPacket.msg_id_counter = 1
        return MQTTSNPacket.msg_id_counter
    
    @staticmethod
    def build_connect(client_id: str, clean_session: bool = True) -> bytes:
        """Build MQTT-SN CONNECT packet"""
        flags = 0x04 if clean_session else 0x00
        keep_alive = 60
        
        payload = struct.pack('!B', flags)
        payload += struct.pack('!B', MQTTSNPacket.PROTOCOL_ID)
        payload += struct.pack('!H', keep_alive)
        payload += client_id.encode()
        
        packet = struct.pack('!B', len(payload) + 2)
        packet += struct.pack('!B', 0x04)  # CONNECT
        packet += payload
        
        return packet
    
    @staticmethod
    def build_publish(topic_id: int, message: str, qos: int = 0, retain: bool = False) -> bytes:
        """Build MQTT-SN PUBLISH packet"""
        flags = 0x00
        
        if qos > 0:
            flags |= (qos & 0x03) << 5
        
        if retain:
            flags |= 0x10
        
        flags |= 0x00  # TopicIDType = predefined
        
        msg_id = MQTTSNPacket.get_next_msg_id()
        
        payload = struct.pack('!B', flags)
        payload += struct.pack('!H', topic_id)
        payload += struct.pack('!H', msg_id)
        payload += message.encode()
        
        packet = struct.pack('!B', len(payload) + 2)
        packet += struct.pack('!B', 0x0C)  # PUBLISH
        packet += payload
        
        return packet
    
    @staticmethod
    def build_pingreq() -> bytes:
        """Build MQTT-SN PINGREQ packet - NO payload"""
        payload = b""
        
        packet = struct.pack('!B', len(payload) + 2)
        packet += struct.pack('!B', 0x1E)  # PINGREQ
        packet += payload
        
        return packet
    
    @staticmethod
    def build_disconnect(duration: int = 0) -> bytes:
        """Build MQTT-SN DISCONNECT packet"""
        if duration > 0:
            payload = struct.pack('!H', duration)
        else:
            payload = b""
        
        packet = struct.pack('!B', len(payload) + 2)
        packet += struct.pack('!B', 0x1E)  # DISCONNECT
        packet += payload
        
        return packet
    
class MQTTSNGatewayClient:
    """Client for gateway communication"""
    
    def __init__(self, gateway_ip: str, gateway_port: int = 1884):
        self.gateway_ip = gateway_ip
        self.gateway_port = gateway_port
        self.sock = None
    
    def connect(self) -> bool:
        """Create UDP socket"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.settimeout(2.0)
            return True
        except Exception as e:
            print(f"✗ Failed to create socket: {e}")
            return False
    
    def disconnect(self):
        """Close socket"""
        if self.sock:
            self.sock.close()
    
    def send_packet(self, data: bytes) -> bool:
        """Send packet to gateway"""
        try:
            self.sock.sendto(data, (self.gateway_ip, self.gateway_port))
            return True
        except Exception as e:
            print(f"✗ Send failed: {e}")
            return False
    
    def recv_packet(self, timeout: float = 0.5) -> bytes:
        """Receive packet from gateway"""
        try:
            self.sock.settimeout(timeout)
            data, _ = self.sock.recvfrom(1024)
            return data
        except socket.timeout:
            return None
        except Exception as e:
            print(f"✗ Recv failed: {e}")
            return None

"""
Shared configuration for unit tests
Includes MQTT-SN packet handling and responses
"""

import socket
import struct
from typing import Optional, Tuple

class SimpleUDPServer:
    """Simple UDP server for listening and responding"""
    
    def __init__(self, listen_port: int):
        self.listen_port = listen_port
        self.sock = None
    
    def bind(self) -> bool:
        """Bind to port"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.sock.bind(('0.0.0.0', self.listen_port))
            self.sock.settimeout(1.0)
            return True
        except Exception as e:
            print(f"Bind error: {e}")
            return False
    
    def recv(self, timeout: float = 1.0) -> Optional[Tuple[bytes, Tuple[str, int]]]:
        """Receive UDP packet"""
        try:
            self.sock.settimeout(timeout)
            data, addr = self.sock.recvfrom(1024)
            return data, addr
        except socket.timeout:
            return None
        except Exception as e:
            print(f"Recv error: {e}")
            return None
    
    def send(self, data: bytes, addr: Tuple[str, int]) -> bool:
        """Send UDP packet to address"""
        try:
            self.sock.sendto(data, addr)
            return True
        except Exception as e:
            print(f"Send error: {e}")
            return False
    
    def close(self):
        """Close socket"""
        if self.sock:
            self.sock.close()

class MQTTSNPacket:
    """MQTT-SN packet builder and parser"""
    
    MSG_TYPES = {
        0x00: 'ADVERTISE',
        0x01: 'SEARCHGW',
        0x02: 'GWINFO',
        0x04: 'CONNECT',
        0x05: 'CONNACK',
        0x0C: 'PUBLISH',
        0x0D: 'PUBACK',
        0x12: 'SUBSCRIBE',
        0x13: 'SUBACK',
        0x1E: 'PINGREQ',
        0x1F: 'PINGRESP',
        0x20: 'DISCONNECT',
        0x18: 'REGISTER',
    }
    
    @staticmethod
    def parse_packet(data: bytes) -> dict:
        """Parse MQTT-SN packet"""
        if len(data) < 2:
            return {'error': 'packet too short', 'size': len(data)}
        
        length_byte = data[0]
        msg_type = data[1]
        
        return {
            'size': len(data),
            'length_byte': length_byte,
            'msg_type': MQTTSNPacket.MSG_TYPES.get(msg_type, f'UNKNOWN(0x{msg_type:02x})'),
            'msg_type_code': msg_type,
            'hex': data.hex(),
            'data': data
        }
    
    @staticmethod
    def create_connack(return_code: int = 0) -> bytes:
        """
        Create CONNACK packet
        return_code: 0 = accepted
        
        CONNACK format:
        - Byte 0: Length (3)
        - Byte 1: Message Type (0x05)
        - Byte 2: Return Code
        """
        return bytes([3, 0x05, return_code])
    
    @staticmethod
    def create_pingresp() -> bytes:
        """
        Create PINGRESP packet
        
        PINGRESP format:
        - Byte 0: Length (2)
        - Byte 1: Message Type (0x1F)
        """
        return bytes([2, 0x1F])
    
    @staticmethod
    def create_puback(topic_id: int = 0, packet_id: int = 0, return_code: int = 0) -> bytes:
        """
        Create PUBACK packet
        
        PUBACK format:
        - Byte 0: Length (7)
        - Byte 1: Message Type (0x0D)
        - Byte 2-3: Topic ID (big-endian)
        - Byte 4-5: Packet ID (big-endian)
        - Byte 6: Return Code
        """
        return bytes([
            7, 0x0D,
            (topic_id >> 8) & 0xFF, topic_id & 0xFF,
            (packet_id >> 8) & 0xFF, packet_id & 0xFF,
            return_code
        ])

class PacketValidator:
    """Validate packet format and content"""
    
    @staticmethod
    def is_valid_mqtt_sn(data: bytes) -> bool:
        """Check if data looks like MQTT-SN packet"""
        if len(data) < 2:
            return False
        
        # Just check if it's not empty and has reasonable length
        return len(data) > 0 and len(data) <= 1024
    
    @staticmethod
    def analyze_packet(data: bytes) -> dict:
        """Analyze packet structure"""
        return MQTTSNPacket.parse_packet(data)
#!/usr/bin/env python3
"""
Simple UDP MQTT-SN Gateway (without MQTT broker dependency for testing)
This listens for UDP packets from your Pico and displays them
"""

import socket
import threading
import time

class SimpleUDPGateway:
    def __init__(self, udp_port=5000):
        self.udp_port = udp_port
        self.udp_socket = None
        self.clients = {}  # Track connected clients
        
    def start(self):
        print(f"🚀 Starting Simple UDP MQTT-SN Gateway (TEST MODE - No MQTT)")
        print(f"   UDP Port: {self.udp_port}")
        print(f"   Press Ctrl+C to stop")
        print("=" * 50)
        
        # Initialize UDP socket
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        try:
            self.udp_socket.bind(('0.0.0.0', self.udp_port))
            print(f"✅ Listening on UDP port {self.udp_port}")
            print(f"   Waiting for Pico W to connect...")
            print("=" * 50)
        except Exception as e:
            print(f"❌ Failed to bind to port {self.udp_port}: {e}")
            return
        
        try:
            while True:
                data, addr = self.udp_socket.recvfrom(1024)
                threading.Thread(target=self.handle_packet, args=(data, addr)).start()
        except KeyboardInterrupt:
            print("\n\n🛑 Shutting down gateway...")
        finally:
            self.cleanup()
    
    def handle_packet(self, data, addr):
        if len(data) < 2:
            return
            
        length = data[0]
        msg_type = data[1]
        
        msg_type_name = {
            0x04: "CONNECT",
            0x05: "CONNACK",
            0x0C: "PUBLISH",
            0x0D: "PUBACK",
            0x12: "SUBSCRIBE",
            0x13: "SUBACK",
            0x14: "UNSUBSCRIBE",
            0x15: "UNSUBACK",
        }.get(msg_type, f"UNKNOWN({msg_type:02X})")
        
        payload = data[2:] if len(data) > 2 else b''
        
        print(f"📨 {addr[0]}:{addr[1]} → {msg_type_name}")
        print(f"   Length: {length}, Type: 0x{msg_type:02X}, Payload: {len(payload)} bytes")
        
        if len(data) > 2:
            # Show hex dump of payload
            hex_str = ' '.join(f'{b:02X}' for b in payload[:16])
            if len(payload) > 16:
                hex_str += "..."
            print(f"   Hex: {hex_str}")
        
        # Send acknowledgments for certain message types
        if msg_type == 0x04:  # CONNECT
            self.send_connack(addr)
            self.clients[addr] = {'connected': True}
        elif msg_type == 0x0C and addr in self.clients:  # PUBLISH
            self.send_puback(addr, data[3:5] if len(data) > 4 else b'\x00\x00')
        elif msg_type == 0x12 and addr in self.clients:  # SUBSCRIBE
            self.send_suback(addr)
        
        print()
    
    def send_connack(self, addr):
        """Send CONNACK message"""
        connack = bytes([3, 0x05, 0x00])  # Length=3, CONNACK, Return Code=0
        self.udp_socket.sendto(connack, addr)
        print(f"   → Sent CONNACK to {addr}")
    
    def send_puback(self, addr, msg_id):
        """Send PUBACK message"""
        puback = bytes([4, 0x0D]) + msg_id  # Length=4, PUBACK, Message ID
        self.udp_socket.sendto(puback, addr)
        print(f"   → Sent PUBACK to {addr}")
    
    def send_suback(self, addr):
        """Send SUBACK message"""
        suback = bytes([8, 0x13, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00])
        self.udp_socket.sendto(suback, addr)
        print(f"   → Sent SUBACK to {addr}")
    
    def cleanup(self):
        if self.udp_socket:
            self.udp_socket.close()
            print("✅ Gateway stopped")

if __name__ == "__main__":
    print("=" * 50)
    print("Simple UDP MQTT-SN Gateway for Testing")
    print("=" * 50)
    print("\nThis gateway:")
    print("  • Listens for UDP packets on port 5000")
    print("  • Responds to MQTT-SN CONNECT/PUBLISH/SUBSCRIBE")
    print("  • Does NOT require mosquitto MQTT broker")
    print("  • Perfect for testing Pico W MQTT-SN client")
    print("\nMake sure your Pico W connects to:")
    print("  • IP: 172.20.10.1 (or your PC's IP)")
    print("  • Port: 5000")
    print()
    
    gateway = SimpleUDPGateway()
    gateway.start()

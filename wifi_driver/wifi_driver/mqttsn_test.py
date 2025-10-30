#!/usr/bin/env python3
import socket
import struct
import time

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 1884))
sock.settimeout(1.0)  # Add 1 second timeout
print(f"MQTT-SN Test Server listening on port 1884")
print(f"Waiting for packets from Pico W...")
print(f"Press Ctrl+C to stop\n")

while True:
    try:
        data, addr = sock.recvfrom(1024)
        print(f"\n[{addr[0]}:{addr[1]}] Received {len(data)} bytes:")
        print(f"  Hex: {' '.join(f'{b:02X}' for b in data)}")
        
        # Try to parse MQTT-SN packet type
        if len(data) >= 2:
            length = data[0]
            msg_type = data[1]
            type_names = {
                0x04: "CONNECT",
                0x05: "CONNACK",
                0x0C: "PUBLISH",
                0x0D: "PUBACK",
                0x12: "SUBSCRIBE",
                0x13: "SUBACK",
                0x16: "PINGREQ",
                0x17: "PINGRESP",
                0x18: "DISCONNECT"
            }
            type_name = type_names.get(msg_type, f"UNKNOWN(0x{msg_type:02X})")
            print(f"  Type: {type_name}")
            
            # Send CONNACK if it's a CONNECT
            if msg_type == 0x04:
                connack = bytes([0x03, 0x05, 0x00])
                sock.sendto(connack, addr)
                print(f"  -> Sent CONNACK")
                
    except socket.timeout:
        # Timeout is normal, just continue
        continue
    except KeyboardInterrupt:
        print("\n\nShutting down...")
        break
    except Exception as e:
        print(f"Error: {e}")

sock.close()
print("Server stopped.")

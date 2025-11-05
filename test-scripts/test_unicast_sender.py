#!/usr/bin/env python3
"""
Simple unicast sender to test if Pico W can receive UDP packets
Replace PICO_IP with your Pico W's IP address (172.20.10.5)
"""
import socket
import time

PICO_IP = "172.20.10.5"  
PICO_PORT = 1883

# ADVERTISE message: 05 00 01 00 3C
advertise_packet = bytes([0x05, 0x00, 0x01, 0x00, 0x3C])

print(f"Sending ADVERTISE packets to {PICO_IP}:{PICO_PORT}")
print(f"Packet: {' '.join([f'{b:02X}' for b in advertise_packet])}")
print("Press Ctrl+C to stop\n")

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

try:
    while True:
        sock.sendto(advertise_packet, (PICO_IP, PICO_PORT))
        print(f"[{time.strftime('%H:%M:%S')}] Sent ADVERTISE packet to {PICO_IP}:{PICO_PORT}")
        time.sleep(2)
except KeyboardInterrupt:
    print("\nStopped")
finally:
    sock.close()


#!/usr/bin/env python3
"""
Unicast Receiver Bridge for MQTT-SN Gateway
Receives unicast packets from WSL (or any source) and forwards them to Pico W
This is used when gateway is in WSL and sends multicast - WSL forwarder converts
multicast to unicast and sends to this script.
"""
import socket
import sys
import time

# Listen configuration
LISTEN_IP = "0.0.0.0"  # Listen on all interfaces
LISTEN_PORT = 1883

# Pico W configuration
PICO_IP = "172.20.10.5"
PICO_PORT = 1883

def main():
    # Create socket for receiving unicast from WSL/other sources
    recv_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    recv_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        recv_sock.bind((LISTEN_IP, LISTEN_PORT))
        print(f"[INFO] Listening on {LISTEN_IP}:{LISTEN_PORT}")
    except OSError as e:
        print(f"[ERROR] Failed to bind to port {LISTEN_PORT}: {e}")
        return
    
    # Create socket for sending unicast to Pico W
    send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    print("=" * 60)
    print("MQTT-SN Unicast Receiver Bridge")
    print("=" * 60)
    print(f"Listening for unicast: {LISTEN_IP}:{LISTEN_PORT}")
    print(f"Forwarding to Pico W: {PICO_IP}:{PICO_PORT}")
    print("=" * 60)
    print("Waiting for packets...\n")
    print("[INFO] This bridge receives unicast packets (from WSL forwarder)")
    print("[INFO] and forwards them to Pico W\n")
    
    packet_count = 0
    last_packet_time = None
    last_status_time = time.time()
    status_interval = 10.0
    
    try:
        while True:
            try:
                # Receive unicast packet
                recv_sock.settimeout(1.0)
                data, addr = recv_sock.recvfrom(1024)
                packet_count += 1
                
                current_time = time.time()
                
                time_since_last = None
                if last_packet_time:
                    time_since_last = current_time - last_packet_time
                
                print(f"[{packet_count}] Received {len(data)} bytes from {addr[0]}:{addr[1]}", end="")
                if time_since_last:
                    print(f" (gap: {time_since_last:.1f}s)", end="")
                print()
                
                if len(data) >= 5:
                    print(f"     Data: {' '.join([f'{b:02X}' for b in data[:10]])}")
                
                last_packet_time = current_time
                
                # Forward as unicast to Pico W
                try:
                    send_sock.sendto(data, (PICO_IP, PICO_PORT))
                    print(f"     -> Forwarded to {PICO_IP}:{PICO_PORT}\n")
                except Exception as e:
                    print(f"     [ERROR] Failed to forward: {e}\n")
                    
            except socket.timeout:
                # Timeout is expected - print status periodically
                current_time = time.time()
                if current_time - last_status_time >= status_interval:
                    elapsed = current_time - last_status_time
                    print(f"[STATUS] Still listening... (no packets for {elapsed:.0f}s, total received: {packet_count})")
                    last_status_time = current_time
                continue
                
    except KeyboardInterrupt:
        print("\n\nStopped by user")
        print(f"Total packets forwarded: {packet_count}")
    finally:
        recv_sock.close()
        send_sock.close()

if __name__ == "__main__":
    main()


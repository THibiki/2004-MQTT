#!/usr/bin/env python3
"""
WSL Multicast Forwarder
Runs in WSL to capture multicast from gateway and forward as unicast to Windows
"""
import socket
import struct
import sys
import time

# Gateway multicast configuration
MULTICAST_IP = "225.1.1.1"
MULTICAST_PORT = 1883

# Windows bridge configuration
WINDOWS_IP = "172.20.10.2"  # Your Windows machine IP
WINDOWS_PORT = 1883

def main():
    # Create socket for receiving multicast
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(1.0)
    
    # Bind to multicast port
    try:
        sock.bind(('0.0.0.0', MULTICAST_PORT))
    except OSError as e:
        print(f"[ERROR] Failed to bind to port {MULTICAST_PORT}: {e}")
        return
    
    # Join multicast group
    group = socket.inet_aton(MULTICAST_IP)
    mreq = struct.pack('4sL', group, socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    
    # Create socket for sending unicast to Windows
    send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    print("=" * 60)
    print("WSL Multicast to Unicast Forwarder")
    print("=" * 60)
    print(f"Listening for multicast: {MULTICAST_IP}:{MULTICAST_PORT}")
    print(f"Forwarding to Windows: {WINDOWS_IP}:{WINDOWS_PORT}")
    print("=" * 60)
    print("Waiting for packets...\n")
    
    packet_count = 0
    last_packet_time = None
    
    try:
        while True:
            try:
                # Receive multicast packet
                data, addr = sock.recvfrom(1024)
                packet_count += 1
                
                current_time = time.time()
                time_since_last = None
                if last_packet_time:
                    time_since_last = current_time - last_packet_time
                
                print(f"[{packet_count}] Received {len(data)} bytes from multicast", end="")
                if time_since_last:
                    print(f" (gap: {time_since_last:.1f}s)", end="")
                print()
                
                if len(data) >= 5:
                    print(f"     Data: {' '.join([f'{b:02X}' for b in data[:10]])}")
                
                last_packet_time = current_time
                
                # Forward as unicast to Windows
                try:
                    send_sock.sendto(data, (WINDOWS_IP, WINDOWS_PORT))
                    print(f"     -> Forwarded to Windows {WINDOWS_IP}:{WINDOWS_PORT}\n")
                except Exception as e:
                    print(f"     [ERROR] Failed to forward: {e}\n")
                    
            except socket.timeout:
                continue
                
    except KeyboardInterrupt:
        print("\n\nStopped by user")
        print(f"Total packets forwarded: {packet_count}")
    finally:
        sock.close()
        send_sock.close()

if __name__ == "__main__":
    main()




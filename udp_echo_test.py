#!/usr/bin/env python3
"""
Simple UDP Echo Server for Testing
Listens on all interfaces and echoes back any received data
Run this on WSL to test if Pico can reach it
"""

import socket
import sys
from datetime import datetime

PORT = 1885

def log(msg):
    print(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] {msg}")

def main():
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        # Bind to all interfaces
        sock.bind(('0.0.0.0', PORT))
        log(f"✓ UDP Echo Server listening on 0.0.0.0:{PORT}")
        log(f"  This means it's accessible on:")
        log(f"    - 172.29.116.3:{PORT} (WSL IP)")
        log(f"    - 127.0.0.1:{PORT} (localhost)")
        log(f"  Waiting for packets from Pico...")
        print()
    except OSError as e:
        log(f"❌ Cannot bind to port {PORT}")
        log(f"   Error: {e}")
        log(f"   Is something else using this port?")
        log(f"   Try: sudo netstat -ulnp | grep {PORT}")
        sys.exit(1)
    
    packet_count = 0
    
    while True:
        try:
            # Receive data
            data, addr = sock.recvfrom(4096)
            packet_count += 1
            
            log(f"📨 Packet #{packet_count} from {addr}")
            log(f"   Length: {len(data)} bytes")
            log(f"   Data: {data[:100]}")  # Print first 100 bytes
            
            # Echo it back
            sock.sendto(data, addr)
            log(f"   ✓ Echoed back to {addr}")
            print()
            
        except KeyboardInterrupt:
            log("\n👋 Shutting down...")
            break
        except Exception as e:
            log(f"❌ Error: {e}")
    
    sock.close()
    log(f"Total packets received: {packet_count}")

if __name__ == "__main__":
    main()
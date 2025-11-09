#!/usr/bin/env python3
"""
Bidirectional UDP Forwarder for MQTT-SN Gateway
Forwards packets between Windows and WSL in both directions
Run this on Windows (not WSL)
"""

import socket
import sys
import threading
import select
from datetime import datetime

# Configuration
WINDOWS_IP = "172.20.10.7"   # Windows hotspot IP (where Pico sends)
WSL_IP = "172.29.116.3"       # WSL IP (where gateway runs)
GATEWAY_PORT = 1885           # Port gateway listens on
LISTEN_PORT = 1885            # Port we listen on Windows side

def log(message):
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{timestamp}] {message}")

def hex_dump(data, max_bytes=32):
    """Return hex dump of data"""
    preview = data[:max_bytes]
    hex_str = ' '.join(f'{b:02x}' for b in preview)
    if len(data) > max_bytes:
        hex_str += f'... ({len(data)} bytes total)'
    return hex_str

class BidirectionalForwarder:
    def __init__(self):
        self.pico_addr = None
        self.running = True
        self.stats = {'to_wsl': 0, 'to_pico': 0}
        
    def run(self):
        """Main forwarding loop using select for bidirectional forwarding"""
        
        # Create Windows-side socket (for Pico)
        sock_windows = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock_windows.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock_windows.setblocking(False)
        
        try:
            sock_windows.bind((WINDOWS_IP, LISTEN_PORT))
            log(f"✓ Listening on {WINDOWS_IP}:{LISTEN_PORT} (for Pico)")
        except OSError as e:
            log(f"❌ Cannot bind to {WINDOWS_IP}:{LISTEN_PORT}")
            log(f"   Error: {e}")
            log(f"\nTroubleshooting:")
            log(f"1. Check IP exists: ipconfig | findstr {WINDOWS_IP}")
            log(f"2. Check port free: netstat -an | findstr :{LISTEN_PORT}")
            log(f"3. Run as Administrator")
            log(f"4. Try different port if 1885 is taken")
            sys.exit(1)
        
        # Create WSL-side socket
        sock_wsl = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock_wsl.setblocking(False)
        # Bind to any available port for WSL communication
        sock_wsl.bind(('0.0.0.0', 0))
        local_port = sock_wsl.getsockname()[1]
        log(f"✓ WSL comm socket on port {local_port}")
        log(f"✓ Will forward to {WSL_IP}:{GATEWAY_PORT}")
        print()
        log("🚀 Forwarder active - waiting for packets...")
        print()
        
        while self.running:
            try:
                # Wait for data on either socket
                readable, _, _ = select.select([sock_windows, sock_wsl], [], [], 1.0)
                
                for sock in readable:
                    data, addr = sock.recvfrom(4096)
                    
                    if sock == sock_windows:
                        # Packet from Pico → forward to WSL
                        if self.pico_addr is None:
                            self.pico_addr = addr
                            log(f"🎯 Pico discovered: {addr}")
                        
                        log(f"→ Pico→WSL: {len(data)}B from {addr}")
                        log(f"  {hex_dump(data)}")
                        sock_wsl.sendto(data, (WSL_IP, GATEWAY_PORT))
                        self.stats['to_wsl'] += 1
                        log(f"  ✓ Forwarded to WSL")
                        
                    elif sock == sock_wsl:
                        # Response from WSL → forward to Pico
                        if self.pico_addr:
                            log(f"← WSL→Pico: {len(data)}B from {addr}")
                            log(f"  {hex_dump(data)}")
                            sock_windows.sendto(data, self.pico_addr)
                            self.stats['to_pico'] += 1
                            log(f"  ✓ Forwarded to Pico {self.pico_addr}")
                        else:
                            log(f"⚠ Got WSL response but don't know Pico address yet")
                
            except socket.error as e:
                if e.errno != 10035:  # WSAEWOULDBLOCK is expected for non-blocking
                    log(f"Socket error: {e}")
            except KeyboardInterrupt:
                break
            except Exception as e:
                log(f"Error: {e}")
                import traceback
                traceback.print_exc()
        
        sock_windows.close()
        sock_wsl.close()
        log("Sockets closed")

def main():
    print("=" * 70)
    print("     MQTT-SN Bidirectional UDP Forwarder (Windows ↔ WSL)")
    print("=" * 70)
    print(f"  Windows side: {WINDOWS_IP}:{LISTEN_PORT}")
    print(f"  WSL side:     {WSL_IP}:{GATEWAY_PORT}")
    print("=" * 70)
    print()
    print("Setup checklist:")
    print("  ✓ Pico configured to send to:", WINDOWS_IP)
    print("  ✓ WSL gateway running on:", WSL_IP)
    print("  ✓ This forwarder running on: Windows")
    print()
    
    forwarder = BidirectionalForwarder()
    
    try:
        forwarder.run()
    except KeyboardInterrupt:
        pass
    finally:
        log("")
        log("=" * 40)
        log(f"📊 Statistics:")
        log(f"  Packets to WSL:  {forwarder.stats['to_wsl']}")
        log(f"  Packets to Pico: {forwarder.stats['to_pico']}")
        log("=" * 40)
        log("👋 Shutdown complete")

if __name__ == "__main__":
    main()
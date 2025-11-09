#!/usr/bin/env python3
"""
UDP Forwarder for MQTT-SN Gateway
Forwards UDP packets from Windows IP to WSL IP
Run this on Windows (not WSL)
"""

import socket
import sys
import threading
from datetime import datetime

# Configuration
WINDOWS_IP = "172.20.10.7"  # Windows hotspot IP (where Pico sends)
WSL_IP = "172.29.116.3"      # WSL IP (where gateway runs)
PORT = 1885
WSL_LISTEN_PORT = 10885      # Different port to listen for WSL responses

def log(message):
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"[{timestamp}] {message}")

def hex_dump(data, max_bytes=64):
    """Return hex dump of data"""
    preview = data[:max_bytes]
    hex_str = ' '.join(f'{b:02x}' for b in preview)
    if len(data) > max_bytes:
        hex_str += '...'
    return hex_str

class UDPForwarder:
    def __init__(self):
        self.pico_addr = None
        self.sock_listen = None
        self.sock_send_to_wsl = None
        self.lock = threading.Lock()
        
    def setup_sockets(self):
        """Setup listening socket on Windows IP"""
        # Socket to listen for Pico packets
        self.sock_listen = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock_listen.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        try:
            self.sock_listen.bind((WINDOWS_IP, PORT))
            log(f"✓ Bound to {WINDOWS_IP}:{PORT}")
        except OSError as e:
            log(f"ERROR: Cannot bind to {WINDOWS_IP}:{PORT}")
            log(f"       {e}")
            log(f"\nTroubleshooting:")
            log(f"1. Verify IP exists: ipconfig | findstr {WINDOWS_IP}")
            log(f"2. Check port not in use: netstat -an | findstr :{PORT}")
            log(f"3. Try running as Administrator")
            log(f"4. Try disabling Windows Firewall temporarily")
            sys.exit(1)
        
        # Socket to send to WSL
        self.sock_send_to_wsl = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        log(f"✓ Ready to forward to {WSL_IP}:{PORT}")
    
    def forward_pico_to_wsl(self):
        """Forward packets from Pico to WSL gateway"""
        log("Started: Pico → WSL forwarder")
        
        while True:
            try:
                data, addr_from = self.sock_listen.recvfrom(4096)
                
                with self.lock:
                    # Remember Pico's address from first packet
                    if self.pico_addr is None:
                        self.pico_addr = addr_from
                        log(f"🎯 Pico detected at {addr_from}")
                
                log(f"→ RX {len(data)}B from Pico {addr_from}")
                log(f"  Data: {hex_dump(data, 32)}")
                
                # Forward to WSL
                self.sock_send_to_wsl.sendto(data, (WSL_IP, PORT))
                log(f"  ✓ FWD to WSL {WSL_IP}:{PORT}")
                
            except Exception as e:
                log(f"❌ Error in forward_pico_to_wsl: {e}")
                import traceback
                traceback.print_exc()

def main():
    print("=" * 60)
    print("        MQTT-SN UDP Forwarder (Windows ↔ WSL)")
    print("=" * 60)
    print(f"  Listen on:  {WINDOWS_IP}:{PORT} (Windows - for Pico)")
    print(f"  Forward to: {WSL_IP}:{PORT} (WSL - gateway)")
    print("=" * 60)
    print()
    
    log("Initializing forwarder...")
    
    forwarder = UDPForwarder()
    forwarder.setup_sockets()
    
    # Start forwarding thread (Pico → WSL)
    thread = threading.Thread(target=forwarder.forward_pico_to_wsl, daemon=True)
    thread.start()
    
    log("✓ Forwarder running")
    log("  Waiting for packets from Pico...")
    log("  Press Ctrl+C to stop")
    print()
    
    try:
        # Keep main thread alive
        while True:
            threading.Event().wait(1)
    except KeyboardInterrupt:
        log("\n👋 Shutting down...")
        sys.exit(0)

if __name__ == "__main__":
    main()
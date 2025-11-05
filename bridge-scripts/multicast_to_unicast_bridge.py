#!/usr/bin/env python3
"""
Multicast to Unicast Bridge for MQTT-SN Gateway
Receives multicast packets from gateway (225.1.1.1:1883) and forwards them as unicast to Pico W
"""
import socket
import struct
import sys
import time

# Gateway multicast configuration
MULTICAST_IP = "225.1.1.1"
MULTICAST_PORT = 1883

# Pico W configuration
PICO_IP = "172.20.10.5"  # change this to your Pico W's IP
PICO_PORT = 1883

def get_local_interface_ip(target_ip):
    """Get the local IP address on the same subnet as target_ip"""
    try:
        # Create a UDP socket to determine which interface would be used
        test_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        test_sock.settimeout(1)
        test_sock.connect((target_ip, 80))
        local_ip = test_sock.getsockname()[0]
        test_sock.close()
        return local_ip
    except Exception as e:
        print(f"[WARNING] Could not determine local interface: {e}")
        return None

def get_all_interfaces():
    """Get all available network interfaces by trying to connect to different addresses"""
    interfaces = []
    # Try common private network ranges to detect interfaces
    test_ips = ["8.8.8.8", "1.1.1.1"]  # Public DNS servers to detect default route
    for test_ip in test_ips:
        try:
            test_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            test_sock.settimeout(0.1)
            test_sock.connect((test_ip, 53))
            local_ip = test_sock.getsockname()[0]
            test_sock.close()
            if local_ip and local_ip not in interfaces and not local_ip.startswith('127.'):
                interfaces.append(local_ip)
        except:
            pass
    return interfaces

def main():
    # Get local interface IP on the same network as Pico W
    local_ip = get_local_interface_ip(PICO_IP)
    if local_ip:
        print(f"[INFO] Detected interface for Pico W: {local_ip}")
    else:
        # Try to get all interfaces and use the first non-localhost one
        all_interfaces = get_all_interfaces()
        if all_interfaces:
            local_ip = all_interfaces[0]
            print(f"[INFO] Using interface: {local_ip} (from available interfaces)")
        else:
            print("[WARNING] Could not detect network interface, using INADDR_ANY")
    
    # Create socket for receiving multicast
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    # On Windows, SO_REUSEPORT may not be available, but we can try SO_REUSEADDR
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except AttributeError:
        # SO_REUSEPORT not available on Windows, that's OK
        pass
    
    # Make socket non-blocking so we can check for Ctrl+C
    sock.settimeout(1.0)  # 1 second timeout
    
    # Bind to the multicast port
    # On Windows, binding to '' might not work for multicast, try binding to 0.0.0.0
    try:
        sock.bind(('', MULTICAST_PORT))
    except OSError as e:
        print(f"[ERROR] Failed to bind to port {MULTICAST_PORT}: {e}")
        print("[INFO] Trying to bind to 0.0.0.0...")
        sock.bind(('0.0.0.0', MULTICAST_PORT))
    
    # Join multicast group
    group = socket.inet_aton(MULTICAST_IP)
    
    # On Windows, we may need to specify the interface IP instead of INADDR_ANY
    # The struct format is: 4 bytes for multicast IP + 4 bytes for interface IP (as unsigned long)
    if local_ip:
        try:
            # Try with specific interface first (better for Windows)
            interface_bytes = socket.inet_aton(local_ip)
            # Convert interface bytes to unsigned long (network byte order)
            interface_long = struct.unpack('!L', interface_bytes)[0]
            mreq = struct.pack('4sL', group, interface_long)
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
            print(f"[INFO] Joined multicast group {MULTICAST_IP} on interface {local_ip}")
        except Exception as e:
            print(f"[WARNING] Failed to join multicast on specific interface: {e}")
            print("[INFO] Trying with INADDR_ANY...")
            # Fall back to INADDR_ANY
            mreq = struct.pack('4sL', group, socket.INADDR_ANY)
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
            print("[INFO] Joined multicast group using INADDR_ANY")
    else:
        # Use INADDR_ANY if we couldn't determine interface
        mreq = struct.pack('4sL', group, socket.INADDR_ANY)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        print("[INFO] Joined multicast group using INADDR_ANY")
    
    # Enable multicast loopback (so we can receive our own packets if needed)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
    
    # Create socket for sending unicast to Pico W
    send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    print("=" * 60)
    print("MQTT-SN Multicast to Unicast Bridge")
    print("=" * 60)
    print(f"Listening for multicast: {MULTICAST_IP}:{MULTICAST_PORT}")
    print(f"Forwarding to Pico W: {PICO_IP}:{PICO_PORT}")
    print("=" * 60)
    print("Waiting for packets...\n")
    
    packet_count = 0
    last_packet_time = None
    last_status_time = time.time()
    status_interval = 10.0  # Print status every 10 seconds
    
    print("[INFO] Bridge is ready and listening for multicast packets...")
    print("[INFO] If no packets are received, check:")
    print("       1. Gateway is running and sending ADVERTISE messages")
    print("       2. Gateway and bridge are on the same network")
    print("       3. Windows Firewall allows UDP port 1883")
    print("       4. Network interface supports multicast\n")
    
    try:
        while True:
            try:
                # Receive multicast packet (with timeout so we can check for Ctrl+C)
                data, addr = sock.recvfrom(1024)
                packet_count += 1
                
                current_time = time.time()
                
                # Calculate time since last packet
                time_since_last = None
                if last_packet_time:
                    time_since_last = current_time - last_packet_time
                
                # Always print when we receive a packet (action item)
                print(f"[{packet_count}] Received {len(data)} bytes from {addr[0]}:{addr[1]}", end="")
                if time_since_last:
                    print(f" (gap: {time_since_last:.1f}s)", end="")
                print()
                
                if len(data) >= 5:
                    print(f"     Data: {' '.join([f'{b:02X}' for b in data[:10]])}")
                
                # Warn if packets are coming too frequently
                if time_since_last and time_since_last < 5:
                    print(f"     [WARNING] Packets arriving every {time_since_last:.2f}s - faster than expected (60s KeepAlive)")
                
                last_packet_time = current_time
                
                # Forward as unicast to Pico W (always print - this is an action)
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
        sock.close()
        send_sock.close()

if __name__ == "__main__":
    main()


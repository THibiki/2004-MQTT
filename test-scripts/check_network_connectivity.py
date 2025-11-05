#!/usr/bin/env python3
"""
Network Connectivity Checker
Checks if gateway and bridge are on the same network and can communicate
"""
import socket
import subprocess
import sys
import platform

def get_local_ip():
    """Get local IP address"""
    try:
        # Connect to a remote address to determine local IP
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(1)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except Exception as e:
        print(f"[ERROR] Could not determine local IP: {e}")
        return None

def get_network_info():
    """Get network information"""
    print("=" * 60)
    print("Network Connectivity Check")
    print("=" * 60)
    
    local_ip = get_local_ip()
    if local_ip:
        print(f"Local IP Address: {local_ip}")
        
        # Get subnet
        parts = local_ip.split('.')
        if len(parts) == 4:
            subnet = f"{parts[0]}.{parts[1]}.{parts[2]}.0/24"
            print(f"Detected Subnet: {subnet}")
    else:
        print("[ERROR] Could not determine local IP")
        return None
    
    print("\nChecking network interfaces...")
    try:
        if platform.system() == "Windows":
            result = subprocess.run(['ipconfig'], capture_output=True, text=True, shell=True)
            print(result.stdout)
        else:
            result = subprocess.run(['ifconfig'], capture_output=True, text=True)
            print(result.stdout)
    except Exception as e:
        print(f"[WARNING] Could not get network info: {e}")
    
    return local_ip

def check_wsl_gateway():
    """Check if gateway might be running in WSL"""
    print("\n" + "=" * 60)
    print("WSL Detection")
    print("=" * 60)
    
    if platform.system() == "Windows":
        try:
            # Check if WSL is installed
            result = subprocess.run(['wsl', '--list', '--verbose'], 
                                  capture_output=True, text=True, shell=True)
            if result.returncode == 0:
                print("[INFO] WSL is installed")
                print("[WARNING] If gateway is running in WSL, multicast may not work!")
                print("[INFO] WSL uses a virtual network adapter that may not forward multicast properly")
                print("[INFO] Solution: Run gateway on Windows natively or use WSL2 with proper network mode")
                return True
        except:
            pass
    
    print("[INFO] Not running in WSL context")
    return False

def test_multicast_reception():
    """Test if we can receive multicast packets"""
    print("\n" + "=" * 60)
    print("Multicast Reception Test")
    print("=" * 60)
    
    MULTICAST_IP = "225.1.1.1"
    MULTICAST_PORT = 1883
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.settimeout(2.0)
        sock.bind(('0.0.0.0', MULTICAST_PORT))
        
        # Join multicast group
        group = socket.inet_aton(MULTICAST_IP)
        mreq = struct.pack('4sL', group, socket.INADDR_ANY)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        
        print(f"[INFO] Socket created and bound to {MULTICAST_IP}:{MULTICAST_PORT}")
        print("[INFO] Waiting 2 seconds for multicast packets...")
        
        try:
            data, addr = sock.recvfrom(1024)
            print(f"[SUCCESS] Received {len(data)} bytes from {addr[0]}:{addr[1]}")
            print(f"         Data: {' '.join([f'{b:02X}' for b in data[:10]])}")
            sock.close()
            return True
        except socket.timeout:
            print("[WARNING] No multicast packets received in 2 seconds")
            print("[INFO] This could mean:")
            print("        - Gateway is not sending multicast")
            print("        - Multicast is not being forwarded from WSL")
            print("        - Network interface doesn't support multicast")
            print("        - Firewall is blocking multicast")
            sock.close()
            return False
            
    except Exception as e:
        print(f"[ERROR] Multicast test failed: {e}")
        return False

if __name__ == "__main__":
    import struct
    
    local_ip = get_network_info()
    is_wsl = check_wsl_gateway()
    
    if is_wsl:
        print("\n" + "!" * 60)
        print("IMPORTANT: Gateway appears to be running in WSL")
        print("!" * 60)
        print("\nMulticast from WSL typically does NOT reach Windows.")
        print("Solutions:")
        print("1. Run gateway on Windows natively (not in WSL)")
        print("2. Use WSL2 with mirror networking mode")
        print("3. Use a different machine for the gateway")
        print("4. Configure WSL to forward multicast (complex)")
        print("!" * 60 + "\n")
    
    test_multicast_reception()
    
    print("\n" + "=" * 60)
    print("Recommendation:")
    print("=" * 60)
    print("If gateway is in WSL and multicast test failed,")
    print("the gateway needs to run on Windows or a separate machine.")
    print("=" * 60)


import socket
import threading
import sys

def udp_relay(listen_host, listen_port, target_host, target_port):
    print(f"UDP Relay: {listen_host}:{listen_port} -> {target_host}:{target_port}")
    
    # Create listening socket
    listen_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    listen_sock.bind((listen_host, listen_port))
    print(f"Listening on {listen_host}:{listen_port}")
    
    def forward_to_target(data, client_addr):
        try:
            # Forward to target
            target_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            target_sock.sendto(data, (target_host, target_port))
            
            print(f"Forwarded {len(data)} bytes from {client_addr} to {target_host}:{target_port}")
            
            # Listen for response with timeout
            try:
                target_sock.settimeout(10.0)
                response, _ = target_sock.recvfrom(4096)
                listen_sock.sendto(response, client_addr)
                print(f"Forwarded response {len(response)} bytes back to {client_addr}")
            except socket.timeout:
                print(f"No response from target for {client_addr} (this is normal for initial packets)")
            except Exception as e:
                print(f"Error receiving response: {e}")
            finally:
                target_sock.close()
                
        except Exception as e:
            print(f"Error forwarding: {e}")
    
    try:
        print("Waiting for packets...")
        while True:
            data, client_addr = listen_sock.recvfrom(4096)
            print(f"Received {len(data)} bytes from {client_addr}")
            
            # Handle in separate thread to allow bidirectional communication
            thread = threading.Thread(target=forward_to_target, args=(data, client_addr))
            thread.daemon = True
            thread.start()
            
    except KeyboardInterrupt:
        print("Stopping relay...")
    finally:
        listen_sock.close()

if __name__ == "__main__":
    # Listen on Windows Wi-Fi interface, forward to WSL
    listen_host = "172.20.10.14"  # Windows Wi-Fi IP
    listen_port = 1884
    target_host = "172.30.67.75"  # WSL IP  
    target_port = 1884
    
    print("MQTT-SN UDP Relay for Pico W")
    print("Copy this script to Windows and run: python udp_relay_windows.py")
    print()
    
    udp_relay(listen_host, listen_port, target_host, target_port)
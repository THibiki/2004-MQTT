import socket
import threading

# UDP listener (from Pico)
UDP_LISTEN_IP = "0.0.0.0"
UDP_LISTEN_PORT = 1885

# TCP connection to MQTT broker
BROKER_IP = "127.0.0.1"  # Change to your broker's IP
BROKER_PORT = 1883

# Store Pico's address for responses
pico_addr = None

def tcp_to_udp_forwarder(tcp_sock, udp_sock):
    """Forward TCP responses back to Pico via UDP"""
    global pico_addr
    while True:
        try:
            data = tcp_sock.recv(4096)
            if not data or pico_addr is None:
                break
            print(f"TCP→UDP: {len(data)} bytes to {pico_addr}")
            udp_sock.sendto(data, pico_addr)
        except Exception as e:
            print(f"TCP receive error: {e}")
            break

# Create UDP socket
udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_sock.bind((UDP_LISTEN_IP, UDP_LISTEN_PORT))

# Create TCP socket to broker
tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcp_sock.connect((BROKER_IP, BROKER_PORT))
print(f"Connected to broker at {BROKER_IP}:{BROKER_PORT}")

# Start TCP→UDP forwarding thread
threading.Thread(target=tcp_to_udp_forwarder, args=(tcp_sock, udp_sock), daemon=True).start()

print(f"Gateway listening on UDP {UDP_LISTEN_PORT}")
while True:
    data, addr = udp_sock.recvfrom(4096)
    pico_addr = addr  # Save Pico's address
    print(f"UDP→TCP: {len(data)} bytes from {addr}")
    tcp_sock.sendall(data)

import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 1883))

print(f"Listening on port 1883")
print(f"PC IP: {socket.gethostbyname(socket.gethostname())}")
print("Waiting for packets from Pico W...")

while True:
    data, addr = sock.recvfrom(1024)
    print(f"RX from {addr}: {' '.join(f'{b:02X}' for b in data)}")
    sock.sendto(data, addr)  # Echo back
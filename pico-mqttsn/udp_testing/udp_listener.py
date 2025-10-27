import socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('0.0.0.0', 5005))
print('Listening on UDP port 5005...')
while True:
    data, addr = s.recvfrom(4096)
    print('Got', data, 'from', addr)
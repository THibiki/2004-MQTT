import sys
import socket

REPLY = "--reply" in sys.argv

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 1885))
print("Listening on 0.0.0.0:1885 (reply=%s)" % REPLY)

while True:
    data, addr = sock.recvfrom(65535)
    print("recv", len(data), "from", addr, "data:", data.hex())
    if REPLY:
        try:
            sock.sendto(b"ACK", addr)
        except Exception as e:
            print("reply failed:", e)
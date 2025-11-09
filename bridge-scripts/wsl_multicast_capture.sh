#!/bin/bash
# WSL Multicast Capture Script
# Captures multicast packets in WSL and forwards them as unicast to Windows
# Run this in WSL if gateway is also in WSL

MULTICAST_IP="225.1.1.1"
MULTICAST_PORT=1883
WINDOWS_IP="172.20.10.2"  # Your Windows machine IP
WINDOWS_PORT=1883

echo "============================================================"
echo "WSL Multicast Capture to Windows Bridge"
echo "============================================================"
echo "Multicast: $MULTICAST_IP:$MULTICAST_PORT"
echo "Forwarding to Windows: $WINDOWS_IP:$WINDOWS_PORT"
echo "============================================================"

# Check if socat is installed
if ! command -v socat &> /dev/null; then
    echo "[ERROR] socat is not installed"
    echo "Install it with: sudo apt-get update && sudo apt-get install -y socat"
    exit 1
fi

# Capture multicast and forward to Windows
socat UDP4-RECVFROM:$MULTICAST_PORT,ip-add-membership=$MULTICAST_IP:0.0.0.0,reuseaddr,fork UDP4-SENDTO:$WINDOWS_IP:$WINDOWS_PORT




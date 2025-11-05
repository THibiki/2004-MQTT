# WSL Multicast Issue - Solutions

## Problem
The MQTT-SN Gateway is running in WSL, but **multicast packets from WSL do NOT reach Windows**. This is a known limitation of WSL networking.

## Network Configuration

### Your Current Setup:
- **Windows IP**: 172.20.10.2 (Wi-Fi)
- **Pico W IP**: 172.20.10.5
- **WSL Network**: 192.168.144.1 (separate virtual network)
- **Gateway**: Running in WSL, sending multicast to 225.1.1.1:1883

### The Issue:
Multicast packets sent from WSL are trapped in the WSL virtual network and don't reach Windows.

## Solutions

### Solution 1: Run Gateway on Windows (Recommended)
**Best option if you have a Windows build of the gateway**

1. Compile the gateway for Windows
2. Run it natively on Windows (not in WSL)
3. Multicast will work normally

### Solution 2: Use WSL2 with Mirror Networking (Windows 11)
**If you're on Windows 11, you can enable mirror networking**

1. Check your WSL version:
   ```powershell
   wsl --list --verbose
   ```

2. If using WSL2, create/edit `.wslconfig` in your Windows user folder:
   ```
   [wsl2]
   networkingMode=mirrored
   ```

3. Restart WSL:
   ```powershell
   wsl --shutdown
   ```

4. Restart gateway in WSL

### Solution 3: WSL Multicast Forwarder Script
**Use a script in WSL to capture multicast and forward as unicast**

1. In WSL, install `socat`:
   ```bash
   sudo apt-get update
   sudo apt-get install -y socat
   ```

2. Run the capture script (in WSL):
   ```bash
   cd "/mnt/c/Users/limxu/.a Git Clones/2004-MQTT-1/test-scripts"
   chmod +x wsl_multicast_capture.sh
   ./wsl_multicast_capture.sh
   ```

3. This will forward multicast from WSL to Windows at 172.20.10.2:1883
4. Make sure your Windows bridge script is listening

### Solution 4: Use a Different Machine
Run the gateway on:
- A Linux machine on your network
- A Raspberry Pi
- A separate Windows machine
- A VM with proper network bridging (not WSL)

### Solution 5: Python Script in WSL
**Alternative to socat - pure Python solution**

Create a Python script in WSL that:
1. Joins the multicast group in WSL
2. Receives multicast packets
3. Forwards them as unicast to Windows (172.20.10.2:1883)

## Checking Network Connectivity

### Verify Gateway and Bridge are on Same Network:

1. **Check WSL IP** (in WSL):
   ```bash
   hostname -I
   ip addr show
   ```

2. **Check Windows IP** (in PowerShell):
   ```powershell
   ipconfig
   ```

3. **Test Connectivity** (from WSL to Windows):
   ```bash
   ping 172.20.10.2
   ```

4. **Test Multicast Reception** (run the check script):
   ```powershell
   cd "C:\Users\limxu\.a Git Clones\2004-MQTT-1\test-scripts"
   python check_network_connectivity.py
   ```

### Network Requirements:
- Gateway and bridge must be on the **same physical network** (same router/Wi-Fi)
- Same subnet (172.20.10.x)
- Firewall allows UDP port 1883
- Network supports multicast (most home networks do)

## Quick Test

Test if multicast works at all:

1. **In WSL**, send a test multicast packet:
   ```bash
   echo "test" | socat - UDP4-DATAGRAM:225.1.1.1:1883,broadcast
   ```

2. **In Windows**, check if bridge script receives it

If it doesn't work, use Solution 3 (WSL forwarder) or Solution 1 (native Windows gateway).

## Recommended Approach

For your setup, I recommend **Solution 3** (WSL forwarder script) because:
- Gateway can stay in WSL
- Simple to implement
- No need to recompile gateway
- Works immediately

Then run:
1. WSL forwarder script (captures multicast, sends unicast to Windows)
2. Windows bridge script (receives unicast from WSL, forwards to Pico W)

This creates a two-hop bridge:
```
Gateway (WSL multicast) → WSL Forwarder → Windows Bridge → Pico W
```


# MQTT-SN Gateway Running Guide

## GATEWAY ALWAYS JS WSL PLS

## Quick Reference

```bash
# Navigate to gateway directory
cd paho.mqtt-sn.embedded-c/MQTTSNGateway/bin

# Start gateway
./MQTT-SNGateway

# Check if running
ps aux | grep MQTT-SNGateway | grep -v grep

# Stop gateway
pkill -f MQTT-SNGateway

# Kill all processes
pkill -9 -f MQTT-SNGateway
```

## 1. Start Gateway

**IMPORTANT:** Before starting, check `gateway.conf`:
```bash
cd paho.mqtt-sn.embedded-c/MQTTSNGateway/bin
grep ShearedMemory gateway.conf
```

**Must be:** `ShearedMemory=NO` (logs go to terminal stdout)


Then start the gateway:
```bash
./MQTT-SNGateway
```

**What you should see:**
- Gateway startup message
- ADVERTISE messages every ~100 seconds
- Client connection messages
- All gateway activity

Press `Ctrl+C` to stop the gateway.

## 2. Check if Gateway is Running

### Check Process
```bash
ps aux | grep MQTT-SNGateway | grep -v grep
```

**Expected output:**
```
xuan   12345  0.0  0.0 250100  5504 ?  Ssl  12:55  0:05 ./MQTT-SNGateway
```

### Check Port
```bash
ss -uln | grep 10000
```

**Expected output:**
```
UNCONN 0  0  0.0.0.0:10000  0.0.0.0:*
```

## 3. Stop Gateway

```bash
# Graceful stop
pkill -f MQTT-SNGateway

# Force kill (if needed)
pkill -9 -f MQTT-SNGateway
```

### Verify Stopped
```bash
ps aux | grep MQTT-SNGateway | grep -v grep
# Should return nothing
```

## 4. Kill All Processes

```bash
# Kill all gateway processes
pkill -9 -f MQTT-SNGateway

# Verify port is free
ss -uln | grep 10000
# Should return nothing if port is free
```

## 5. View Gateway Logs

When you run `./MQTT-SNGateway` in the terminal, logs appear **directly in that same terminal window**. You don't need to do anything else - just watch the terminal output.

**Gateway Starting:**
```
 ***************************************************************************
 * MQTT-SN Gateway
 * Part of Project Paho in Eclipse
 * (https://github.com/eclipse/paho.mqtt-sn.embedded-c.git)
 *
 * Author : Tomoaki YAMAGUCHI
 * Version: 1.5.1
 ***************************************************************************
 ConfigFile  : ./gateway.conf
 ClientList  : /path/to/your_clients.conf
 Broker      : mqtt.eclipseprojects.io : 1883, 8883
 RootCApath  : (null)
 RootCAfile  : (null)
 CertKey     : (null)
 PrivateKey  : (null)
 SensorN/W   : UDP Multicast 225.1.1.1:1883, Gateway Port:10000, TTL:1
 Max Clients : 30

20251105 184055.295 PahoGateway-01 starts running.

```

**ADVERTISE Messages:**
```
20251105 181457.367   ADVERTISE         --->  Clients                             05 00 01 00 3C
20251105 181557.430   ADVERTISE         --->  Clients                             05 00 01 00 3C
```

**What this means:**
- ✅ Gateway is running correctly
- ✅ ADVERTISE messages are sent periodically (every ~100 seconds)

**Invalid Packets (Normal):**
```
20251105 181918.166   UNKNOWN           <---  Unknown Client !                    74 65 73 74 0A
Error: MQTTSNGWClientRecvTask  Client(127.0.0.1:58699) is not connecting. message has been discarded.
```

This is expected - gateway correctly rejects invalid packets.

**Client Activity (when clients connect - NOT YET CUZ MQTT NOT DONE (SOON AH) :/ ):**
```
20251105 181500.123 CONNECT       <--- Client 1                            04 00 06 00 0A 00 01 00 0A myclientid
20251105 181500.125 CONNACK       ---> Client 1                            03 00 00
```


## 6. Test Gateway Connectivity

### Test UDP Port
```bash
# Send a test UDP packet
echo "test" | nc -u localhost 10000
```

You should see `UNKNOWN` message in gateway logs - this is expected (invalid packet format).

## 7. Common Issues

### Port Already in Use
**Error:** `Address already in use` or `Can't open a UDP`

**Solution:**
```bash
# Kill all gateway processes
pkill -9 -f MQTT-SNGateway

# Verify port is free
ss -uln | grep 10000

# Start gateway again
cd paho.mqtt-sn.embedded-c/MQTTSNGateway/bin
./MQTT-SNGateway
```

### Multiple Gateway Processes
```bash
# Check
ps aux | grep MQTT-SNGateway | grep -v grep

# Kill all
pkill -9 -f MQTT-SNGateway
```

### Permission Denied (Shared Memory)
**Error:** `RingBuffer can't create a shared memory. ABORT Gateway!!!`

**Solution:** Run with sudo (only needed first time):
```bash
sudo ./MQTT-SNGateway
```

## Complete Workflow

### Start Gateway
```bash
cd paho.mqtt-sn.embedded-c/MQTTSNGateway/bin
./MQTT-SNGateway
```

### Stop Gateway
```bash
# In another terminal
pkill -9 -f MQTT-SNGateway
```

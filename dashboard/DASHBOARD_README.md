# MQTT Dashboard Setup Guide

## Quick Start (2 minutes)

### Step 1: Navigate to Dashboard Folder
```bash
cd <project-root>\dashboard
```

### Step 2: Create Virtual Environment (First Time Only)
```bash
python -m venv venv
```

If that doesn't work, try:
- `python3 -m venv venv`
- `py -m venv venv`

### Step 3: Activate Virtual Environment
```bash
venv\Scripts\activate
```

You should see `(venv)` appear at the beginning of your prompt:
```
(venv) C:\Users\
```

### Step 4: Install Dependencies from requirements.txt
```bash
pip install -r requirements.txt
```

### Step 5: Start the Dashboard Server
```bash
python dashboard_server.py
```

You should see:
```
====================================================================
📊 MQTT Dashboard Server
====================================================================
📡 Connecting to MQTT broker at localhost:1883...
✅ Connected to MQTT broker localhost:1883
📡 Subscribed to pico/chunks, pico/test, pico/block

🌐 Dashboard server starting on http://localhost:5000
   Open your browser and navigate to: http://localhost:5000

⏳ Waiting for connections...
```

### Step 6: Open Dashboard in Browser
Open your web browser and go to:
```
http://localhost:5000
```

Done! The dashboard will automatically connect and start displaying data from your Pico W.

---

## **Full Command Sequence (Copy-Paste)**

For first-time setup:
```bash
cd <project-root>\dashboard
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
python dashboard_server.py
```

For subsequent runs (after venv is created):
```bash
cd <project-root>\dashboard
venv\Scripts\activate
python dashboard_server.py
```

To deactivate the virtual environment when done:
```bash
deactivate
```

---

## What This Does

The dashboard server acts as a bridge between:
- **Mosquitto (MQTT Broker)** ← connects via TCP on port 1883
- **Your Browser** ← connects via WebSocket

### Architecture:
```
Pico W → UDP → MQTT-SN Gateway → TCP → Mosquitto → TCP → Dashboard Server → WebSocket → Browser
```

---

## Features

✅ **Real-time monitoring** of MQTT messages from Pico W
✅ **Block transfer visualization** with progress bar
✅ **Image display** - automatically shows received images
✅ **QoS level tracking** - displays current QoS setting
✅ **Connection status** - shows MQTT and WebSocket status
✅ **Message log** - color-coded activity log

---

## Requirements

- Python 3.7+
- Mosquitto running on `localhost:1883` (should already be running from your setup)
- Port 5000 available for dashboard server

---

## Troubleshooting

### "python: command not found" or "No module named venv"
**Solution:** Try alternative Python commands:
- `python3 -m venv venv`
- `py -m venv venv`

### "Scripts cannot be loaded because running scripts is disabled"
**Error:** PowerShell execution policy prevents activation
**Solution:** Run PowerShell as Administrator and execute:
```bash
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```
Then try activating the venv again.

### Dashboard server won't start
**Error:** `Address already in use`
**Solution:** Another program is using port 5000. Kill that process or change the port in `dashboard_server.py` (last line: `socketio.run(app, port=5000)`)

### Can't connect to MQTT broker
**Error:** `Failed to connect to MQTT broker`
**Solution:** 
1. Make sure Mosquitto is running in WSL:
   ```bash
   sudo systemctl status mosquitto
   ```
2. If not running, start it:
   ```bash
   sudo systemctl start mosquitto
   ```

### Dashboard shows "Disconnected"
**Solution:** Make sure the dashboard server is running. Check the terminal for error messages.

### No messages appearing
**Solution:** 
1. Make sure your Pico W is connected and sending data
2. Make sure MQTT-SN Gateway is running in WSL
3. Check that `receive_blocks.py` can receive messages (test it separately)

---

## Running Both Scripts

You can run both `receive_blocks.py` (to save images to disk) AND the dashboard at the same time:

**Terminal 1:**
```bash
python receive_blocks.py
```

**Terminal 2:**
```bash
python dashboard_server.py
```

Both will receive the same MQTT messages and work independently.

---

## Stopping the Server

Press `Ctrl+C` in the terminal where the dashboard server is running.

---

## Advanced Configuration

### Change MQTT Broker Address
Edit `dashboard_server.py` and change:
```python
MQTT_BROKER = "localhost"  # Change to your broker IP
MQTT_PORT = 1883           # Change if using different port
```

### Change Dashboard Port
Edit the last line of `dashboard_server.py`:
```python
socketio.run(app, host='0.0.0.0', port=5000)  # Change 5000 to your port
```

### Access from Other Devices
The dashboard is accessible from other devices on your network at:
```
http://YOUR_COMPUTER_IP:5000
```

To find your IP:
- Windows: `ipconfig` (look for IPv4 Address)
- Linux/Mac: `ifconfig` or `ip addr`

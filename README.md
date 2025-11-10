# 2004-MQTT
### 1. Configure WSL Networking
Open Powershell and edit WSL config file (or create if doesn't exist):

    notepad C:\Users\<user>\.wslconfig

Add:

    [wsl2]
    networkingMode=mirrored

Then restart WSL:

    wsl --shutdown

### 2. Verify Networking
Inside WSL, check IP (WSL IP should be same as laptop IP now):

    hostname -I

### 3. Disable Firewall (temporarily)
<span style="color: red;">** Remember to turn it back on after testing.</span>

Before flashing Pico, turn off Windows Firewall. Open Powershell as Administrator:

    Set-NetFirewallProfile -Profile Domain,Public,Private -Enabled False

### 4. Start Mosquitto Broker in WSL

    sudo systemctl start mosquitto
    sudo tail -f /var/log/mosquitto/mosquitto.log

### 5. Build and Run MQTT-SN Gateway

    cd lib/paho.mqtt-sn.embedded-c/MQTTSNGateway
    ./build.sh udp
    cd bin
    ./MQTT-SNGateway

### 6. Build and Flash Pico

Press GP22 to toggle QoS levels.

### 7. After testing, re-enable Firewall

    Set-NetFirewallProfile -Profile Domain,Public,Private -Enabled True


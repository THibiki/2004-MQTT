MQTT-SN Gateway (UDP -> MQTT)

This is a small gateway that listens for MQTT-SN messages over UDP and forwards them to a standard MQTT broker using Eclipse Paho C.

Default behavior:
- UDP listen port: 1884
- MQTT broker URI: tcp://localhost:1883

Build (Windows, using vcpkg + CMake):

1) Install vcpkg and Paho MQTT C
   - bootstrap vcpkg and install paho-mqtt-c (x64)

2) Configure and build with CMake. Example (PowerShell):
   PS> mkdir build; cd build
   PS> cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake
   PS> cmake --build . --config Release

Run (PowerShell):
   PS> .\Release\mqtt_sn_gateway.exe tcp://localhost:1883 1884

Notes:
- The gateway currently implements a minimal mapping: REGISTER assigns sequential topic ids and replies with REGACK. PUBLISH messages with QoS 0 are forwarded to the broker; QoS 1 sends a PUBACK back to the client immediately (no persistent retries).
- The gateway assumes a single client (or the last client address) for UDP replies. For multiple clients you should extend the mapping to track client addresses per client id.
- Update your Pico `main.c` GATEWAY_IP to your laptop IP (10.132.53.215) and GATEWAY_PORT to 1884 to connect to this gateway.

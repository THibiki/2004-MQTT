Component	Description	Responsibility	Tools / Libraries
Pico W MQTT-SN Client	Your main firmware running on Pico W	Implement CONNECT, REGISTER, PUBLISH, SUBSCRIBE, DISCONNECT over UDP	C/C++ with Pico SDK, lwIP UDP, WiFi
Gateway (Laptop)	Acts as translator between MQTT-SN and MQTT	Convert MQTT-SN messages to MQTT and vice versa	Use existing MQTT-SN Gateway (e.g. Eclipse Paho or mosquitto_sn_gateway)
Broker (Laptop)	Standard MQTT broker	Handle publish/subscribe messaging	Mosquitto
Dashboard (Laptop)	Visual interface to show received messages	Subscribes to MQTT broker	MQTT Explorer, Node-RED, or Python dashboard

Category	Feature	Explanation
Connection Handling	CONNECT, DISCONNECT	Establish and close UDP session with gateway
Topic Management	REGISTER, SUBSCRIBE, UNSUBSCRIBE	Allow Pico to register topic IDs and subscribe
Publishing	PUBLISH messages (QoS 0 and QoS 1)	Send data (e.g. sensor readings) to broker via gateway
QoS Support	QoS 0 (fire-and-forget), QoS 1 (with PUBACK)	Implement retransmissions and acknowledgements
Wi-Fi Handling	Auto-reconnect after loss (within 5 ± 2 s)	Follow NFR3
Feedback	LED blink on success	Follow NFR4
MicroSD File Handling (optional)	Read/write large data chunks (fragmentation supported)	Follows driver interface design
Latency Measurement	Measure round-trip delay & delivery success	Required in deliverables


Deliverable	Description
Working MQTT-SN Client on Pico W	Implements the stack (UDP, CONNECT, REGISTER, PUBLISH, SUBSCRIBE)
Functional Demo	End-to-end: Pico → Gateway → Broker → Dashboard
Latency & Delivery Success Report	Measure and record message delay and success rate


[Pico W] --UDP--> [Laptop MQTT-SN Gateway] --MQTT--> [Mosquitto Broker] --> [Dashboard Subscriber]

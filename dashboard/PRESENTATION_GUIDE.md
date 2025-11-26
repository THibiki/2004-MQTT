# Dashboard Presentation Guide
## INF2004 MQTT-SN Block Transfer Project

**Prepared for:** Professor Presentation  
**Date:** 2025-11-26  
**Project:** Real-Time MQTT Dashboard for Raspberry Pi Pico W

---

## Table of Contents
1. [Executive Summary](#executive-summary)
2. [System Architecture](#system-architecture)
3. [Technical Implementation](#technical-implementation)
4. [Key Features Demonstration](#key-features-demonstration)
5. [Code Walkthrough](#code-walkthrough)
6. [Demo Script](#demo-script)
7. [Technical Challenges & Solutions](#technical-challenges--solutions)
8. [Q&A Preparation](#qa-preparation)

---

## Executive Summary

### What is the Dashboard?
The dashboard is a **real-time web-based monitoring system** that visualizes data transmission from Raspberry Pi Pico W devices using MQTT-SN protocol. It provides a professional interface for monitoring IoT device communication, block transfer progress, and image reception.

### Why Did We Build It?
- **Visibility:** Move beyond serial terminal debugging to professional monitoring
- **Real-time feedback:** See data as it arrives with sub-100ms latency
- **Multi-user access:** Multiple team members can monitor simultaneously
- **Professional presentation:** Demonstrates production-ready IoT system

### Technology Stack
- **Backend:** Python (Flask + Flask-SocketIO + Paho MQTT)
- **Frontend:** HTML5, CSS3, JavaScript (Socket.IO client)
- **Protocol:** MQTT over TCP, WebSocket for browser communication
- **Platform:** Cross-platform (Windows, Linux, macOS)

---

## System Architecture

### Complete Data Flow
```
┌─────────────┐     UDP      ┌──────────────┐     TCP      ┌─────────────┐
│  Pico W     │─────────────>│   MQTT-SN    │─────────────>│  Mosquitto  │
│ (Publisher) │  MQTT-SN     │   Gateway    │     MQTT     │   Broker    │
└─────────────┘              └──────────────┘              └─────────────┘
                                                                    │
                                                                    │ TCP
                                                                    │ Port 1883
                                                                    ▼
                                                            ┌─────────────┐
                                                            │  Dashboard  │
                                                            │   Server    │
                                                            │  (Python)   │
                                                            └─────────────┘
                                                                    │
                                                                    │ WebSocket
                                                                    │ Port 5000
                                                                    ▼
                                                            ┌─────────────┐
                                                            │   Web       │
                                                            │  Browser    │
                                                            └─────────────┘
```

### Component Roles

**1. Pico W (Publisher)**
- Reads images from SD card
- Splits into 240-byte chunks with 8-byte headers
- Publishes via MQTT-SN over UDP

**2. MQTT-SN Gateway (Eclipse Paho)**
- Bridges UDP (MQTT-SN) ↔ TCP (MQTT)
- Handles protocol translation
- Maintains topic mappings

**3. Mosquitto Broker**
- Standard MQTT message broker
- Topic-based publish/subscribe
- Multiple client support

**4. Dashboard Server (Our Implementation)**
- **Dual role:** MQTT client + WebSocket server
- Subscribes to: `pico/chunks`, `pico/test`, `pico/block`
- Forwards messages to web clients in real-time
- Runs on port 5000

**5. Web Browser**
- Real-time UI updates via WebSocket
- No page refresh needed
- Visual progress tracking

---

## Technical Implementation

### Backend Architecture (`dashboard_server.py`)

#### Core Components

**1. Flask Application**
```python
app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*")
```
- Web server framework
- Serves HTML and handles HTTP routes
- CORS enabled for development flexibility

**2. MQTT Client**
```python
mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message
mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
```
- Connects to Mosquitto broker (localhost:1883)
- Event-driven callback architecture
- Auto-reconnect with 60s keepalive

**3. WebSocket Bridge**
```python
@socketio.on('connect')
def handle_connect():
    emit('mqtt_status', {'connected': mqtt_connected})
```
- Bidirectional communication with browser
- Real-time message forwarding
- Connection state synchronization

#### Message Processing Flow

**Topic: `pico/chunks` (Block Transfer Data)**
```python
if len(msg.payload) >= 8:
    # Parse 8-byte header
    block_id, part_num, total_parts, data_len = struct.unpack('<HHHH', msg.payload[:8])
    chunk_data = msg.payload[8:8+data_len]
    
    # Forward to browser
    socketio.emit('mqtt_message', {
        'topic': msg.topic,
        'block_id': block_id,
        'part_num': part_num,
        'total_parts': total_parts,
        'data': list(chunk_data)  # Convert bytes to JSON-compatible list
    })
```

**Header Format:**
- **block_id** (2 bytes): Unique transfer identifier
- **part_num** (2 bytes): Chunk sequence number (1-indexed)
- **total_parts** (2 bytes): Total chunks in transfer
- **data_len** (2 bytes): Payload size (up to 240 bytes)

**Topic: `pico/test` (Status Messages)**
- Plain text messages
- QoS level announcements
- General status updates

**Topic: `pico/block` (Completion Notifications)**
- Transfer completion messages
- Success/failure indicators

---

### Frontend Architecture (`dashboard.html`)

#### UI Layout Structure

```
┌──────────────────────────────────────────────────────────┐
│  Header: Status Indicator + Title + Connection Info     │
├──────────────────────┬───────────────────────────────────┤
│  Connection Stats    │  Block Transfer Progress          │
│  - Status            │  - Block ID                       │
│  - Uptime            │  - Progress Bar                   │
│  - Messages          │  - Chunks Received                │
│  - Current QoS       │  - Data Size                      │
├──────────────────────┴───────────────────────────────────┤
│  Message Log (Scrollable, Color-Coded)                   │
├───────────────────────────────────────────────────────────┤
│  Received Image Display (Auto-Resize)                    │
└───────────────────────────────────────────────────────────┘
```

#### Key JavaScript Functions

**1. WebSocket Connection Management**
```javascript
socket.on('connect', () => {
    log('✅ Connected to dashboard server', 'success');
    updateStatus(true);
    connectionStartTime = Date.now();
});
```

**2. Block Chunk Handling**
```javascript
function handleBlockChunk(data) {
    const blockId = data.block_id;
    const partNum = data.part_num;
    const totalParts = data.total_parts;
    
    // Store chunk
    currentBlock.parts[partNum] = new Uint8Array(data.data);
    
    // Calculate progress
    const received = Object.keys(currentBlock.parts).length;
    const progress = (received / totalParts * 100).toFixed(1);
    
    // Update UI
    document.getElementById('progressFill').style.width = `${progress}%`;
    
    // Check if complete
    if (received === totalParts) {
        assembleBlock();
    }
}
```

**3. Image Reassembly**
```javascript
function assembleBlock() {
    // Calculate total size
    let totalSize = 0;
    for (let i = 1; i <= totalParts; i++) {
        totalSize += parts[i].length;
    }
    
    // Concatenate chunks in order
    const imageData = new Uint8Array(totalSize);
    let offset = 0;
    for (let i = 1; i <= totalParts; i++) {
        imageData.set(parts[i], offset);
        offset += parts[i].length;
    }
    
    // Detect MIME type from magic bytes
    let mimeType = 'application/octet-stream';
    if (imageData[0] === 0xFF && imageData[1] === 0xD8) {
        mimeType = 'image/jpeg';  // JPEG magic: FF D8
    } else if (imageData[0] === 0x89 && imageData[1] === 0x50) {
        mimeType = 'image/png';   // PNG magic: 89 50
    }
    
    // Create and display image
    const blob = new Blob([imageData], { type: mimeType });
    const url = URL.createObjectURL(blob);
    container.innerHTML = `<img src="${url}" class="image-preview">`;
}
```

**4. Real-Time Statistics**
```javascript
setInterval(() => {
    if (connectionStartTime) {
        const elapsed = Math.floor((Date.now() - connectionStartTime) / 1000);
        const minutes = Math.floor(elapsed / 60);
        const seconds = elapsed % 60;
        document.getElementById('uptime').textContent = `${minutes}m ${seconds}s`;
    }
}, 1000);
```

---

## Key Features Demonstration

### 1. Connection Status Monitoring

**Visual Indicators:**
- **Pulsing green dot:** Connected and active
- **Static red dot:** Disconnected
- **Connection status text:** "Connected" / "Disconnected"
- **Uptime counter:** Real-time elapsed time

**What to Highlight:**
- Instant visual feedback on connection state
- Dual monitoring: WebSocket (dashboard ↔ browser) and MQTT (dashboard ↔ broker)
- Automatic reconnection detection

**Demo Action:**
```bash
# Stop dashboard server → Red indicator
# Restart dashboard server → Green indicator + connection log entry
```

---

### 2. Statistics Panel

**Metrics Displayed:**
- **Status:** Connected/Disconnected
- **Uptime:** Real-time counter (e.g., "5m 42s")
- **Messages Received:** Total count across all topics
- **Current QoS:** Last announced QoS level (0/1/2)

**What to Highlight:**
- QoS level updates when GP22 button pressed on Pico
- Message counter increments with every chunk received
- Professional metrics presentation

---

### 3. Block Transfer Progress

**Visual Elements:**
- **Block ID:** Unique identifier for current transfer
- **Progress Bar:** Animated gradient fill (0-100%)
- **Chunks Received:** "97 / 214" format
- **Data Size:** Total bytes in KB

**Real-Time Updates:**
- Progress bar fills smoothly with CSS transitions
- Log entries every 10 chunks or on completion
- Automatic reset 3 seconds after completion

**What to Highlight:**
- Can track multiple blocks sequentially
- Handles out-of-order chunk delivery
- Visual feedback for large transfers (200+ chunks)

**Demo Action:**
```
Press GP21 on Pico W → Watch progress bar fill from 0% to 100%
```

---

### 4. Message Log

**Color Coding:**
- **Blue (info):** General messages, chunk progress
- **Green (success):** Connections, completions
- **Yellow (warning):** Disconnections
- **Red (error):** Missing chunks, failures

**Features:**
- Topic badges (`pico/chunks`, `pico/test`, `pico/block`)
- Timestamps for each entry
- Auto-scroll to latest message
- Clear button for reset

**Example Log Entries:**
```
[14:23:15] ✅ Connected to dashboard server
[14:23:16] ✅ MQTT broker connected
[14:23:45] 🆕 Receiving block 1: 214 parts
[14:23:47] 📦 Progress: 100/214 chunks (46.7%)
[14:23:49] ✅ Block 1 complete: 50.15 KB received in 4.2s
```

**What to Highlight:**
- Easy debugging with detailed logs
- Historical record of session activity
- Professional formatting with emojis for clarity

---

### 5. Image Display

**Automatic Features:**
- **File type detection:** Reads JPEG/PNG/GIF magic bytes
- **Responsive sizing:** Scales to fit container
- **Immediate display:** Shows as soon as reassembly completes
- **Placeholder state:** Shows "Waiting for image transfer..." when idle

**Technical Details:**
- Uses Blob API for efficient binary handling
- Creates object URLs for in-browser rendering
- Maintains aspect ratio with CSS `object-fit: contain`

**What to Highlight:**
- No server-side file storage needed
- Pure client-side image rendering
- Supports multiple transfers in same session

---

## Code Walkthrough

### Backend Key Code Sections

#### 1. MQTT Connection Setup
```python
def start_mqtt_client():
    global mqtt_client
    
    mqtt_client = mqtt.Client()
    mqtt_client.on_connect = on_mqtt_connect
    mqtt_client.on_disconnect = on_mqtt_disconnect
    mqtt_client.on_message = on_mqtt_message
    
    try:
        print(f"📡 Connecting to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}...")
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_start()  # Non-blocking background thread
    except Exception as e:
        print(f"❌ Failed to connect to MQTT broker: {e}")
```

**Explain:**
- `loop_start()` runs MQTT client in background thread
- Flask runs in main thread
- Both operate concurrently without blocking

#### 2. Message Routing
```python
def on_mqtt_message(client, userdata, msg):
    try:
        if msg.topic == "pico/chunks":
            # Binary chunk processing
            block_id, part_num, total_parts, data_len = struct.unpack('<HHHH', msg.payload[:8])
            chunk_data = msg.payload[8:8+data_len]
            socketio.emit('mqtt_message', { ... })
        
        elif msg.topic == "pico/test":
            # Text message
            text = msg.payload.decode('utf-8', errors='ignore')
            socketio.emit('mqtt_message', {'topic': msg.topic, 'text': text})
    except Exception as e:
        print(f"❌ Error processing message: {e}")
```

**Explain:**
- Topic-based routing
- Binary vs. text payload handling
- Error handling prevents server crashes

#### 3. WebSocket Broadcasting
```python
@socketio.on('connect')
def handle_connect():
    print('🌐 Web client connected')
    emit('mqtt_status', {'connected': mqtt_connected})
```

**Explain:**
- `emit()` sends to single client
- `socketio.emit()` broadcasts to all clients
- State synchronization on new connections

---

### Frontend Key Code Sections

#### 1. Socket.IO Event Handling
```javascript
socket.on('mqtt_message', (data) => {
    messageCount++;
    document.getElementById('messageCount').textContent = messageCount;

    if (data.topic === 'pico/chunks') {
        handleBlockChunk(data);
    } else if (data.topic === 'pico/test') {
        // Extract QoS from message text
        const qosMatch = data.text.match(/QoS(\d)/);
        if (qosMatch) {
            document.getElementById('currentQos').textContent = qosMatch[1];
        }
        log(`<span class="topic-badge">pico/test</span> ${data.text}`, 'info');
    }
});
```

**Explain:**
- Regex pattern matching for QoS extraction
- Event-driven UI updates (no polling)
- Topic-specific handling

#### 2. Progress Calculation
```javascript
const received = Object.keys(currentBlock.parts).length;
const progress = (received / totalParts * 100).toFixed(1);

document.getElementById('chunksReceived').textContent = `${received} / ${totalParts}`;
document.getElementById('progressFill').style.width = `${progress}%`;
document.getElementById('progressFill').textContent = `${progress}%`;
```

**Explain:**
- Uses dictionary keys count for progress
- CSS width property for visual bar
- Fixed decimal precision for clean display

#### 3. Missing Chunk Detection
```javascript
const missing = [];
for (let i = 1; i <= totalParts; i++) {
    if (!parts[i]) missing.push(i);
}

if (missing.length > 0) {
    log(`❌ Missing chunks: ${missing.join(', ')}`, 'error');
    return;  // Don't assemble incomplete blocks
}
```

**Explain:**
- Sequential chunk verification
- Error reporting for debugging
- Prevents corrupt image display

---

## Demo Script

### Pre-Demo Checklist
- [ ] Mosquitto running in WSL: `sudo systemctl status mosquitto`
- [ ] MQTT-SN Gateway running: `cd lib/paho.mqtt-sn.embedded-c/MQTTSNGateway/bin && ./MQTT-SNGateway`
- [ ] Publisher Pico W connected with SD card (image loaded)
- [ ] Browser open (clear cache if needed)
- [ ] Terminal visible for server logs

---

### Step-by-Step Demo

#### **Part 1: System Startup (2 minutes)**

**Action 1: Start Dashboard Server**
```bash
cd dashboard
venv\Scripts\activate
python dashboard_server.py
```

**Expected Output:**
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

**Talking Points:**
- "The server connects to Mosquitto and subscribes to three topics"
- "It's now listening for both MQTT messages and WebSocket connections"

---

**Action 2: Open Browser**
```
Navigate to: http://localhost:5000
```

**Expected Behavior:**
- Green pulsing indicator appears
- Connection status shows "Connected"
- Message log shows: "✅ Connected to dashboard server"
- MQTT status banner shows: "Connected to Mosquitto broker"

**Talking Points:**
- "The browser establishes a WebSocket connection automatically"
- "Green indicator confirms two-way communication is active"
- "Notice the uptime counter started"

---

#### **Part 2: QoS Toggle Demonstration (1 minute)**

**Action: Press GP22 Button on Pico W**

**Expected Behavior:**
- Message log entry: `[pico/test] QoS=1` (or 2, 0 depending on cycle)
- "Current QoS" stat updates immediately
- Message counter increments

**Talking Points:**
- "The Pico publishes its QoS level to pico/test topic"
- "Dashboard extracts the QoS value using regex and updates the display"
- "This demonstrates real-time status monitoring"

---

#### **Part 3: Block Transfer (3 minutes)**

**Action: Press GP21 Button on Pico W**

**Expected Sequence:**

**1. Transfer Start (t=0s)**
- Log: "🆕 Receiving block 1: 214 parts"
- Block ID updates to "1"
- Progress bar starts at 0%

**Talking Point:**
- "The Pico starts sending 214 chunks of 240 bytes each"

**2. Progress Updates (t=1-3s)**
- Progress bar fills gradually: 10% → 25% → 50% → 75%
- Log: "📦 Progress: 100/214 chunks (46.7%)"
- Chunks counter: "150 / 214"

**Talking Point:**
- "Each chunk contains 8 bytes of metadata plus 240 bytes of image data"
- "The dashboard stores chunks in a dictionary keyed by part number"
- "Handles out-of-order delivery automatically"

**3. Completion (t=4s)**
- Progress bar reaches 100%
- Log: "✅ Block 1 complete: 50.15 KB received in 4.2s"
- Image appears in display area
- Data Size: "50.15 KB"

**Talking Point:**
- "When all chunks arrive, JavaScript reassembles them sequentially"
- "File type detected from magic bytes (FF D8 for JPEG)"
- "Image rendered directly in browser using Blob API"

**4. Reset (t=7s)**
- Progress bar resets to 0%
- Chunks counter: "0 / 0"
- Ready for next transfer

---

#### **Part 4: Architecture Explanation (2 minutes)**

**Show Terminal Logs Side-by-Side:**

**Pico Serial Output:**
```
Chunk 97/214 sent (240 bytes)
Chunk 98/214 sent (240 bytes)
```

**Gateway Log:**
```
PUBLISH from Pico_Publisher (topic: pico/chunks, QoS: 1)
```

**Dashboard Log:**
```
📦 Progress: 100/214 chunks (46.7%)
```

**Browser Message Log:**
```
[14:23:47] 📦 Progress: 100/214 chunks (46.7%)
```

**Talking Point:**
- "Data flows through four layers: Pico → Gateway → Broker → Dashboard → Browser"
- "Each layer uses appropriate protocol: UDP → TCP → WebSocket"
- "Total latency: typically under 100ms"

---

#### **Part 5: Multi-User Feature (1 minute)**

**Action: Open Second Browser Tab/Window**
```
http://localhost:5000
```

**Expected Behavior:**
- Second dashboard loads and shows current statistics
- Both dashboards update simultaneously during next transfer
- Server log: "🌐 Web client connected" (twice)

**Talking Point:**
- "WebSocket allows multiple concurrent connections"
- "All clients receive same data broadcast"
- "Useful for team monitoring or presentations"

---

## Technical Challenges & Solutions

### Challenge 1: Binary Data Over WebSocket

**Problem:**
- MQTT payloads are binary `bytes` objects
- WebSocket with Socket.IO uses JSON serialization
- JSON cannot directly encode binary data

**Solution:**
```python
# Server: Convert bytes to list of integers
chunk_data = msg.payload[8:8+data_len]
socketio.emit('mqtt_message', {
    'data': list(chunk_data)  # [255, 216, 255, 224, ...]
})
```

```javascript
// Client: Convert list back to Uint8Array
const chunkData = new Uint8Array(data.data);
```

**Why This Works:**
- JSON supports arrays of integers
- `Uint8Array` provides efficient binary operations in browser
- No data loss in conversion

---

### Challenge 2: Out-of-Order Chunk Delivery

**Problem:**
- UDP/MQTT does not guarantee ordered delivery
- Chunks may arrive: 1, 3, 2, 5, 4, 7, 6...
- Sequential assembly would fail

**Solution:**
```javascript
currentBlock.parts[partNum] = chunkData;  // Dictionary keyed by part number

// Reassemble in correct order
for (let i = 1; i <= totalParts; i++) {
    imageData.set(parts[i], offset);
    offset += parts[i].length;
}
```

**Why This Works:**
- Dictionary storage allows random access
- Explicit ordering during assembly phase
- Missing chunk detection before assembly

---

### Challenge 3: MIME Type Detection

**Problem:**
- Pico sends raw image bytes without file extension
- Browser needs MIME type for `<img>` rendering
- Different image formats have different requirements

**Solution:**
```javascript
// Read magic bytes (first 2-4 bytes)
let mimeType = 'application/octet-stream';
if (imageData[0] === 0xFF && imageData[1] === 0xD8) {
    mimeType = 'image/jpeg';  // JPEG: FF D8 FF
} else if (imageData[0] === 0x89 && imageData[1] === 0x50) {
    mimeType = 'image/png';   // PNG: 89 50 4E 47
} else if (imageData[0] === 0x47 && imageData[1] === 0x49) {
    mimeType = 'image/gif';   // GIF: 47 49 46 38
}

const blob = new Blob([imageData], { type: mimeType });
```

**Why This Works:**
- Magic bytes are file format identifiers
- Browser uses MIME type for correct rendering
- Supports multiple formats without hardcoding

---

### Challenge 4: Cross-Platform Networking

**Problem:**
- Pico W on WiFi (192.168.x.x)
- Mosquitto in WSL (127.0.0.1 internally)
- Dashboard on Windows (localhost)
- Need all components to communicate

**Solution:**
```ini
# .wslconfig
[wsl2]
networkingMode=mirrored
```

**Why This Works:**
- Mirrored mode shares same IP between WSL and Windows
- Pico sees Windows IP → reaches WSL services
- Dashboard localhost → reaches WSL Mosquitto
- Eliminates need for port forwarding

---

### Challenge 5: Memory Management

**Problem:**
- Large image transfers (50+ KB) consume browser memory
- Object URLs create memory references
- Multiple transfers cause memory leak

**Solution:**
```javascript
// Create image URL
const url = URL.createObjectURL(blob);
container.innerHTML = `<img src="${url}">`;

// Cleanup after display (optional, for frequent transfers)
setTimeout(() => {
    URL.revokeObjectURL(url);
}, 30000);  // Release after 30s
```

**Why This Works:**
- `createObjectURL` creates reference, not copy
- `revokeObjectURL` releases memory when no longer needed
- Browser garbage collection handles blob cleanup

---

## Q&A Preparation

### Expected Questions & Answers

#### **Q1: Why use WebSocket instead of regular HTTP polling?**

**Answer:**
"WebSocket provides real-time bidirectional communication with much lower latency than polling. With polling, we'd need to repeatedly send HTTP requests every second, creating overhead and delaying updates. WebSocket keeps a persistent connection open, allowing the server to push data instantly when it arrives from the Pico. For our block transfer scenario with 200+ chunks arriving in 4 seconds, WebSocket provides sub-100ms updates compared to potential 1-second delays with polling."

---

#### **Q2: What happens if a chunk is lost during transmission?**

**Answer:**
"The dashboard detects missing chunks before attempting image assembly. Here's the sequence:

```javascript
const missing = [];
for (let i = 1; i <= totalParts; i++) {
    if (!parts[i]) missing.push(i);
}
if (missing.length > 0) {
    log(`❌ Missing chunks: ${missing.join(', ')}`, 'error');
}
```

The missing chunks are logged in red, and the image is not displayed. However, with QoS 1 or 2, the Pico W's MQTT-SN client will automatically retransmit lost messages, so in practice, complete delivery is very reliable."

---

#### **Q3: Can the dashboard handle multiple Pico devices simultaneously?**

**Answer:**
"Currently, the dashboard processes blocks sequentially by Block ID. If two Picos send blocks simultaneously, the dashboard will switch between them as chunks arrive. For production use, we could enhance this by:

1. Maintaining separate state per device/client ID
2. Adding a device selector dropdown in the UI
3. Creating separate image display areas for each device

The MQTT infrastructure already supports multiple publishers, so the enhancement would be purely on the dashboard side."

---

#### **Q4: Why separate the dashboard server from the MQTT broker?**

**Answer:**
"This follows the separation of concerns principle:

- **Mosquitto (Broker):** Message routing and distribution - lightweight, battle-tested, minimal configuration
- **Dashboard (Application):** Business logic, data processing, UI serving - customizable to our needs

This architecture allows us to:
- Run multiple independent applications against the same broker
- Replace/upgrade dashboard without affecting broker
- Scale horizontally (multiple dashboard instances for different purposes)
- Use Mosquitto's robust features (QoS, persistence, authentication) without reimplementing them"

---

#### **Q5: What security considerations exist for this system?**

**Answer:**
"Current implementation prioritizes development ease, but for production we'd add:

**Authentication:**
- Mosquitto username/password authentication
- TLS/SSL for encrypted MQTT connections
- JWT tokens for WebSocket authentication

**Network Security:**
- Firewall rules limiting access to broker port
- VPN for remote access
- MQTT ACLs (Access Control Lists) to restrict topic access

**Code Security:**
- Input validation on received chunks
- Rate limiting to prevent DoS
- Content Security Policy headers for web dashboard

Our classroom environment uses a private WiFi network, providing basic isolation."

---

#### **Q6: How would you scale this for 100+ Pico devices?**

**Answer:**
"For large-scale deployment, we'd implement:

**Dashboard Enhancements:**
- Device registry with filtering/search
- Aggregated statistics view (average transfer time, success rate)
- Historical data storage (database like PostgreSQL)
- Alert system for anomalies

**Infrastructure:**
- Mosquitto clustering for high availability
- Load balancer for multiple dashboard instances
- Redis for shared state between dashboard instances
- Time-series database (InfluxDB) for metrics

**Monitoring:**
- Prometheus + Grafana for system metrics
- Error rate tracking and alerting
- Performance profiling for bottlenecks"

---

#### **Q7: What's the maximum image size you can transfer?**

**Answer:**
"Theoretical maximum depends on multiple factors:

**Chunk Header Constraints:**
- `total_parts` is uint16 (2 bytes) → max 65,535 chunks
- 65,535 chunks × 240 bytes = ~15 MB per block

**Practical Limits:**
- **Pico W RAM:** 264 KB SRAM limits in-memory buffering
- **SD Card Speed:** ~1-2 MB/s read speed
- **Network:** WiFi bandwidth ~10 Mbps effective
- **Browser:** JavaScript heap typically 512+ MB

**Current Implementation:**
- Tested with 50 KB images (~214 chunks) successfully
- Transfer time: ~4 seconds
- For larger images, we'd implement streaming to SD card on receiver side rather than full in-memory assembly"

---

#### **Q8: Why Python instead of Node.js or another language?**

**Answer:**
"Python was chosen for several reasons:

**Educational Context:**
- Course uses Python for scripts (`receive_blocks.py`)
- Team familiarity with Python syntax
- Rapid prototyping for academic deadline

**Library Ecosystem:**
- Paho MQTT: mature, well-documented Python library
- Flask: minimal boilerplate for web serving
- Flask-SocketIO: seamless WebSocket integration

**Alternative Considerations:**
- **Node.js:** Would be equally suitable, slightly lower latency
- **Go:** Better performance, but steeper learning curve
- **Rust:** Maximum performance, but unnecessary complexity for our scale

For a production IoT platform, we might choose Node.js for unified JavaScript front/back-end or Go for high-performance edge computing."

---

#### **Q9: How do you ensure data integrity during transmission?**

**Answer:**
"Multiple layers of integrity checking:

**Layer 1: UDP Checksum**
- Automatic at transport layer
- Detects corrupted packets (discarded if invalid)

**Layer 2: MQTT-SN Protocol**
- Message ID tracking
- QoS 1/2: Acknowledgments ensure delivery

**Layer 3: Application Layer (Future Enhancement)**
```c
// In Pico W sender:
uint16_t crc = calculate_crc16(chunk_data, data_len);
header[8] = crc;  // Add CRC to header

// In dashboard receiver:
const received_crc = data[8];
const calculated_crc = crc16(data.slice(9));
if (received_crc !== calculated_crc) {
    log('CRC mismatch - corrupted chunk', 'error');
}
```

**Layer 4: Image Validation**
- Magic byte verification
- JPEG/PNG libraries reject invalid formats
- Browser won't render corrupt images

Currently relying on Layers 1-2, but Layer 3 would add explicit chunk-level validation."

---

#### **Q10: What debugging capabilities does the dashboard provide?**

**Answer:**
"The dashboard serves as a powerful debugging tool:

**Real-Time Monitoring:**
- Message log shows every received chunk
- Color-coded entries (blue/green/yellow/red) for quick issue identification
- Timestamps for latency analysis

**Transfer Analytics:**
- Progress percentage shows stalled transfers
- Chunk counter reveals missing packets
- Transfer time measurement identifies slow network

**Connection Diagnostics:**
- Dual status indicators (WebSocket + MQTT)
- Automatic reconnection detection
- Uptime tracking for stability testing

**Developer Console:**
- Browser DevTools show WebSocket frames
- Network tab reveals payload sizes
- Console.log statements for deep debugging

Example debugging scenario:
```
Problem: Progress stuck at 90%
Dashboard shows: Chunks 193/214
Action: Check message log for errors
Finding: 'Missing chunks: 194, 195, 203, 208, ...'
Solution: Increase retry timeout in Pico QoS settings
```

This immediate visual feedback greatly speeds up development iteration."

---

## Closing Statement

"To summarize, our dashboard demonstrates a complete end-to-end IoT monitoring solution with:

✅ **Professional web-based interface** replacing basic serial terminal debugging  
✅ **Real-time visualization** with sub-100ms latency using WebSocket technology  
✅ **Robust protocol handling** supporting MQTT-SN, binary data, and image reassembly  
✅ **Multi-user capability** allowing simultaneous monitoring by team members  
✅ **Production-ready architecture** with separation of concerns and error handling  

The system successfully bridges embedded systems (Pico W), enterprise messaging (MQTT), and modern web technologies, showcasing practical skills in full-stack IoT development.

Thank you for your attention. I'm happy to answer any questions or provide a live demonstration."

---

## Appendix: Quick Reference

### Terminal Commands Cheat Sheet
```bash
# Start Mosquitto (WSL)
sudo systemctl start mosquitto

# Start MQTT-SN Gateway (WSL)
cd lib/paho.mqtt-sn.embedded-c/MQTTSNGateway/bin
./MQTT-SNGateway

# Start Dashboard (Windows)
cd dashboard
venv\Scripts\activate
python dashboard_server.py

# Monitor Mosquitto logs
sudo tail -f /var/log/mosquitto/mosquitto.log

# Test MQTT broker
mosquitto_sub -h localhost -t "#" -v
```

### Port Reference
| Service | Port | Protocol | Purpose |
|---------|------|----------|---------|
| Mosquitto | 1883 | TCP | MQTT broker |
| MQTT-SN Gateway | 1884 | UDP | MQTT-SN → MQTT bridge |
| Dashboard | 5000 | TCP | HTTP + WebSocket server |

### URL Quick Access
- **Dashboard:** http://localhost:5000
- **Mosquitto Broker:** localhost:1883 (MQTT clients only)

### File Locations
- **Dashboard Server:** `dashboard/dashboard_server.py`
- **Dashboard HTML:** `dashboard/dashboard.html`
- **Dependencies:** `dashboard/requirements.txt`
- **Setup Guide:** `dashboard/DASHBOARD_README.md`

---

**Document Version:** 1.0  
**Last Updated:** 2025-11-26  
**Author:** INF2004 Project Team

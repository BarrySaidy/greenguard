# GreenGuard 🌿

**An IoT-based plant health monitoring system that combines environmental sensors with AI-powered image classification to detect plant diseases in real-time.**

GreenGuard uses two linked senseBox Eye boards (ESP32-S3) to continuously monitor plant health through sensor data (temperature, humidity, light) and visual analysis using an Edge Impulse machine learning model. All data is logged to the cloud and accessible via a web dashboard.

---

## System Architecture

GreenGuard uses a **dual-sensor distributed architecture**:

```
┌─────────────────────────────────────────────────────────────┐
│                    GreenGuard System                          │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Eye 1 (Master / Brain)          Eye 2 (Slave / Vision)     │
│  ├─ HDC1080 (temp/humidity)      ├─ OV2640 Camera          │
│  ├─ LTR329 (light / lux)         ├─ Edge Impulse model     │
│  ├─ OLED display (4 screens)     │  (96x96 classifier)     │
│  ├─ HTTP server (dashboard)      └─ HTTP REST API          │
│  ├─ WiFi client                                             │
│  └─ Calls Eye 2 every 30s ──────────────────→              │
│     (classification + capture)                              │
│                                                              │
│  Both → Upload to openSenseMap every 60s                    │
│       → Show live data on web dashboard                     │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### System Flow

1. **Eye 1 (Master)**
   - Reads environmental sensors every 100ms (temp, humidity, light)
   - Every 30 seconds: Calls Eye 2's `/classify` endpoint
   - Parses classification result (healthy / early_sick / critical)
   - Displays on OLED screen (cycles 4 screens every 5 seconds)
   - Every 60 seconds: Uploads all sensor data + confidence to openSenseMap
   - Hosts web dashboard accessible via browser

2. **Eye 2 (Vision)**
   - Captures image from OV2640 camera
   - Runs Edge Impulse classifier (96x96 MobileNetV1)
   - Returns JSON: label, confidence, timestamp
   - Serves `/capture` endpoint with last JPEG image

3. **Cloud (openSenseMap)**
   - Receives temperature, humidity, light, confidence every 60s
   - Provides historical data and visualization

---

## Hardware Setup

### Components Required

| Component | Quantity | Notes |
|-----------|----------|-------|
| senseBox Eye (ESP32-S3) | 2 | One as Eye 1 (master), one as Eye 2 (vision) |
| HDC1080 (temp/humidity) | 1 | On Eye 1 only, over Qwiic |
| LTR329ALS (light sensor) | 1 | On Eye 1 only, over Qwiic |
| SSD1306 OLED 128x64 | 1 | On Eye 1 only, over Qwiic |
| OV2640 Camera Module | 1 | On Eye 2 only (built into senseBox Eye) |
| Qwiic Cable (4-pin) | 3+ | For daisy-chaining sensors on Eye 1 |
| USB-C Cable | 2 | For programming + power |
| WiFi Network | - | Both boards need WiFi connectivity |

### Qwiic Daisy Chain (Eye 1)

```
senseBox Eye 1 (Qwiic port)
├─ HDC1080 (0x40) - temperature & humidity
├─ LTR329ALS (0x29) - ambient light → **Connect LAST due to I2C timing**
└─ SSD1306 OLED (0x3D) - display
```

**I2C Addresses used:**
- `0x40` - HDC1080
- `0x29` - LTR329ALS (light sensor)
- `0x3D` - SSD1306 OLED
- (No ToF on current version)

### Wiring Diagram

See [docs/wiring.md](docs/wiring.md) for detailed pin assignments and schematics.

---

## Software Stack

| Component | Version | Purpose |
|-----------|---------|---------|
| Arduino IDE | 2.0+ | Programming environment |
| Board: esp32 by Espressif | 2.0+ | ESP32-S3 support |
| Adafruit GFX | Latest | OLED graphics library |
| Adafruit SSD1306 | Latest | OLED driver |
| Adafruit HDC1000 | Latest | Temperature/humidity sensor |
| Adafruit LTR329 | Latest | Light sensor library |
| ArduinoJson | 6.x | JSON parsing |
| Edge Impulse | Exported | ML model for plant classification |
| openSenseMap API | v0 | Cloud data logging |

---

## Project Structure

```
greenguard/
├── README.md                          # This file
├── LICENSE                            # Project license
├── firmware/
│   ├── eye1/
│   │   └── eye1.ino                   # Master board sketch (sensors + dashboard)
│   └── eye2/
│       └── eye2.ino                   # Vision board sketch (camera + classifier)
├── models/
│   └── greenguard-model-1/
│       ├── README.md                  # Model training details
│       └── [Edge Impulse exported Arduino library]
├── data/
│   └── plantvillage/                  # Training dataset (not tracked in git)
│       ├── healthy/
│       ├── early_sick/
│       └── critical/
└── docs/
    └── wiring.md                      # Hardware wiring guide
```

---

## Getting Started

### Prerequisites

1. **Two senseBox Eye boards** (ESP32-S3)
2. **Arduino IDE 2.0+** with ESP32 board support
3. **Adafruit libraries** (install via Library Manager):
   - Adafruit GFX Library
   - Adafruit SSD1306
   - Adafruit HDC1000
   - Adafruit LTR329 Light Sensor
   - ArduinoJson

4. **Edge Impulse model**:
   - Export from your Edge Impulse project as Arduino library
   - Place in `models/greenguard-model-1/`

5. **openSenseMap account**:
   - Register at https://opensensemap.org
   - Create a new senseBox (note the box ID)
   - Create 4 sensors: temperature, humidity, light (lux), confidence (%)
   - Get sensor IDs

### Installation Steps

#### 1. Setup Arduino Environment

```bash
# Install ESP32 board support
# Tools → Board Manager → Search "esp32" → Install "esp32 by Espressif Systems" (v2.0+)

# Install required libraries via Library Manager:
# Sketch → Include Library → Manage Libraries
# - Adafruit GFX Library
# - Adafruit SSD1306
# - Adafruit HDC1000
# - Adafruit LTR329 Light Sensor
# - ArduinoJson (by Benoit Blanchon)
```

#### 2. Export Edge Impulse Model

1. Go to your Edge Impulse project
2. **Deployment** → **Arduino Library** → Click **Build**
3. Download the `.zip` file
4. In Arduino IDE: **Sketch → Include Library → Add .ZIP Library...**
5. Select the downloaded file
6. Note the library name (e.g., `GreenGuard-Model-1_inferencing`)

#### 3. Configure Eye 2 (Vision Board)

**File:** `firmware/eye2/eye2.ino`

Update these constants:

```cpp
const char* WIFI_SSID     = "Your_WiFi_SSID";
const char* WIFI_PASSWORD = "Your_WiFi_Password";

// Set static IP for Eye 2 (must be different from Eye 1)
IPAddress EYE2_IP (192, 168, 178, 21);  // e.g., 192.168.178.21
IPAddress GATEWAY (192, 168, 178,  1);
IPAddress SUBNET  (255, 255, 255,  0);
IPAddress DNS     (8, 8, 8, 8);
```

**Upload:**
1. Plug in Eye 2 board via USB-C
2. Select **Tools → Board → esp32 → senseBox MCU 2.1** (or similar)
3. Select **Tools → Port → COM#** (your board)
4. **Sketch → Upload**
5. Open Serial Monitor (115200 baud) to verify boot

Expected output:
```
=== GreenGuard Eye 2 booting ===
Camera initialised OK
Connecting to WiFi: Your_WiFi_SSID
WiFi connected
Eye 2 IP: 192.168.178.21
HTTP server started
Eye 2 ready and waiting.
```

#### 4. Configure Eye 1 (Master Board)

**File:** `firmware/eye1/eye1.ino`

Update these constants:

```cpp
const char* WIFI_SSID     = "Your_WiFi_SSID";
const char* WIFI_PASSWORD = "Your_WiFi_Password";

// Static IP for Eye 1 (must be different from Eye 2)
IPAddress EYE1_IP (192, 168, 178, 20);  // e.g., 192.168.178.20
IPAddress GATEWAY (192, 168, 178,  1);
IPAddress SUBNET  (255, 255, 255,  0);
IPAddress DNS     (8, 8, 8, 8);

// Eye 2 endpoints
const char* EYE2_CLASSIFY = "http://192.168.178.21/classify";
const char* EYE2_CAPTURE  = "http://192.168.178.21/capture";
const char* EYE2_HEALTH   = "http://192.168.178.21/health";

// openSenseMap staging API credentials
const char* OSM_BOX_ID        = "your_box_id_from_opensensemap";
const char* OSM_TEMP_SENSOR   = "your_temp_sensor_id";
const char* OSM_HUMID_SENSOR  = "your_humidity_sensor_id";
const char* OSM_LIGHT_SENSOR  = "your_light_sensor_id";
const char* OSM_CONFIDENCE_SENSOR = "your_confidence_sensor_id";
const char* OSM_AUTH_KEY      = "your_box_id";  // Usually same as BOX_ID
```

**Upload:**
1. Plug in Eye 1 board via USB-C
2. Select same board and port
3. **Sketch → Upload**
4. Open Serial Monitor (115200 baud) to verify boot

Expected output:
```
=== GreenGuard Eye 1 (Master) booting ===
HDC1080 OK
LTR329 OK
OLED OK
Connecting to: Your_WiFi_SSID
WiFi connected
Eye 1 IP: 192.168.178.20
Checking Eye 2...
Eye 2: online
HTTP server started
=== Eye 1 ready ===
  Dashboard: http://192.168.178.20/
  Status:    http://192.168.178.20/status
```

#### 5. Connect Hardware

1. **Eye 1 Qwiic chain:**
   - Qwiic port 1 → HDC1080
   - Qwiic port 2 → LTR329 (light sensor) - **Connect this LAST**
   - Qwiic port 3 → SSD1306 OLED

2. **Power both boards** via USB-C

3. **Verify on Serial Monitor** that all sensors initialize

---

## Usage

### Web Dashboard

**Access:** `http://192.168.178.20/` (or Eye 1's actual IP)

Shows:
- Live temperature, humidity, light levels
- Plant health verdict (HEALTHY / NEEDS ATTENTION / CRITICAL)
- Confidence percentage
- Last classification timestamp
- Plant image from Eye 2 camera
- Refresh button

### REST API Endpoints

**Eye 1 (Master) - `http://192.168.178.20`:**

| Endpoint | Method | Response | Interval |
|----------|--------|----------|----------|
| `/` | GET | HTML dashboard | - |
| `/status` | GET | JSON (all sensor + classification data) | - |

**Eye 2 (Vision) - `http://192.168.178.21`:**

| Endpoint | Method | Response | Notes |
|----------|--------|----------|-------|
| `/classify` | GET | JSON (label, confidence, timestamp) | Runs inference ~2-5s |
| `/capture` | GET | JPEG binary | Returns last captured image |
| `/health` | GET | JSON (status check) | Quick response |

**Example:** `curl http://192.168.178.20/status`

```json
{
  "temperature": 23.5,
  "humidity": 65.2,
  "light": 450,
  "verdict": "HEALTHY",
  "eye2_label": "healthy",
  "eye2_confidence": 0.92,
  "eye2_uncertain": false,
  "eye2_timestamp": "12:34:56",
  "eye2_alive": true,
  "eye2_image": "http://192.168.178.21/capture",
  "eye1_ip": "192.168.178.20",
  "uptime_s": 3600
}
```

### OLED Display (Eye 1)

Cycles 4 screens every 5 seconds:

**Screen 1: Verdict**
```
  ┌──────────────────────┐
  │   GreenGuard         │
  │        😊            │
  │    HEALTHY           │
  └──────────────────────┘
```

**Screen 2: Environment**
```
  ┌──────────────────────┐
  │   Environment        │
  │ Temp:  23.5 C        │
  │ Humid: 65.2 %        │
  │ Light: 450 lux       │
  │ T:OK H:OK L:OK       │
  └──────────────────────┘
```

**Screen 3: Plant Eye**
```
  ┌──────────────────────┐
  │    Plant Eye         │
  │ Result: healthy      │
  │ Conf:  92%           │
  │ [██████████░░░]      │
  │ At: 12:34:56         │
  └──────────────────────┘
```

**Screen 4: System**
```
  ┌──────────────────────┐
  │     System           │
  │ Eye1: 192.168.178.20 │
  │ Eye2: online         │
  │ Cloud: 45s ago       │
  └──────────────────────┘
```

### Cloud Logging (openSenseMap)

Data uploaded every 60 seconds:
- Temperature (°C)
- Humidity (%)
- Light level (lux)
- Classification confidence (%)

View at: https://api.staging.opensensemap.org (staging) or https://api.opensensemap.org (production)

---

## Troubleshooting

### Eye 2 Not Detected

**Symptom:** Serial says `Eye 2: offline`

**Fix:**
1. Verify Eye 2 is booted and shows `HTTP server started`
2. Check Eye 1 can ping Eye 2: On your computer, run `ping 192.168.178.21`
3. Verify both are on the same WiFi network
4. Check static IPs don't conflict: `192.168.178.20` (Eye 1) vs `192.168.178.21` (Eye 2)

### LTR329 (Light Sensor) Not Detected

**Symptom:** Serial says `WARNING: LTR329 not found at 0x29`

**Fixes:**
1. **Check Qwiic cable connection** — ensure tight fit
2. **Reorder daisy chain** — try connecting LTR329 AFTER other sensors (not immediately after ESP32)
3. **Replace cable** — Qwiic cables can be faulty
4. **Inspect pins** — look for bent or corroded contacts
5. If still fails, the sensor may be defective

### Camera Not Initializing (Eye 2)

**Symptom:** Serial says `FATAL: Camera failed`

**Fixes:**
1. Check camera module is fully seated in the CSI connector
2. Try disconnecting other sensors on Eye 2
3. Restart the board
4. Replace camera module if still failing

### Can't Access Web Dashboard

**Symptom:** Browser shows `took too long to respond` or `connection refused`

**Fixes:**
1. **Check Eye 1 is online:** Serial should show `Eye 1 ready`
2. **Verify correct IP:** Check serial monitor for actual IP (might not be 192.168.178.20 on hotspot/different networks)
3. **Same WiFi network:** Make sure your computer is on the same WiFi as Eye 1
4. **No firewall blocking:** Check if network firewall blocks port 80

### Confidence Upload Failing

**Symptom:** Serial says `openSenseMap staging: error`

**Fixes:**
1. Verify `OSM_AUTH_KEY` matches your senseBox ID
2. Check all sensor IDs are correct in code
3. Verify you created 4 sensors in openSenseMap (temp, humidity, light, confidence)
4. Test connection on another device to rule out local network issues

---

## Customization

### Change Classification Interval

**File:** `firmware/eye1/eye1.ino`

```cpp
const unsigned long CLASSIFY_INTERVAL = 30000;  // Change from 30s to whatever
```

### Change Cloud Upload Interval

**File:** `firmware/eye1/eye1.ino`

```cpp
const unsigned long OSM_INTERVAL = 60000;  // Change from 60s to whatever
```

### Change OLED Screen Cycle Time

**File:** `firmware/eye1/eye1.ino`

```cpp
const unsigned long OLED_CYCLE = 5000;  // Change from 5s to whatever
```

### Use Production openSenseMap API

**File:** `firmware/eye1/eye1.ino`

In `uploadToOpenSenseMap()`, change:

```cpp
const char* server = "api.opensensemap.org";  // was api.staging.opensensemap.org
```

---

## Model Training

The machine learning model (`GreenGuard-Model-1`) was trained on:

- **Dataset:** PlantVillage (Tomato subset)
- **Classes:** healthy, early_sick, critical
- **Training images:** 120 per class (360 total)
- **Test images:** 30 per class (90 total)
- **Architecture:** MobileNetV1 96x96 0.25
- **Input:** 96×96 RGB images
- **Framework:** Edge Impulse

To retrain the model with your own data:
1. Create a new Edge Impulse project
2. Upload training images in folders: `healthy/`, `early_sick/`, `critical/`
3. Create impulse with transfer learning (MobileNetV1)
4. Train model
5. Deploy as Arduino library
6. Replace the library in this project

---

## Project Status

- [x] Phase 1 — Dataset + Edge Impulse model
- [x] Phase 2 — senseBox Eye 2 firmware (vision)
- [x] Phase 3 — senseBox Eye 1 firmware (master + sensors)
- [x] Phase 4 — Integration + system test
- [ ] Phase 5 — Mobile app (future)
- [ ] Phase 6 — Multi-plant dashboard (future)

---

## License

See [LICENSE](LICENSE) file

---

## Contributing

To improve this project:
1. Test changes on hardware
2. Document any new features
3. Update this README with setup changes
4. Submit improvements via pull request

---

## Support

For issues or questions:
1. Check **Troubleshooting** section above
2. Review **Serial Monitor output** for error messages
3. Verify all hardware connections
4. Test individual components (sensors, camera) separately

---

**Happy plant monitoring! 🌱💚**

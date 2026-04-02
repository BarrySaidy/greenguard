#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vl53l8cx.h>
#include "Adafruit_HDC1000.h"

// ================= DISPLAY CONFIG =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3D
#define PROXIMITY_THRESHOLD 500  // mm — triggers display wake

// ================= WIFI CONFIG =================
const char* ssid = "Apartment 79.02.03";
const char* password = "45516787831950213141";

// ================= SSL CERTIFICATE (ISRG Root X1) =================
const char* root_ca =
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "-----END CERTIFICATE-----\n";

// ================= OPENSENSEMAP CONFIG =================
const char* SENSEBOX_ID = "c38e9z8vz5x4lkqe165zidtl";
const char* SENSOR_TEMP = "d765420e812ffd5c03ec0caf";
const char* SENSOR_HUMI = "a410b7ffcdb5ada4a6b561da";
const char* AUTHORIZATION_KEY = "mDzPEgu4OJ9IXlEgyOMNTY8tXR74XDXuSAk2WKWEjb0";
const char* server = "api.staging.opensensemap.org";

const unsigned long uploadInterval = 30000;  // 30 seconds
unsigned long lastUpload = 0;

// ================= OBJECTS =================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_HDC1000 hdc = Adafruit_HDC1000();
VL53L8CX sensor_tof(&Wire, -1, -1);
WiFiClientSecure client;
WebServer webServer(80);

// ================= STATE =================
float lastDistance = 0;
float lastTemp = 0;
float lastHum = 0;
bool wifiConnected = false;

// ================= WEB PAGE =================
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Green Guard</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: #fff;
    color: #222;
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 40px 24px;
  }
  h1 { font-size: 1.5rem; color: #346739; margin-bottom: 4px; }
  .subtitle { font-size: 0.8rem; color: #999; margin-bottom: 36px; }
  .grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 14px;
    width: 100%;
    max-width: 960px;
  }
  .card {
    background: #f8f8f8;
    border-radius: 12px;
    padding: 20px;
  }
  .card-label {
    font-size: 0.7rem;
    text-transform: uppercase;
    letter-spacing: 1px;
    color: #999;
    margin-bottom: 10px;
  }
  .card-value {
    font-size: 2.2rem;
    font-weight: 600;
    color: #346739;
  }
  .card-unit { font-size: 0.85rem; color: #999; margin-left: 2px; }
  .presence .card-value { font-size: 1.3rem; }
  .present { color: #346739; }
  .away { color: #bbb; }
  .status-bar {
    margin-top: 36px;
    display: flex;
    gap: 20px;
    font-size: 0.75rem;
    color: #bbb;
  }
  .dot {
    display: inline-block;
    width: 6px; height: 6px;
    border-radius: 50%;
    margin-right: 4px;
    background: #346739;
    animation: pulse 2s infinite;
  }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.3} }
  @media (max-width: 800px) { .grid { grid-template-columns: repeat(2,1fr); } }
  @media (max-width: 480px) { .grid { grid-template-columns: 1fr; } }
</style>
</head>
<body>
  <h1>Green Guard</h1>
  <p class="subtitle">Plant environment monitoring - live sensor data</p>
  <div class="grid">
    <div class="card">
      <div class="card-label">Temperature</div>
      <div class="card-value"><span id="temp">--</span><span class="card-unit">&deg;C</span></div>
    </div>
    <div class="card">
      <div class="card-label">Humidity</div>
      <div class="card-value"><span id="hum">--</span><span class="card-unit">%</span></div>
    </div>
    <div class="card">
      <div class="card-label">Distance</div>
      <div class="card-value"><span id="dist">--</span><span class="card-unit">mm</span></div>
    </div>
    <div class="card">
      <div class="card-label">Presence</div>
      <div class="card-value" id="presence">--</div>
    </div>
  </div>
  <div class="status-bar">
    <span><span class="dot"></span>Live</span>
    <span>Updates every 2s</span>
    <span id="lastUpdate"></span>
  </div>
<script>
  function fetchData() {
    fetch('/api/data')
      .then(r => r.json())
      .then(d => {
        document.getElementById('temp').textContent = d.temperature.toFixed(1);
        document.getElementById('hum').textContent = d.humidity.toFixed(1);
        document.getElementById('dist').textContent = d.distance.toFixed(0);
        const el = document.getElementById('presence');
        if (d.presence) {
          el.textContent = 'Someone nearby';
          el.className = 'card-value present';
        } else {
          el.textContent = 'No one detected';
          el.className = 'card-value away';
        }
        document.getElementById('lastUpdate').textContent =
          'Last: ' + new Date().toLocaleTimeString();
      })
      .catch(() => {});
  }
  fetchData();
  setInterval(fetchData, 2000);
</script>
</body>
</html>
)rawliteral";

// ================= WEB SERVER HANDLERS =================
void handleRoot() {
  webServer.send(200, "text/html", webpage);
}

void handleApi() {
  String json = "{\"temperature\":" + String(lastTemp, 2)
              + ",\"humidity\":" + String(lastHum, 2)
              + ",\"distance\":" + String(lastDistance, 1)
              + ",\"presence\":" + String((lastDistance > 0 && lastDistance < PROXIMITY_THRESHOLD) ? "true" : "false")
              + "}";
  webServer.send(200, "application/json", json);
}

// ================= WIFI =================
void initWiFi() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.println(ssid);
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
    display.println("Connected!");
    display.println(WiFi.localIP().toString());
  } else {
    wifiConnected = false;
    Serial.println("\nWiFi failed - offline mode");
    display.println("WiFi failed!");
    display.println("Running offline...");
  }
  display.display();
  delay(1500);
}

// ================= OPENSENSEMAP UPLOAD =================
void uploadToOSeM() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost -> reconnect");
    WiFi.reconnect();
    delay(3000);
    return;
  }

  if (client.connected()) client.stop();

  Serial.println("Uploading to openSenseMap...");

  if (!client.connect(server, 443)) {
    Serial.println("SSL connect failed");
    return;
  }

  // Build CSV body
  String body = String(SENSOR_TEMP) + "," + String(lastTemp, 2) + "\n"
              + String(SENSOR_HUMI) + "," + String(lastHum, 2);

  // HTTP request
  client.printf(
    "POST /boxes/%s/data HTTP/1.1\r\n"
    "Authorization: %s\r\n"
    "Host: %s\r\n"
    "Content-Type: text/csv\r\n"
    "Connection: close\r\n"
    "Content-Length: %d\r\n\r\n",
    SENSEBOX_ID, AUTHORIZATION_KEY, server, body.length()
  );
  client.print(body);

  // Read response
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
    if (line.startsWith("HTTP")) Serial.println(line);
  }
  while (client.available()) {
    Serial.write(client.read());
  }
  Serial.println("\nUpload done");

  client.stop();
}

// ================= TOF DISTANCE =================
float getMinDistance() {
  static float oldDistance = 0;
  VL53L8CX_ResultsData Results;
  uint8_t ready = 0;

  sensor_tof.check_data_ready(&ready);
  if (ready) {
    sensor_tof.get_ranging_data(&Results);
    float minDist = 10000.0;
    for (int i = 0; i < 64 * VL53L8CX_NB_TARGET_PER_ZONE; i++) {
      if (Results.target_status[i] != 255 && Results.distance_mm[i] > 0) {
        if (Results.distance_mm[i] < minDist) {
          minDist = Results.distance_mm[i];
        }
      }
    }
    oldDistance = (minDist == 10000.0) ? 0 : minDist;
  }
  return oldDistance;
}

// ================= DISPLAY: SENSOR DATA =================
void showSensorData() {
  display.clearDisplay();

  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("    Green Guard");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Temperature
  display.setCursor(0, 16);
  display.print("Temperature:");
  display.setTextSize(2);
  display.setCursor(0, 28);
  display.print(lastTemp, 1);
  display.setTextSize(1);
  display.print(" C");

  // Humidity
  display.setCursor(0, 48);
  display.print("Humidity:");
  display.setTextSize(2);
  display.setCursor(64, 48);
  display.print(lastHum, 0);
  display.setTextSize(1);
  display.print(" %");

  display.display();
}

// ================= DISPLAY: SCREENSAVER =================
void showScreensaver() {
  display.clearDisplay();

  // Centered "Green Guard"
  display.setTextSize(1);
  display.setCursor(31, 26);
  display.print("Green Guard");

  // IP or status centered below
  display.setTextSize(1);
  if (wifiConnected) {
    String ip = WiFi.localIP().toString();
    int16_t ipX = (SCREEN_WIDTH - ip.length() * 6) / 2;
    display.setCursor(ipX, 46);
    display.print(ip);
  } else {
    display.setCursor(40, 46);
    display.print("Offline");
  }

  display.display();
}

// ================= SETUP =================
void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(400000);

  // Init OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ERROR: OLED not found!");
    while (1);
  }
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("  Green Guard");
  display.println("Starting sensors...");
  display.display();

  // Init HDC1000
  if (!hdc.begin()) {
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("HDC1000 not found!");
    display.display();
    Serial.println("ERROR: HDC1000 not found!");
    while (1);
  }

  // Init VL53L8CX
  sensor_tof.begin();
  sensor_tof.init();
  sensor_tof.set_resolution(VL53L8CX_RESOLUTION_8X8);
  sensor_tof.set_ranging_frequency_hz(15);
  sensor_tof.start_ranging();

  // Init WiFi
  initWiFi();
  client.setCACert(root_ca);

  // Start web server
  if (wifiConnected) {
    webServer.on("/", handleRoot);
    webServer.on("/api/data", handleApi);
    webServer.begin();
    Serial.println("Web server started: http://" + WiFi.localIP().toString());
  }

  display.clearDisplay();
  display.setCursor(0, 10);
  display.println("Green Guard Ready!");
  if (wifiConnected) {
    display.println();
    display.print("Web: ");
    display.println(WiFi.localIP().toString());
  }
  display.display();
  delay(2000);
}

// ================= LOOP =================
void loop() {
  // Handle web requests
  if (wifiConnected) {
    webServer.handleClient();
  }

  lastDistance = getMinDistance();

  // Read sensor data periodically
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 2000) {
    lastRead = millis();
    lastTemp = hdc.readTemperature();
    lastHum = hdc.readHumidity();

    Serial.print("Temp: ");
    Serial.print(lastTemp, 1);
    Serial.print(" C  Hum: ");
    Serial.print(lastHum, 1);
    Serial.print(" %  Dist: ");
    Serial.print(lastDistance);
    Serial.println(" mm");
  }

  // Display: proximity-based
  if (lastDistance > 0 && lastDistance < PROXIMITY_THRESHOLD) {
    showSensorData();
    delay(1000);
  } else {
    showScreensaver();
    delay(50);
  }

  // Upload to openSenseMap every 30s
  if (wifiConnected && millis() - lastUpload > uploadInterval) {
    lastUpload = millis();
    uploadToOSeM();
  }
}

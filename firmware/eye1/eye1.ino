/*
 * GreenGuard — senseBox Eye 1 (Master / Brain Node)
 *
 * Responsibilities:
 *   - Read environmental sensors over I2C Qwiic daisy chain
 *     HDC1080   (0x40) → temperature + humidity
 *     LTR329ALS (0x29) → visible light in lux
 *     OLED      (0x3D) → 4-screen dashboard
 *   - Call Eye 2 GET /classify every 30s over WiFi
 *   - Parse verdict and confidence from Eye 2
 *   - Drive OLED 4-screen cycling dashboard (always on)
 *   - Host HTTP server:
 *       GET /        → HTML browser dashboard
 *       GET /status  → JSON with all sensor + vision data
 *   - Upload to openSenseMap every 60s
 *
 * Hardware:  senseBox Eye (ESP32-S3)
 * I2C Pins:  SDA=2, SCL=1 (senseBox Eye Qwiic)
 * Light:     senseBox LTR329ALS + VEML6070 combo board
 */

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Adafruit_HDC1000.h>
#include <Adafruit_LTR329_LTR303.h>

// ─────────────────────────────────────────────
// CONFIGURATION — edit before flashing
// ─────────────────────────────────────────────

const char* WIFI_SSID     = ""; // Your WiFi SSID
const char* WIFI_PASSWORD = ""; // Your WiFi password

// Static IP for Eye 1
IPAddress EYE1_IP (192, 168, 178, 20); // Set this to your desired static IP for Eye 1
IPAddress GATEWAY (192, 168, 178,  1); // Your network gateway (usually your router's IP)
IPAddress SUBNET  (255, 255, 255,  0); // Your network subnet mask
IPAddress DNS     (8, 8, 8, 8);

// Eye 2 endpoints
const char* EYE2_CLASSIFY = "http://192.168.178.21/classify";
const char* EYE2_CAPTURE  = "http://192.168.178.21/capture";
const char* EYE2_HEALTH   = "http://192.168.178.21/health";

// openSenseMap staging — register at opensensemap.org and paste IDs here
const char* OSM_BOX_ID        = "";  // Your senseBox ID
const char* OSM_TEMP_SENSOR   = "";  // Your temperature sensor ID
const char* OSM_HUMID_SENSOR  = "";  // Your humidity sensor ID
const char* OSM_LIGHT_SENSOR  = ""; // Your light sensor ID
const char* OSM_CONFIDENCE_SENSOR = ""; // Your confidence sensor ID
const char* OSM_AUTH_KEY      = "";  // Your authorization key

// SSL Certificate for api.staging.opensensemap.org (ISRG Root X1)
const char* root_ca =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
    "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
    "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
    "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
    "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
    "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
    "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
    "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
    "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
    "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
    "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
    "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
    "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
    "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
    "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
    "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
    "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
    "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
    "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
    "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
    "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
    "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
    "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
    "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
    "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
    "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
    "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
    "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
    "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
    "-----END CERTIFICATE-----\n";

// Timing intervals
const unsigned long CLASSIFY_INTERVAL  = 30000;  // 30s
const unsigned long OSM_INTERVAL       = 60000;  // 60s
const unsigned long OLED_CYCLE         =  5000;  // 5s per screen

// ─────────────────────────────────────────────
// I2C + DISPLAY + SENSORS
// ─────────────────────────────────────────────

#define PIN_SDA       2
#define PIN_SCL       1
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS  0x3D

Adafruit_SSD1306  display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_HDC1000  hdc;
Adafruit_LTR329   ltr;

// ─────────────────────────────────────────────
// GLOBAL STATE
// ─────────────────────────────────────────────

// Sensor readings
float g_temperature = 0.0;
float g_humidity    = 0.0;
float g_light       = 0.0;

// Eye 2 classification result
String g_label      = "waiting...";
float  g_confidence = 0.0;
bool   g_uncertain  = false;
String g_raw_label  = "waiting...";
String g_timestamp  = "never";
bool   g_eye2_alive = false;

// OLED state
int           oled_screen      = 0;
unsigned long oled_last_cycle  = 0;

// Timing trackers
unsigned long last_classify   = 0;
unsigned long last_osm_upload = 0;

// HTTP server
WebServer server(80);

// ─────────────────────────────────────────────
// SENSOR INITIALISATION
// ─────────────────────────────────────────────

void initSensors() {
  Wire.begin(PIN_SDA, PIN_SCL);

  // HDC1080 — temperature and humidity
  if (!hdc.begin()) {
    Serial.println("WARNING: HDC1080 not found at 0x40");
  } else {
    Serial.println("HDC1080 OK");
  }

  // LTR329ALS — visible light (I2C address 0x29)
  if (!ltr.begin()) {
    Serial.println("WARNING: LTR329 not found at 0x29");
  } else {
    ltr.setGain(LTR3XX_GAIN_1);
    ltr.setIntegrationTime(LTR3XX_INTEGTIME_100);
    ltr.setMeasurementRate(LTR3XX_MEASRATE_200);
    Serial.println("LTR329 OK");
  }



  // OLED SSD1306
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("WARNING: OLED not found at 0x3D");
  } else {
    display.setRotation(2);
    display.clearDisplay();
    display.display();
    Serial.println("OLED OK");
  }
}

// ─────────────────────────────────────────────
// READ SENSORS
// ─────────────────────────────────────────────

void readSensors() {
  // Temperature and humidity
  g_temperature = hdc.readTemperature();
  g_humidity    = hdc.readHumidity();

  // Visible light from LTR329 — direct read
  bool     valid;
  uint16_t visible = 0, infrared = 0;
  valid = ltr.readBothChannels(visible, infrared);
  if (valid) {
    g_light = (float)visible;
    Serial.printf("LTR329 OK: visible=%u lux\n", visible);
  } else {
    Serial.println("LTR329 read FAILED, retrying...");
    delay(110);
    valid = ltr.readBothChannels(visible, infrared);
    if (valid) {
      g_light = (float)visible;
      Serial.printf("LTR329 retry OK: visible=%u lux\n", visible);
    } else {
      Serial.println("LTR329 read FAILED after retry — keeping g_light at 0");
    }
  }



  Serial.printf("Temp: %.1f°C  Humid: %.1f%%  Light: %.0f\n",
    g_temperature, g_humidity, g_light);
}

// ─────────────────────────────────────────────
// OVERALL VERDICT
// ─────────────────────────────────────────────

String getOverallVerdict() {
  bool temp_ok  = (g_temperature >= 15.0 && g_temperature <= 32.0);
  bool humid_ok = (g_humidity    >= 30.0 && g_humidity    <= 80.0);
  bool light_ok = (g_light       >= 100.0);
  bool env_ok   = temp_ok && humid_ok && light_ok;

  if (g_label == "critical")              return "CRITICAL";
  if (g_label == "early_sick" || !env_ok) return "NEEDS ATTENTION";
  if (g_label == "healthy"    && env_ok)  return "HEALTHY";
  if (g_label == "uncertain")             return "UNCERTAIN";
  return "MONITORING";
}

String getVerdictEmoji(String verdict) {
  if (verdict == "HEALTHY")          return ":)";
  if (verdict == "NEEDS ATTENTION")  return ":/";
  if (verdict == "CRITICAL")         return "!!";
  if (verdict == "UNCERTAIN")        return "??";
  return "..";
}

// ─────────────────────────────────────────────
// OLED SCREENS
// ─────────────────────────────────────────────

void drawScreen1() {
  String verdict = getOverallVerdict();
  String emoji   = getVerdictEmoji(verdict);

  display.clearDisplay();
  display.fillRect(0, 0, 128, 12, WHITE);
  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(28, 2);
  display.println("GreenGuard");

  display.setTextColor(WHITE);
  display.setTextSize(3);
  display.setCursor(48, 18);
  display.println(emoji);

  display.setTextSize(1);
  int16_t x, y; uint16_t w, h;
  display.getTextBounds(verdict, 0, 0, &x, &y, &w, &h);
  display.setCursor((128 - w) / 2, 52);
  display.println(verdict);
  display.display();
}

void drawScreen2() {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 12, WHITE);
  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(22, 2);
  display.println("Environment");

  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 16);
  display.print("Temp:  ");
  display.print(g_temperature, 1);
  display.println(" C");

  display.setCursor(0, 28);
  display.print("Humid: ");
  display.print(g_humidity, 1);
  display.println(" %");

  display.setCursor(0, 40);
  display.print("Light: ");
  display.print(g_light, 0);
  display.println(" lux");

  display.setCursor(0, 54);
  bool temp_ok  = (g_temperature >= 15.0 && g_temperature <= 32.0);
  bool humid_ok = (g_humidity    >= 30.0 && g_humidity    <= 80.0);
  bool light_ok = (g_light       >= 100.0);
  display.print(temp_ok  ? "T:OK " : "T:!! ");
  display.print(humid_ok ? "H:OK " : "H:!! ");
  display.print(light_ok ? "L:OK"  : "L:!!");
  display.display();
}

void drawScreen3() {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 12, WHITE);
  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(28, 2);
  display.println("Plant Eye");

  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 16);
  display.print("Result: ");
  display.println(g_label);

  display.setCursor(0, 30);
  display.print("Conf:  ");
  display.print(g_confidence * 100, 0);
  display.println("%");

  int barWidth = (int)(g_confidence * 100);
  display.drawRect(0, 40, 100, 8, WHITE);
  display.fillRect(0, 40, barWidth, 8, WHITE);

  display.setCursor(0, 54);
  if (g_uncertain) {
    display.println("* Low confidence");
  } else {
    display.print("At: ");
    display.println(g_timestamp);
  }
  display.display();
}

void drawScreen4() {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 12, WHITE);
  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(28, 2);
  display.println("System");

  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(0, 16);
  display.print("Eye1: ");
  display.println(WiFi.localIP().toString());

  display.setCursor(0, 28);
  display.print("Eye2: ");
  display.println(g_eye2_alive ? "online" : "offline");

  display.setCursor(0, 40);
  display.print("Cloud: ");
  unsigned long since = (millis() - last_osm_upload) / 1000;
  display.print(since);
  display.println("s ago");

  display.display();
}

void updateOLED() {
  unsigned long now = millis();
  if (now - oled_last_cycle >= OLED_CYCLE) {
    oled_last_cycle = now;
    oled_screen = (oled_screen + 1) % 4;
  }
  switch (oled_screen) {
    case 0: drawScreen1(); break;
    case 1: drawScreen2(); break;
    case 2: drawScreen3(); break;
    case 3: drawScreen4(); break;
  }
}



// ─────────────────────────────────────────────
// EYE 2 CLASSIFICATION
// ─────────────────────────────────────────────

void callEye2() {
  Serial.println("Calling Eye 2 /classify...");
  HTTPClient http;
  http.begin(EYE2_CLASSIFY);
  http.setTimeout(10000);
  int code = http.GET();

  if (code == 200) {
    String payload = http.getString();
    Serial.println("Eye 2: " + payload);

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      g_label      = doc["label"].as<String>();
      g_confidence = doc["confidence"].as<float>();
      g_uncertain  = doc["uncertain"].as<bool>();
      g_raw_label  = doc["raw_label"].as<String>();
      g_timestamp  = doc["timestamp"].as<String>();
      g_eye2_alive = true;
      Serial.printf("Verdict: %s (%.0f%%)\n",
        g_label.c_str(), g_confidence * 100);
    } else {
      Serial.println("JSON parse error");
    }
  } else {
    Serial.printf("Eye 2 error: %d\n", code);
    g_eye2_alive = false;
  }
  http.end();
}

// ─────────────────────────────────────────────
// openSenseMap UPLOAD
// ─────────────────────────────────────────────

void uploadToOpenSenseMap() {
  Serial.println("Uploading to openSenseMap staging...");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost -> reconnect");
    WiFi.reconnect();
    delay(3000);
    return;
  }

  WiFiClientSecure client;
  client.setCACert(root_ca);

  const char* server = "api.staging.opensensemap.org";
  if (!client.connect(server, 443)) {
    Serial.println("SSL connect failed");
    return;
  }

  // Build CSV payload: sensorId,value\n
  String body = "";
  body += OSM_TEMP_SENSOR;       body += "," + String(g_temperature, 1) + "\n";
  body += OSM_HUMID_SENSOR;      body += "," + String(g_humidity, 1)    + "\n";
  body += OSM_LIGHT_SENSOR;      body += "," + String(g_light, 0)       + "\n";
  body += OSM_CONFIDENCE_SENSOR; body += "," + String(g_confidence * 100, 1);

  // Send HTTP POST request with proper headers
  client.printf(
    "POST /boxes/%s/data HTTP/1.1\r\n"
    "Authorization: %s\r\n"
    "Host: %s\r\n"
    "Content-Type: text/csv\r\n"
    "Connection: close\r\n"
    "Content-Length: %d\r\n\r\n",
    OSM_BOX_ID, OSM_AUTH_KEY, server, body.length()
  );

  client.print(body);
  Serial.printf("Sent to staging API - body: %d bytes\n", body.length());

  // Read response headers until blank line
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  // Read response body
  while (client.available()) {
    String response = client.readStringUntil('\n');
    Serial.println("Response: " + response);
  }

  client.stop();
}

// ─────────────────────────────────────────────
// HTTP — /status JSON
// ─────────────────────────────────────────────

void handleStatus() {
  String verdict = getOverallVerdict();
  String json = "{";
  json += "\"temperature\":"     + String(g_temperature, 1)              + ",";
  json += "\"humidity\":"        + String(g_humidity, 1)                 + ",";
  json += "\"light\":"           + String(g_light, 0)                    + ",";
  json += "\"verdict\":\""       + verdict                               + "\",";
  json += "\"eye2_label\":\""    + g_label                               + "\",";
  json += "\"eye2_confidence\":" + String(g_confidence, 2)              + ",";
  json += "\"eye2_uncertain\":"  + String(g_uncertain ? "true":"false")  + ",";
  json += "\"eye2_timestamp\":\"" + g_timestamp                         + "\",";
  json += "\"eye2_alive\":"      + String(g_eye2_alive ? "true":"false") + ",";
  json += "\"eye2_image\":\""    + String(EYE2_CAPTURE)                  + "\",";
  json += "\"eye1_ip\":\""       + WiFi.localIP().toString()             + "\",";
  json += "\"uptime_s\":"        + String(millis() / 1000);
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// ─────────────────────────────────────────────
// HTTP — / HTML Dashboard
// ─────────────────────────────────────────────

void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>GreenGuard Dashboard</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: sans-serif; background: #1a3a1b; color: #f0f0f0; padding: 16px; }
  h1 { color: #97BC62; font-size: 1.6em; margin-bottom: 4px; }
  .sub { color: #aaa; font-size: 0.85em; margin-bottom: 20px; }
  .verdict-box { background: #2C5F2D; border-radius: 12px; padding: 20px;
                 text-align: center; margin-bottom: 16px; }
  .verdict-label { font-size: 2em; font-weight: bold; color: #fff; }
  .emoji { font-size: 3em; }
  .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 16px; }
  .card { background: #243a25; border-radius: 10px; padding: 14px; }
  .card h3 { color: #97BC62; font-size: 0.85em; margin-bottom: 10px;
             text-transform: uppercase; letter-spacing: 1px; }
  .reading { font-size: 1.4em; font-weight: bold; }
  .unit { font-size: 0.75em; color: #aaa; }
  .ok  { color: #97BC62; }
  .bad { color: #ff6b6b; }
  .camera-section { background: #243a25; border-radius: 10px;
                    padding: 14px; margin-bottom: 16px; }
  .camera-section h3 { color: #97BC62; font-size: 0.85em; margin-bottom: 10px;
                        text-transform: uppercase; letter-spacing: 1px; }
  .camera-img { width: 192px; height: 192px; border-radius: 8px;
                background: #1a3a1b; display: block;
                image-rendering: pixelated;
                margin: 0 auto; }
  .conf-bar-bg { background: #1a3a1b; border-radius: 4px; height: 8px; margin-top: 8px; }
  .conf-bar { background: #97BC62; border-radius: 4px; height: 8px; }
  .refresh-btn { display: block; width: 100%; padding: 12px;
                 background: #2C5F2D; color: #fff; border: none;
                 border-radius: 8px; font-size: 1em; cursor: pointer;
                 margin-bottom: 12px; }
  .footer { color: #666; font-size: 0.75em; text-align: center; margin-top: 12px; }
</style>
</head>
<body>
<h1>🌿 GreenGuard</h1>
<p class="sub">Live Plant Health Monitor — updates every 30s</p>

<div class="verdict-box">
  <div class="emoji" id="emoji">🌿</div>
  <div class="verdict-label" id="verdict">Loading...</div>
</div>

<div class="grid">
  <div class="card">
    <h3>🌡 Temperature</h3>
    <div class="reading" id="temp">--</div>
    <div class="unit">°C</div>
    <div id="temp-s" class="unit"></div>
  </div>
  <div class="card">
    <h3>💧 Humidity</h3>
    <div class="reading" id="humid">--</div>
    <div class="unit">%</div>
    <div id="humid-s" class="unit"></div>
  </div>
  <div class="card">
    <h3>☀️ Light</h3>
    <div class="reading" id="light">--</div>
    <div class="unit">lux</div>
    <div id="light-s" class="unit"></div>
  </div>
  <div class="card">
    <h3>👁 Vision</h3>
    <div class="reading" id="eye-label">--</div>
    <div class="unit" id="eye-conf"></div>
    <div class="conf-bar-bg">
      <div class="conf-bar" id="conf-bar" style="width:0%"></div>
    </div>
  </div>
</div>

<div class="camera-section">
  <h3>📷 Last Plant Image</h3>
  <img id="plant-img" class="camera-img" src="" alt="Waiting for first scan...">
  <div class="unit" style="margin-top:6px" id="img-ts"></div>
</div>

<button class="refresh-btn" onclick="load()">Refresh Now</button>
<div class="footer" id="footer">Connecting...</div>

<script>
function emoji(v) {
  if (v==="HEALTHY")         return "😊";
  if (v==="NEEDS ATTENTION") return "😐";
  if (v==="CRITICAL")        return "🚨";
  if (v==="UNCERTAIN")       return "🤔";
  return "🌿";
}
async function load() {
  try {
    const d = await (await fetch("/status")).json();
    document.getElementById("verdict").textContent = d.verdict;
    document.getElementById("emoji").textContent   = emoji(d.verdict);
    document.getElementById("temp").textContent    = d.temperature.toFixed(1);
    document.getElementById("humid").textContent   = d.humidity.toFixed(1);
    document.getElementById("light").textContent   = d.light;

    const tOk = d.temperature>=15 && d.temperature<=32;
    const hOk = d.humidity>=30    && d.humidity<=80;
    const lOk = d.light>=100;

    document.getElementById("temp-s").textContent  = tOk ? "✓ Normal":"⚠ Out of range";
    document.getElementById("temp-s").className    = "unit "+(tOk?"ok":"bad");
    document.getElementById("humid-s").textContent = hOk ? "✓ Normal":"⚠ Out of range";
    document.getElementById("humid-s").className   = "unit "+(hOk?"ok":"bad");
    document.getElementById("light-s").textContent = lOk ? "✓ Normal":"⚠ Too dark";
    document.getElementById("light-s").className   = "unit "+(lOk?"ok":"bad");

    document.getElementById("eye-label").textContent = d.eye2_label;
    const pct = Math.round(d.eye2_confidence*100);
    document.getElementById("eye-conf").textContent  = pct+"% confidence";
    document.getElementById("conf-bar").style.width  = pct+"%";

    if (d.eye2_alive) {
      document.getElementById("plant-img").src = d.eye2_image+"?t="+Date.now();
      document.getElementById("img-ts").textContent = "Captured at "+d.eye2_timestamp;
    }

    const m = Math.floor(d.uptime_s/60), s = d.uptime_s%60;
    document.getElementById("footer").textContent =
      "Eye1: "+d.eye1_ip+" | Eye2: "+(d.eye2_alive?"online":"offline")+
      " | Uptime: "+m+"m "+s+"s";
  } catch(e) {
    document.getElementById("verdict").textContent = "Connection error";
  }
}
load();
setInterval(load, 30000);
</script>
</body>
</html>
)rawhtml";
  server.send(200, "text/html", html);
}

void handleNotFound() {
  server.send(404, "text/plain",
    "GreenGuard Eye 1\n\nEndpoints:\n  GET /\n  GET /status\n");
}

// ─────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(2000);
  Serial.println("\n=== GreenGuard Eye 1 (Master) booting ===");

  // 1. Sensors + OLED
  initSensors();

  // Splash screen
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(20, 22);
  display.println("GreenGuard");
  display.setCursor(28, 36);
  display.println("Starting...");
  display.display();

  // 2. WiFi
  Serial.printf("Connecting to: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  if (!WiFi.config(EYE1_IP, GATEWAY, SUBNET, DNS)) {
    Serial.println("WARNING: Static IP failed — using DHCP");
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("FATAL: WiFi failed");
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("WiFi failed!");
    display.display();
    while (true) delay(1000);
  }

  Serial.print("Eye 1 IP: ");
  Serial.println(WiFi.localIP());

  // 3. Check Eye 2
  Serial.println("Checking Eye 2...");
  HTTPClient http;
  http.begin(EYE2_HEALTH);
  http.setTimeout(5000);
  int code = http.GET();
  g_eye2_alive = (code == 200);
  http.end();
  Serial.printf("Eye 2: %s\n", g_eye2_alive ? "online" : "offline");

  // 4. HTTP server routes
  server.on("/",       HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  // 5. Initial readings
  readSensors();
  callEye2();
  last_classify    = millis();
  last_osm_upload  = millis();

  Serial.println("=== Eye 1 ready ===");
  Serial.printf("  Dashboard: http://%s/\n",       WiFi.localIP().toString().c_str());
  Serial.printf("  Status:    http://%s/status\n", WiFi.localIP().toString().c_str());
}

// ─────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────

void loop() {
  unsigned long now = millis();

  server.handleClient();
  readSensors();
  updateOLED();

  // Call Eye 2 every 30s
  if (now - last_classify >= CLASSIFY_INTERVAL) {
    last_classify = now;
    callEye2();
  }

  // Upload to openSenseMap every 60s
  if (now - last_osm_upload >= OSM_INTERVAL) {
    last_osm_upload = now;
    uploadToOpenSenseMap();
  }

  delay(100);
}

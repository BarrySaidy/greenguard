/*
 * GreenGuard — senseBox Eye 2 (Slave / Vision Node)
 *
 * Responsibilities:
 *   - Connect to WiFi with static IP
 *   - Serve GET /classify → capture image, run inference, return JSON
 *   - Serve GET /capture  → return last captured JPEG image
 *   - Serve GET /health   → confirm Eye 2 is alive
 *
 * Hardware: senseBox Eye (ESP32-S3)
 * Model:    GreenGuard-Model-1 (healthy / early_sick / critical)
 *
 * Pin definitions verified against senseBox Eye seminar sketch.
 */

#include "esp_camera.h"
#include "img_converters.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <GreenGuard-Model-1_inferencing.h>

// ─────────────────────────────────────────────
// CONFIGURATION — edit these before flashing
// ─────────────────────────────────────────────

const char* WIFI_SSID     = "";
const char* WIFI_PASSWORD = "";

// ─────────────────────────────────────────────
// senseBox Eye Pin Definitions (verified from seminar sketch)
// ─────────────────────────────────────────────

#define PIN_QWIIC_SDA 2
#define PIN_QWIIC_SCL 1

#define PWDN_GPIO_NUM   46
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   15
#define SIOD_GPIO_NUM    4   // sccb_sda
#define SIOC_GPIO_NUM    5   // sccb_scl
#define Y9_GPIO_NUM     16
#define Y8_GPIO_NUM     17
#define Y7_GPIO_NUM     18
#define Y6_GPIO_NUM     12
#define Y5_GPIO_NUM     10
#define Y4_GPIO_NUM      8
#define Y3_GPIO_NUM      9
#define Y2_GPIO_NUM     11
#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    7
#define PCLK_GPIO_NUM   13

// ─────────────────────────────────────────────
// GLOBALS
// ─────────────────────────────────────────────

WebServer server(80);

// Last classification result
String last_label      = "unknown";
float  last_confidence = 0.0;
bool   last_uncertain  = false;
String last_timestamp  = "never";

// Last captured JPEG stored in heap for /capture endpoint
uint8_t* last_jpeg_buf = nullptr;
size_t   last_jpeg_len = 0;

// Frame buffer pointer used during inference callback
static camera_fb_t* inference_frame = nullptr;

// ─────────────────────────────────────────────
// CAMERA INITIALISATION
// ─────────────────────────────────────────────

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size   = FRAMESIZE_96X96;
  config.jpeg_quality = 12;
  config.fb_count     = 1;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  Serial.println("Camera initialised OK");
  return true;
}

// ─────────────────────────────────────────────
// EDGE IMPULSE IMAGE CALLBACK
// ─────────────────────────────────────────────

/*
 * Edge Impulse calls this function to retrieve pixel data.
 * offset and length are in pixels.
 * We convert RGB565 to 0xRRGGBB packed into a float.
 */
int ei_camera_get_data(size_t offset, size_t length, float* out_ptr) {
  uint16_t* buf = (uint16_t*)inference_frame->buf;

  for (size_t i = 0; i < length; i++) {
    uint16_t pixel = buf[offset + i];
    // RGB565: RRRRRGGGGGGBBBBB — extract and scale to 8-bit
    uint8_t r = ((pixel >> 11) & 0x1F) << 3;
    uint8_t g = ((pixel >> 5)  & 0x3F) << 2;
    uint8_t b = (pixel         & 0x1F) << 3;
    out_ptr[i] = (r << 16) | (g << 8) | b;
  }
  return 0;
}

// ─────────────────────────────────────────────
// INFERENCE
// ─────────────────────────────────────────────

String runInference() {
  Serial.println("Capturing image for inference...");

  // Capture fresh frame
  inference_frame = esp_camera_fb_get();
  if (!inference_frame) {
    Serial.println("Camera capture failed");
    return "{\"label\":\"error\",\"confidence\":0.0,\"uncertain\":true,"
           "\"error\":\"capture failed\",\"timestamp\":\"" + last_timestamp + "\"}";
  }

  // Convert RGB565 frame to JPEG for /capture endpoint
  if (last_jpeg_buf != nullptr) {
    free(last_jpeg_buf);
    last_jpeg_buf = nullptr;
    last_jpeg_len = 0;
  }
  frame2jpg(inference_frame, 80, &last_jpeg_buf, &last_jpeg_len);

  // Build Edge Impulse signal
  ei::signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
  signal.get_data     = &ei_camera_get_data;

  // Run classifier
  ei_impulse_result_t result = { 0 };
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);

  // Return frame buffer immediately
  esp_camera_fb_return(inference_frame);
  inference_frame = nullptr;

  if (err != EI_IMPULSE_OK) {
    Serial.printf("Classifier error code: %d\n", err);
    return "{\"label\":\"error\",\"confidence\":0.0,\"uncertain\":true,"
           "\"error\":\"inference failed\",\"timestamp\":\"" + last_timestamp + "\"}";
  }

  // Find highest confidence class
  float  best_val   = 0.0;
  String best_label = "unknown";

  Serial.println("Classification results:");
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    Serial.printf("  %-12s %.4f\n",
      result.classification[i].label,
      result.classification[i].value);
    if (result.classification[i].value > best_val) {
      best_val   = result.classification[i].value;
      best_label = String(result.classification[i].label);
    }
  }

  // Flag low confidence as uncertain
  last_uncertain  = (best_val < 0.50);
  last_label      = last_uncertain ? "uncertain" : best_label;
  last_confidence = best_val;

  // Build uptime timestamp HH:MM:SS
  unsigned long s = millis() / 1000;
  char ts[10];
  sprintf(ts, "%02lu:%02lu:%02lu", (s / 3600) % 24, (s / 60) % 60, s % 60);
  last_timestamp = String(ts);

  // Build JSON response
  String json = "{";
  json += "\"label\":\""     + last_label                              + "\",";
  json += "\"confidence\":"  + String(last_confidence, 2)              + ",";
  json += "\"uncertain\":"   + String(last_uncertain ? "true":"false") + ",";
  json += "\"raw_label\":\"" + best_label                              + "\",";
  json += "\"timestamp\":\"" + last_timestamp                          + "\"";
  json += "}";

  Serial.println("Result: " + json);
  return json;
}

// ─────────────────────────────────────────────
// HTTP HANDLERS
// ─────────────────────────────────────────────

// GET /classify
void handleClassify() {
  Serial.println("\nGET /classify");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", runInference());
}

// GET /capture — serves last JPEG from /classify
void handleCapture() {
  Serial.println("GET /capture");
  if (last_jpeg_buf == nullptr || last_jpeg_len == 0) {
    server.send(404, "text/plain",
      "No image available yet. Call /classify first.");
    return;
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");
  server.sendHeader("Content-Length", String(last_jpeg_len));
  server.send_P(200, "image/jpeg",
    (const char*)last_jpeg_buf, last_jpeg_len);
}

// GET /health — Eye 1 calls this on boot to confirm Eye 2 is alive
void handleHealth() {
  String json = "{";
  json += "\"status\":\"ok\",";
  json += "\"device\":\"greenguard-eye2\",";
  json += "\"ip\":\""           + WiFi.localIP().toString()    + "\",";
  json += "\"uptime_ms\":"      + String(millis())             + ",";
  json += "\"last_label\":\""   + last_label                   + "\",";
  json += "\"last_confidence\":" + String(last_confidence, 2);
  json += "}";
  server.send(200, "application/json", json);
}

// 404
void handleNotFound() {
  server.send(404, "text/plain",
    "GreenGuard Eye 2\n\n"
    "Available endpoints:\n"
    "  GET /classify  — capture image and run inference\n"
    "  GET /capture   — retrieve last captured JPEG\n"
    "  GET /health    — device status\n");
}

// ─────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== GreenGuard Eye 2 booting ===");

  // 1. Initialise camera
  if (!initCamera()) {
    Serial.println("FATAL: Camera failed. Check pin definitions.");
    while (true) delay(1000);
  }

  // 2. Connect to WiFi with static IP
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("FATAL: WiFi failed. Check SSID and password.");
    while (true) delay(1000);
  }

  Serial.println("WiFi connected");
  Serial.print("Eye 2 IP: ");
  Serial.println(WiFi.localIP());

  // Start mDNS — Eye 2 reachable at greenguard-eye2.local on any network
  if (MDNS.begin("greenguard-eye2")) {
    Serial.println("mDNS: http://greenguard-eye2.local");
  } else {
    Serial.println("WARNING: mDNS failed");
  }

  // 3. Register HTTP routes
  server.on("/classify", HTTP_GET, handleClassify);
  server.on("/capture",  HTTP_GET, handleCapture);
  server.on("/health",   HTTP_GET, handleHealth);
  server.onNotFound(handleNotFound);

  // 4. Start server
  server.begin();

  Serial.println("HTTP server started");
  Serial.println("─────────────────────────────");
  Serial.println("  GET /classify → run inference");
  Serial.println("  GET /capture  → last JPEG");
  Serial.println("  GET /health   → status");
  Serial.println("─────────────────────────────");
  Serial.println("Eye 2 ready and waiting.");
}

// ─────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────

void loop() {
  server.handleClient();
}

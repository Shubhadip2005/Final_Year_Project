/*
 * ═════════════════════════════════════════════════════════════════════
 * RF ANTENNA AUTOMATION SYSTEM - ESP32-CAM MODULE
 * ═════════════════════════════════════════════════════════════════════
 * 
 * Based on WORKING camera code + API endpoints for automation system
 * Uses proven camera initialization that works!
 * 
 * ═════════════════════════════════════════════════════════════════════
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ═════════════════════════════════════════════════════════════════════
// ⚙️ CONFIGURATION
// ═════════════════════════════════════════════════════════════════════

const char* WIFI_SSID = "OPPO Reno10 Pro 5G";
const char* WIFI_PASSWORD = "12233344445";

// STATIC IP FOR ESP32-CAM
IPAddress staticIP(10, 135, 98, 51);      
IPAddress gateway(10, 135, 98, 1);         
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(10, 135, 98, 5);
IPAddress secondaryDNS(8, 8, 8, 8);
const char* OCR_API_URL = "https://antenna-ocr-api.onrender.com/extract-ocr";

// ═════════════════════════════════════════════════════════════════════
// 📷 CAMERA PIN CONFIGURATION (AI THINKER ESP32-CAM)
// ═════════════════════════════════════════════════════════════════════

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ═════════════════════════════════════════════════════════════════════
// 🖥️ WEB SERVER
// ═════════════════════════════════════════════════════════════════════

WebServer server(80);

// ═════════════════════════════════════════════════════════════════════
// 📊 STATE
// ═════════════════════════════════════════════════════════════════════

String systemStatus = "Initializing";
float lastExtractedValue = 0.0;

// ═════════════════════════════════════════════════════════════════════
// 🎯 SETUP
// ═════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n╔════════════════════════════════════════════════════╗");
  Serial.println("║  📷 ESP32-CAM MODULE - PRODUCTION VERSION         ║");
  Serial.println("║  (Based on proven working code)                   ║");
  Serial.println("╚════════════════════════════════════════════════════╝\n");
  
  // Initialize Camera (PROVEN WORKING CONFIG)
  initializeCamera();
  
  // Connect WiFi with Static IP
  connectToWiFi();
  
  // Setup Web Server endpoints
  setupWebServer();
  
  server.begin();
  Serial.println("[SERVER] ✓ Camera API server started on port 80");
  Serial.println("[SYSTEM] Ready to capture and extract!\n");
}

// ═════════════════════════════════════════════════════════════════════
// 🔄 MAIN LOOP
// ═════════════════════════════════════════════════════════════════════

void loop() {
  server.handleClient();
  delay(1);
}

// ═════════════════════════════════════════════════════════════════════
// 📷 CAMERA INITIALIZATION (PROVEN WORKING)
// ═════════════════════════════════════════════════════════════════════

void initializeCamera() {
  Serial.println("[CAMERA] Initializing ESP32-CAM...");
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;       // ← IMPORTANT: This was missing!
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;         // 20 MHz - works well
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;      // 640x480 - good balance
  config.jpeg_quality = 12;               // Good quality
  config.fb_count = 1;
  
  Serial.println("[CAMERA] Calling esp_camera_init()...");
  
  esp_err_t err = esp_camera_init(&config);
  
  if (err != ESP_OK) {
    Serial.printf("[CAMERA] ✗ Init failed with error 0x%x\n", err);
    systemStatus = "Camera Init Failed";
    return;
  }
  
  Serial.println("[CAMERA] ✓ Initialized successfully!");
  systemStatus = "Ready";
}

// ═════════════════════════════════════════════════════════════════════
// 📡 WIFI CONNECTION WITH STATIC IP
// ═════════════════════════════════════════════════════════════════════

void connectToWiFi() {
  Serial.println("[WIFI] Configuring static IP...");
  Serial.println("[WIFI] Static IP: 192.168.1.51 (ALWAYS the same!)");
  
  if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("[WIFI] ✗ Failed to configure static IP");
  }
  
  Serial.println("[WIFI] Connecting to WiFi...");
  Serial.printf("[WIFI] SSID: %s\n", WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WIFI] ✓ Connected!");
    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("[WIFI] ⭐ IP is ALWAYS 192.168.1.51");
    Serial.println("[WIFI] ⭐ No need to check Serial Monitor!");
    systemStatus = "Connected";
  } else {
    Serial.println("[WIFI] ✗ Connection failed");
    systemStatus = "WiFi Failed";
  }
  Serial.println();
}

// ═════════════════════════════════════════════════════════════════════
// 🌐 WEB SERVER SETUP
// ═════════════════════════════════════════════════════════════════════

void setupWebServer() {
  Serial.println("[SERVER] Setting up endpoints...");
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/extract", HTTP_GET, handleExtract);
  server.on("/ping", HTTP_GET, handlePing);
  Serial.println("[SERVER] ✓ Endpoints configured\n");
}

// ═════════════════════════════════════════════════════════════════════
// 📡 API HANDLERS
// ═════════════════════════════════════════════════════════════════════

void handleStatus() {
  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["status"] = systemStatus;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["lastExtractedValue"] = lastExtractedValue;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleCapture() {
  Serial.println("[CAMERA] Capture request received");
  
  camera_fb_t* fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("[CAMERA] ✗ Capture failed!");
    DynamicJsonDocument doc(128);
    doc["success"] = false;
    doc["error"] = "Failed to capture";
    String response;
    serializeJson(doc, response);
    server.send(500, "application/json", response);
    return;
  }
  
  Serial.printf("[CAMERA] ✓ Captured %d bytes\n", fb->len);
  
  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["message"] = "Image captured";
  doc["size"] = fb->len;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  esp_camera_fb_return(fb);
}

void handleExtract() {
  Serial.println("[OCR] Extract request received");
  
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[OCR] ✗ No frame available");
    DynamicJsonDocument doc(128);
    doc["success"] = false;
    doc["error"] = "No frame available";
    String response;
    serializeJson(doc, response);
    server.send(500, "application/json", response);
    return;
  }
  
  Serial.println("[OCR] Sending image to Render API...");
  
  HTTPClient http;
  http.begin(OCR_API_URL);
  http.addHeader("Content-Type", "image/jpeg");
  
  int httpCode = http.POST(fb->buf, fb->len);
  
  DynamicJsonDocument responseDoc(512);
  
  if (httpCode == 200) {
    String payload = http.getString();
    deserializeJson(responseDoc, payload);
    
    if (responseDoc["success"]) {
      lastExtractedValue = responseDoc["extractedValue"];
      
      DynamicJsonDocument doc(256);
      doc["success"] = true;
      doc["extractedValue"] = lastExtractedValue;
      doc["rawText"] = responseDoc["rawText"];
      doc["unit"] = "mA";
      
      String response;
      serializeJson(doc, response);
      server.send(200, "application/json", response);
      
      Serial.printf("[OCR] ✓ Extracted: %.4f mA\n", lastExtractedValue);
    } else {
      DynamicJsonDocument doc(128);
      doc["success"] = false;
      doc["error"] = "OCR failed";
      String response;
      serializeJson(doc, response);
      server.send(500, "application/json", response);
      Serial.println("[OCR] ✗ OCR extraction failed");
    }
  } else {
    DynamicJsonDocument doc(256);
    doc["success"] = false;
    doc["error"] = "API error";
    doc["http_code"] = httpCode;
    String response;
    serializeJson(doc, response);
    server.send(500, "application/json", response);
    Serial.printf("[OCR] ✗ HTTP error %d\n", httpCode);
  }
  
  http.end();
  esp_camera_fb_return(fb);
}

void handlePing() {
  DynamicJsonDocument doc(128);
  doc["success"] = true;
  doc["message"] = "pong";
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

/*
 * ═════════════════════════════════════════════════════════════════════
 * RF ANTENNA AUTOMATION SYSTEM - ESP32-CAM MODULE
 * ═════════════════════════════════════════════════════════════════════
 * 
 * This code runs on the ESP32-CAM module (SEPARATE device from motor ESP32)
 * 
 * Responsibilities:
 * - Capture images from camera
 * - Store images temporarily
 * - Send images to Render OCR API
 * - Return extracted readings
 * 
 * Communication:
 * Main ESP32 → sends HTTP GET /capture → This module captures image
 * Main ESP32 → sends HTTP GET /extract → This module sends to Render
 * 
 * ═════════════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include <SPIFFS.h>

// ═════════════════════════════════════════════════════════════════════
// ⚙️ CONFIGURATION
// ═════════════════════════════════════════════════════════════════════

// WiFi Configuration (SAME NETWORK as main ESP32)
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// STATIC IP FOR ESP32-CAM (THIS DEVICE)
// This IP will ALWAYS be the same - no need to check Serial Monitor!
IPAddress staticIP(192, 168, 1, 51);                   // Fixed IP for camera
IPAddress gateway(192, 168, 1, 1);                     // Your router IP
IPAddress subnet(255, 255, 255, 0);                    // Subnet mask
IPAddress primaryDNS(8, 8, 8, 8);                      // Google DNS
IPAddress secondaryDNS(8, 8, 4, 4);                    // Google DNS

// Render OCR API Endpoint
const char* OCR_API_URL = "https://antenna-ocr-api.onrender.com/extract-ocr";

// ═════════════════════════════════════════════════════════════════════
// 📷 CAMERA PIN CONFIGURATION (FIXED FOR ESP32-CAM)
// ═════════════════════════════════════════════════════════════════════

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5

#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// ═════════════════════════════════════════════════════════════════════
// 🖥️ WEB SERVER
// ═════════════════════════════════════════════════════════════════════

WebServer server(80);

// ═════════════════════════════════════════════════════════════════════
// 📊 STATE VARIABLES
// ═════════════════════════════════════════════════════════════════════

String systemStatus = "Initializing";
uint8_t* lastImageBuffer = NULL;
size_t lastImageSize = 0;
String lastImagePath = "/image.jpg";
float lastExtractedValue = 0.0;

// ═════════════════════════════════════════════════════════════════════
// 🎯 SETUP
// ═════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n╔════════════════════════════════════════════════════╗");
  Serial.println("║  📷 ESP32-CAM MODULE - STARTUP                   ║");
  Serial.println("║  (Controlled by Main ESP32)                      ║");
  Serial.println("╚════════════════════════════════════════════════════╝\n");
  
  // Initialize Camera
  initializeCamera();
  
  // Initialize SPIFFS (file storage)
  initializeSPIFFS();
  
  // Connect WiFi
  connectToWiFi();
  
  // Setup Web Server
  setupWebServer();
  
  // Start server
  server.begin();
  Serial.println("[SERVER] Camera API server started on port 80");
  Serial.println("[SYSTEM] Ready to capture and extract!\n");
}

// ═════════════════════════════════════════════════════════════════════
// 🔄 MAIN LOOP
// ═════════════════════════════════════════════════════════════════════

void loop() {
  server.handleClient();
  delay(10);
}

// ═════════════════════════════════════════════════════════════════════
// 📷 CAMERA INITIALIZATION
// ═════════════════════════════════════════════════════════════════════

void initializeCamera() {
  Serial.println("[CAMERA] Initializing ESP32-CAM...");
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_freq = 20000000;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;  // 800x600
  config.jpeg_quality = 10;            // 0-63, lower = better quality
  config.fb_count = 1;
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAMERA] Init failed with error 0x%x\n", err);
    systemStatus = "Camera Init Failed";
    return;
  }
  
  Serial.println("[CAMERA] ✓ Initialized successfully!");
  systemStatus = "Ready";
}

// ═════════════════════════════════════════════════════════════════════
// 💾 SPIFFS INITIALIZATION
// ═════════════════════════════════════════════════════════════════════

void initializeSPIFFS() {
  Serial.println("[SPIFFS] Initializing file system...");
  
  if (!SPIFFS.begin(true)) {
    Serial.println("[SPIFFS] Failed to mount - will create new");
  } else {
    Serial.println("[SPIFFS] ✓ Mounted successfully");
  }
}

// ═════════════════════════════════════════════════════════════════════
// 📡 WIFI CONNECTION WITH STATIC IP
// ═════════════════════════════════════════════════════════════════════

void connectToWiFi() {
  Serial.println("[WIFI] Configuring static IP...");
  Serial.println("[WIFI] Static IP: 192.168.1.51 (ALWAYS the same!)");
  
  // Configure static IP BEFORE connecting
  if (!WiFi.config(staticIP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("[WIFI] ✗ Failed to configure static IP!");
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
  } else {
    Serial.println("[WIFI] ✗ Connection failed!");
    systemStatus = "WiFi Failed";
  }
  Serial.println();
}

// ═════════════════════════════════════════════════════════════════════
// 🌐 WEB SERVER - API ENDPOINTS
// ═════════════════════════════════════════════════════════════════════

void setupWebServer() {
  Serial.println("[SERVER] Setting up API endpoints...");
  
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/extract", HTTP_GET, handleExtract);
  server.on("/ping", HTTP_GET, handlePing);
  
  Serial.println("[SERVER] ✓ Endpoints configured\n");
}

// ═════════════════════════════════════════════════════════════════════
// 📡 API HANDLERS
// ═════════════════════════════════════════════════════════════════════

// GET /status - Return camera status
void handleStatus() {
  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["status"] = systemStatus;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["lastImagePath"] = lastImagePath;
  doc["lastImageSize"] = lastImageSize;
  doc["lastExtractedValue"] = lastExtractedValue;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  Serial.printf("[API] Status request: %s\n", systemStatus.c_str());
}

// GET /capture - Capture image from camera
void handleCapture() {
  Serial.println("[CAMERA] Capture request received");
  systemStatus = "Capturing...";
  
  // Capture frame
  camera_fb_t* fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("[CAMERA] ✗ Capture failed!");
    systemStatus = "Capture Failed";
    
    DynamicJsonDocument doc(128);
    doc["success"] = false;
    doc["error"] = "Failed to capture frame";
    
    String response;
    serializeJson(doc, response);
    server.send(500, "application/json", response);
    return;
  }
  
  Serial.printf("[CAMERA] ✓ Captured! Size: %d bytes\n", fb->len);
  
  // Save to SPIFFS
  File f = SPIFFS.open(lastImagePath, "w");
  if (f) {
    f.write(fb->buf, fb->len);
    f.close();
    lastImageSize = fb->len;
    
    Serial.printf("[SPIFFS] ✓ Image saved: %s (%d bytes)\n", lastImagePath.c_str(), lastImageSize);
  } else {
    Serial.println("[SPIFFS] ✗ Failed to save image");
  }
  
  // Also keep in memory for quick access
  lastImageBuffer = fb->buf;
  lastImageSize = fb->len;
  
  // Return success
  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["message"] = "Image captured";
  doc["filename"] = lastImagePath;
  doc["size"] = lastImageSize;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  esp_camera_fb_return(fb);
  systemStatus = "Ready";
}

// GET /extract - Extract reading from last captured image
void handleExtract() {
  Serial.println("[OCR] Extract request received");
  systemStatus = "Extracting...";
  
  if (lastImageSize == 0) {
    Serial.println("[OCR] ✗ No image captured yet!");
    
    DynamicJsonDocument doc(128);
    doc["success"] = false;
    doc["error"] = "No image available. Call /capture first.";
    
    String response;
    serializeJson(doc, response);
    server.send(400, "application/json", response);
    return;
  }
  
  // Capture fresh image
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[CAMERA] ✗ Failed to get frame for extraction");
    
    DynamicJsonDocument doc(128);
    doc["success"] = false;
    doc["error"] = "Failed to capture image";
    
    String response;
    serializeJson(doc, response);
    server.send(500, "application/json", response);
    return;
  }
  
  // Send to Render API
  Serial.println("[OCR] Sending image to Render API...");
  
  HTTPClient http;
  http.begin(OCR_API_URL);
  http.addHeader("Content-Type", "image/jpeg");
  
  int httpCode = http.POST(fb->buf, fb->len);
  
  DynamicJsonDocument responseDoc(512);
  
  if (httpCode == 200) {
    String payload = http.getString();
    
    Serial.printf("[HTTP] Response: %s\n", payload.c_str());
    
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
      systemStatus = "Ready";
    } else {
      DynamicJsonDocument doc(128);
      doc["success"] = false;
      doc["error"] = "OCR extraction failed";
      
      String response;
      serializeJson(doc, response);
      server.send(500, "application/json", response);
      
      Serial.println("[OCR] ✗ Extraction failed");
    }
  } else {
    DynamicJsonDocument doc(256);
    doc["success"] = false;
    doc["error"] = "Failed to connect to Render API";
    doc["http_code"] = httpCode;
    
    String response;
    serializeJson(doc, response);
    server.send(500, "application/json", response);
    
    Serial.printf("[HTTP] Error: %d\n", httpCode);
    systemStatus = "API Error";
  }
  
  http.end();
  esp_camera_fb_return(fb);
}

// GET /ping - Simple ping test
void handlePing() {
  DynamicJsonDocument doc(128);
  doc["success"] = true;
  doc["message"] = "pong";
  doc["timestamp"] = millis();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

/*
 * ═════════════════════════════════════════════════════════════════════
 * CAMERA PIN CONFIGURATION
 * ═════════════════════════════════════════════════════════════════════
 * 
 * These pins are FIXED for ESP32-CAM module.
 * Do NOT change them!
 * 
 * Camera Data Pins:
 * GPIO 35 (Y9)  - Data pin D0
 * GPIO 34 (Y8)  - Data pin D1
 * GPIO 39 (Y7)  - Data pin D2
 * GPIO 36 (Y6)  - Data pin D3
 * GPIO 21 (Y5)  - Data pin D4
 * GPIO 19 (Y4)  - Data pin D5
 * GPIO 18 (Y3)  - Data pin D6
 * GPIO 5  (Y2)  - Data pin D7
 * 
 * Camera Control Pins:
 * GPIO 22 (PCLK)   - Pixel Clock
 * GPIO 23 (HREF)   - Horizontal Reference
 * GPIO 25 (VSYNC)  - Vertical Sync
 * GPIO 0  (XCLK)   - Master Clock
 * 
 * Camera I2C:
 * GPIO 26 (SIOD)   - I2C SDA
 * GPIO 27 (SIOC)   - I2C SCL
 * 
 * GPIO 32 (PWDN)   - Power Down
 * GPIO -1 (RESET)  - Reset (unused)
 * 
 * ═════════════════════════════════════════════════════════════════════
 */

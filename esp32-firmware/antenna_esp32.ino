/*
 * RF ANTENNA AUTOMATION SYSTEM - ESP32 FIRMWARE
 * 
 * Controls:
 * - Motor rotation via A4988 driver
 * - ESP32-CAM image capture
 * - Web server for remote control
 * - Communication with Render OCR API
 * - Firebase data storage
 */

#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ==================== WIFI CONFIGURATION ====================
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ==================== MOTOR CONTROL PINS ====================
#define STEP_PIN 19       // GPIO 19 - A4988 STEP
#define DIR_PIN 18        // GPIO 18 - A4988 DIRECTION
#define ENABLE_PIN 5      // GPIO 5  - A4988 ENABLE

// Motor settings
#define STEPS_PER_DEGREE 3.33  // Adjust based on your motor (1.8° per step)
#define MAX_SPEED 1000         // Microseconds between steps (lower = faster)

// ==================== CAMERA CONFIGURATION ====================
camera_config_t config;

// ==================== STATE VARIABLES ====================
WebServer server(80);
volatile int currentAngle = 0;
volatile bool isRunning = false;
volatile bool isPaused = false;
int degreesPerMove = 10;
int repeatCount = 36;
String ocrApiEndpoint = "https://antenna-ocr-api.onrender.com";

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n[ANTENNA SYSTEM] Initializing...");
  
  // Initialize pins
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW);  // Enable motor
  
  // Connect to WiFi
  setupWiFi();
  
  // Initialize camera
  setupCamera();
  
  // Setup web server routes
  setupWebServer();
  
  Serial.println("[ANTENNA SYSTEM] Ready!");
}

// ==================== MAIN LOOP ====================
void loop() {
  server.handleClient();
}

// ==================== WIFI SETUP ====================
void setupWiFi() {
  Serial.println("[WIFI] Connecting to network...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Connected!");
    Serial.println("IP Address: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WIFI] Connection failed!");
  }
}

// ==================== CAMERA SETUP ====================
void setupCamera() {
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
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;
  
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("[CAMERA] Init failed!");
    return;
  }
  Serial.println("[CAMERA] Initialized!");
}

// ==================== WEB SERVER SETUP ====================
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/control", handleControl);
  server.on("/capture", handleCapture);
  server.on("/settings", handleSettings);
  server.begin();
  Serial.println("[SERVER] Web server started on port 80");
}

// ==================== WEB SERVER HANDLERS ====================

// Root HTML page
void handleRoot() {
  String html = R"(
    <!DOCTYPE html>
    <html>
    <head>
      <title>ESP32 Antenna Control</title>
      <style>
        body { font-family: Arial; margin: 20px; background: #f0f0f0; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
        .status { padding: 10px; background: #e8f5e9; border-radius: 5px; margin: 10px 0; }
        .button { padding: 10px 20px; margin: 5px; cursor: pointer; }
        .input-group { margin: 10px 0; }
        input { padding: 8px; width: 200px; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>🛰️ ESP32 Antenna Control</h1>
        <div class="status">
          <p>Status: <strong id="status">Loading...</strong></p>
          <p>Current Angle: <strong id="angle">0</strong>°</p>
        </div>
        <div class="input-group">
          <label>Degrees per Move:</label><br>
          <input type="number" id="degreesPerMove" value="10">
          <button class="button" onclick="setSetting('degrees', document.getElementById('degreesPerMove').value)">Set</button>
        </div>
        <div class="input-group">
          <label>Repeat Count:</label><br>
          <input type="number" id="repeatCount" value="36">
          <button class="button" onclick="setSetting('repeat', document.getElementById('repeatCount').value)">Set</button>
        </div>
        <div>
          <button class="button" onclick="send('reset')">↺ Reset to 0</button>
          <button class="button" onclick="send('rotate', 45)">Rotate to 45°</button>
          <button class="button" onclick="send('rotate', 90)">Rotate to 90°</button>
          <button class="button" onclick="captureImage()">📷 Capture Image</button>
        </div>
      </div>
      <script>
        function send(cmd, angle = null) {
          let url = '/control?cmd=' + cmd;
          if (angle !== null) url += '&angle=' + angle;
          fetch(url).then(r => r.json()).then(d => {
            document.getElementById('status').textContent = d.message;
            if (d.angle !== undefined) document.getElementById('angle').textContent = d.angle;
          });
        }
        function setSetting(key, value) {
          fetch('/settings?key=' + key + '&value=' + value).then(r => r.json()).then(d => {
            alert(d.message);
          });
        }
        function captureImage() {
          fetch('/capture').then(r => r.json()).then(d => alert(d.message));
        }
        setInterval(() => {
          fetch('/status').then(r => r.json()).then(d => {
            document.getElementById('status').textContent = d.status;
            document.getElementById('angle').textContent = d.angle;
          });
        }, 1000);
      </script>
    </body>
    </html>
  )";
  server.send(200, "text/html", html);
}

// Status endpoint
void handleStatus() {
  DynamicJsonDocument doc(200);
  doc["status"] = isRunning ? (isPaused ? "Paused" : "Running") : "Idle";
  doc["angle"] = currentAngle;
  doc["degrees_per_move"] = degreesPerMove;
  doc["repeat_count"] = repeatCount;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// Control endpoint
void handleControl() {
  String cmd = server.arg("cmd");
  
  if (cmd == "rotate") {
    int angle = server.arg("angle").toInt();
    rotateToAngle(angle);
    currentAngle = angle;
  } else if (cmd == "reset") {
    rotateToAngle(0);
    currentAngle = 0;
  } else if (cmd == "pause") {
    isPaused = true;
  } else if (cmd == "resume") {
    isPaused = false;
  }
  
  DynamicJsonDocument doc(200);
  doc["message"] = "Command executed";
  doc["angle"] = currentAngle;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// Capture image and send to OCR API
void handleCapture() {
  camera_fb_t * fb = esp_camera_fb_get();
  
  if (!fb) {
    server.send(500, "application/json", "{\"error\":\"Camera capture failed\"}");
    return;
  }
  
  // For now, just return success
  // In production, you'd send this to the OCR API
  DynamicJsonDocument doc(200);
  doc["message"] = "Image captured";
  doc["imagePath"] = "/camera/capture";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  esp_camera_fb_return(fb);
}

// Settings endpoint
void handleSettings() {
  String key = server.arg("key");
  String value = server.arg("value");
  
  if (key == "degrees") {
    degreesPerMove = value.toInt();
  } else if (key == "repeat") {
    repeatCount = value.toInt();
  }
  
  DynamicJsonDocument doc(200);
  doc["message"] = "Setting updated";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ==================== MOTOR CONTROL FUNCTIONS ====================

void rotateToAngle(int targetAngle) {
  int stepsNeeded = abs((targetAngle - currentAngle) * STEPS_PER_DEGREE);
  
  if (targetAngle > currentAngle) {
    digitalWrite(DIR_PIN, HIGH);  // Clockwise
  } else {
    digitalWrite(DIR_PIN, LOW);   // Counter-clockwise
  }
  
  Serial.println("[MOTOR] Rotating to " + String(targetAngle) + "°");
  
  for (int i = 0; i < stepsNeeded; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(MAX_SPEED / 2);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(MAX_SPEED / 2);
  }
  
  Serial.println("[MOTOR] Reached " + String(targetAngle) + "°");
}

// ==================== UTILITY FUNCTIONS ====================

// Send data to Render OCR API
void sendToOCRApi(camera_fb_t * fb) {
  HTTPClient http;
  
  String url = ocrApiEndpoint + "/extract-ocr";
  http.begin(url);
  http.addHeader("Content-Type", "application/octet-stream");
  
  // Send image data
  int httpResponseCode = http.POST(fb->buf, fb->len);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("[OCR API] Response: " + response);
  } else {
    Serial.println("[OCR API] Error: " + String(httpResponseCode));
  }
  
  http.end();
}

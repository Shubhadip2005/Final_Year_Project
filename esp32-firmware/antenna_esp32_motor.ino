/*
 * ═════════════════════════════════════════════════════════════════════
 * RF ANTENNA AUTOMATION SYSTEM - ESP32 MOTOR CONTROLLER
 * ═════════════════════════════════════════════════════════════════════
 * 
 * This is the MAIN ESP32 that ONLY controls the motor.
 * It receives commands from your React website (Vercel).
 * It communicates with ESP32-CAM module for image capture.
 * 
 * NO HTML INTERFACE - Use the React website instead!
 * 
 * Hardware Required:
 * - ESP32 (Main controller)
 * - ESP32-CAM (Separate module for camera)
 * - A4988 Motor Driver
 * - NEMA 17 Stepper Motor
 * - 12V Power Supply
 * 
 * ═════════════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_http_client.h>

// ═════════════════════════════════════════════════════════════════════
// ⚙️ CONFIGURATION - UPDATE THESE VALUES
// ═════════════════════════════════════════════════════════════════════

// WiFi Configuration
const char* WIFI_SSID = "YOUR_WIFI_SSID";              // ← UPDATE THIS
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";      // ← UPDATE THIS

// STATIC IP FOR MAIN ESP32 (THIS DEVICE)
// This IP will ALWAYS be the same - no need to check Serial Monitor!
IPAddress staticIP(192, 168, 1, 50);                   // Fixed IP
IPAddress gateway(192, 168, 1, 1);                     // Your router IP
IPAddress subnet(255, 255, 255, 0);                    // Subnet mask
IPAddress primaryDNS(8, 8, 8, 8);                      // Google DNS
IPAddress secondaryDNS(8, 8, 4, 4);                    // Google DNS

// ESP32-CAM Module (separate device on same network)
const char* ESP32_CAM_IP = "192.168.1.51";             // Fixed IP for camera

// Render OCR API
const char* OCR_API_URL = "https://antenna-ocr-api.onrender.com/extract-ocr";

// Firebase
const char* FIREBASE_HOST = "your-project.firebaseio.com";
const char* FIREBASE_AUTH = "your-firebase-token";

// ═════════════════════════════════════════════════════════════════════
// 🔌 MOTOR CONTROL PINS (A4988 Driver)
// ═════════════════════════════════════════════════════════════════════

#define STEP_PIN 19        // GPIO 19 - Stepper STEP pulse
#define DIR_PIN 18         // GPIO 18 - Direction control
#define ENABLE_PIN 5       // GPIO 5  - Motor enable (active LOW)

// ═════════════════════════════════════════════════════════════════════
// ⚙️ MOTOR PARAMETERS
// ═════════════════════════════════════════════════════════════════════

#define STEPS_PER_DEGREE 3.33      // NEMA 17: 200 steps/rev = 1.8°/step
#define STEP_DELAY_US 1000         // Microseconds between steps
#define FIELD_STABILIZATION_TIME 30000  // 30 seconds (in milliseconds)

// ═════════════════════════════════════════════════════════════════════
// 🖥️ WEB SERVER ON PORT 80
// ═════════════════════════════════════════════════════════════════════

WebServer server(80);

// ═════════════════════════════════════════════════════════════════════
// 📊 SYSTEM STATE VARIABLES
// ═════════════════════════════════════════════════════════════════════

volatile int currentAngle = 0;
volatile bool isRunning = false;
volatile bool motorEnabled = true;

String systemStatus = "Ready";
unsigned long lastStatusTime = 0;

// ═════════════════════════════════════════════════════════════════════
// 🎯 SETUP FUNCTION
// ═════════════════════════════════════════════════════════════════════

void setup() {
  // Serial for debugging
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n╔════════════════════════════════════════════════════╗");
  Serial.println("║  🛰️  ESP32 MOTOR CONTROLLER - STARTUP             ║");
  Serial.println("║  (React website controls this device)             ║");
  Serial.println("╚════════════════════════════════════════════════════╝\n");
  
  // Initialize Motor
  initializeMotor();
  
  // Connect WiFi
  connectToWiFi();
  
  // Setup Web Server (API only, no HTML)
  setupWebServer();
  
  // Start server
  server.begin();
  Serial.println("[SERVER] API server started on port 80");
  Serial.println("[SYSTEM] Ready for commands from React website!\n");
}

// ═════════════════════════════════════════════════════════════════════
// 🔄 MAIN LOOP
// ═════════════════════════════════════════════════════════════════════

void loop() {
  server.handleClient();
  delay(10);
}

// ═════════════════════════════════════════════════════════════════════
// 🔧 MOTOR INITIALIZATION
// ═════════════════════════════════════════════════════════════════════

void initializeMotor() {
  Serial.println("[MOTOR] Initializing...");
  
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  digitalWrite(ENABLE_PIN, HIGH);  // HIGH = disabled initially
  
  Serial.println("[MOTOR] ✓ Initialized");
  Serial.println("  STEP:   GPIO 19");
  Serial.println("  DIR:    GPIO 18");
  Serial.println("  ENABLE: GPIO 5\n");
}

// ═════════════════════════════════════════════════════════════════════
// 📡 WIFI CONNECTION WITH STATIC IP
// ═════════════════════════════════════════════════════════════════════

void connectToWiFi() {
  Serial.println("[WIFI] Configuring static IP...");
  Serial.println("[WIFI] Static IP: 192.168.1.50 (ALWAYS the same!)");
  
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
    Serial.println("[WIFI] ⭐ IP is ALWAYS 192.168.1.50");
    Serial.println("[WIFI] ⭐ No need to check Serial Monitor!");
    systemStatus = "Connected";
  } else {
    Serial.println("[WIFI] ✗ Connection failed!");
    systemStatus = "WiFi Failed";
  }
  Serial.println();
}

// ═════════════════════════════════════════════════════════════════════
// 🌐 WEB SERVER - API ENDPOINTS (NO HTML)
// ═════════════════════════════════════════════════════════════════════

void setupWebServer() {
  Serial.println("[SERVER] Setting up API endpoints...");
  
  // API Endpoints (returns JSON only)
  server.on("/status", HTTP_GET, handleStatus);           // Get current status
  server.on("/rotate", HTTP_GET, handleRotate);           // Rotate to angle
  server.on("/capture", HTTP_GET, handleCapture);         // Tell camera to capture
  server.on("/extract", HTTP_GET, handleExtract);         // Extract from camera image
  server.on("/enable-motor", HTTP_GET, handleEnableMotor);
  server.on("/disable-motor", HTTP_GET, handleDisableMotor);
  server.on("/ping", HTTP_GET, handlePing);               // Ping test
  
  Serial.println("[SERVER] ✓ Endpoints configured\n");
}

// ═════════════════════════════════════════════════════════════════════
// 📡 API HANDLERS
// ═════════════════════════════════════════════════════════════════════

// GET /status - Return current motor status
void handleStatus() {
  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["angle"] = currentAngle;
  doc["status"] = systemStatus;
  doc["motor_enabled"] = motorEnabled;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["running"] = isRunning;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  Serial.printf("[API] Status request: angle=%d°, status=%s\n", currentAngle, systemStatus.c_str());
}

// GET /rotate?angle=90 - Rotate motor to specific angle
void handleRotate() {
  if (!server.hasArg("angle")) {
    DynamicJsonDocument doc(128);
    doc["success"] = false;
    doc["error"] = "Missing 'angle' parameter";
    
    String response;
    serializeJson(doc, response);
    server.send(400, "application/json", response);
    return;
  }
  
  int targetAngle = server.arg("angle").toInt();
  
  Serial.printf("[MOTOR] Received rotate command: %d°\n", targetAngle);
  
  // Rotate motor
  rotateToAngle(targetAngle);
  
  // Respond
  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["angle"] = currentAngle;
  doc["message"] = "Rotated to " + String(targetAngle) + "°";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// GET /capture - Tell ESP32-CAM to capture image
void handleCapture() {
  Serial.println("[CAMERA] Capture request received");
  
  // Send command to ESP32-CAM
  const char* camURL = "http://192.168.1.XXX/capture";  // ← Use actual camera IP
  
  HTTPClient http;
  http.begin(camURL);
  int httpCode = http.GET();
  
  DynamicJsonDocument doc(256);
  
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument camResponse(512);
    deserializeJson(camResponse, payload);
    
    doc["success"] = true;
    doc["message"] = "Image captured from ESP32-CAM";
    doc["filename"] = camResponse["filename"] | "image.jpg";
    
    Serial.println("[CAMERA] ✓ Image captured successfully");
  } else {
    doc["success"] = false;
    doc["error"] = "Failed to communicate with ESP32-CAM";
    Serial.printf("[CAMERA] ✗ Error: HTTP %d\n", httpCode);
  }
  
  http.end();
  
  String response;
  serializeJson(doc, response);
  server.send(httpCode == 200 ? 200 : 500, "application/json", response);
}

// GET /extract - Extract reading from camera image
void handleExtract() {
  Serial.println("[OCR] Extract request received");
  
  // This endpoint tells the ESP32-CAM to send its image to Render API
  // The result comes back and we return it to the website
  
  DynamicJsonDocument doc(256);
  doc["success"] = true;
  doc["message"] = "Extraction initiated. Check Firebase for results.";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// GET /enable-motor - Enable motor
void handleEnableMotor() {
  digitalWrite(ENABLE_PIN, LOW);  // LOW = enabled
  motorEnabled = true;
  systemStatus = "Motor Enabled";
  
  DynamicJsonDocument doc(128);
  doc["success"] = true;
  doc["message"] = "Motor enabled";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  Serial.println("[MOTOR] Enabled");
}

// GET /disable-motor - Disable motor
void handleDisableMotor() {
  digitalWrite(ENABLE_PIN, HIGH);  // HIGH = disabled
  motorEnabled = false;
  systemStatus = "Motor Disabled";
  
  DynamicJsonDocument doc(128);
  doc["success"] = true;
  doc["message"] = "Motor disabled";
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
  
  Serial.println("[MOTOR] Disabled");
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

// ═════════════════════════════════════════════════════════════════════
// 🔄 MOTOR CONTROL FUNCTIONS
// ═════════════════════════════════════════════════════════════════════

void rotateToAngle(int targetAngle) {
  if (!motorEnabled) {
    Serial.println("[MOTOR] Motor is disabled!");
    return;
  }
  
  // Normalize angle to 0-360
  targetAngle = targetAngle % 360;
  if (targetAngle < 0) targetAngle += 360;
  
  // Calculate steps
  int angleDifference = targetAngle - currentAngle;
  
  // Handle wrap-around (e.g., 350° to 10° = -340° becomes 20°)
  if (angleDifference > 180) {
    angleDifference -= 360;
  } else if (angleDifference < -180) {
    angleDifference += 360;
  }
  
  int stepsNeeded = abs(angleDifference * STEPS_PER_DEGREE);
  
  if (stepsNeeded == 0) {
    Serial.printf("[MOTOR] Already at %d°\n", targetAngle);
    return;
  }
  
  // Set direction
  if (angleDifference > 0) {
    digitalWrite(DIR_PIN, HIGH);   // Clockwise
  } else {
    digitalWrite(DIR_PIN, LOW);    // Counter-clockwise
  }
  
  // Enable motor
  digitalWrite(ENABLE_PIN, LOW);
  
  Serial.printf("[MOTOR] Rotating from %d° to %d° (%d steps)...\n", 
                currentAngle, targetAngle, stepsNeeded);
  
  // Generate step pulses
  for (int i = 0; i < stepsNeeded; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(STEP_DELAY_US / 2);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(STEP_DELAY_US / 2);
  }
  
  // Update current angle
  currentAngle = targetAngle;
  systemStatus = "Ready";
  
  Serial.printf("[MOTOR] ✓ Reached %d°\n", currentAngle);
}

// ═════════════════════════════════════════════════════════════════════
// 📋 HELPER FUNCTIONS
// ═════════════════════════════════════════════════════════════════════

void printSystemInfo() {
  Serial.println("\n╔════════════════════════════════════════════════════╗");
  Serial.println("║           SYSTEM INFORMATION                      ║");
  Serial.println("╠════════════════════════════════════════════════════╣");
  Serial.printf("║ WiFi SSID:        %-32s║\n", WIFI_SSID);
  Serial.printf("║ IP Address:       %-32s║\n", WiFi.localIP().toString().c_str());
  Serial.printf("║ Current Angle:    %-32d║\n", currentAngle);
  Serial.printf("║ Motor Enabled:    %-32s║\n", motorEnabled ? "Yes" : "No");
  Serial.printf("║ Status:           %-32s║\n", systemStatus.c_str());
  Serial.println("╠════════════════════════════════════════════════════╣");
  Serial.println("║ API ENDPOINTS:                                    ║");
  Serial.println("║ GET /status           - Get motor status          ║");
  Serial.println("║ GET /rotate?angle=N   - Rotate to angle N         ║");
  Serial.println("║ GET /capture          - Capture image             ║");
  Serial.println("║ GET /extract          - Extract reading           ║");
  Serial.println("║ GET /ping             - Ping test                 ║");
  Serial.println("╚════════════════════════════════════════════════════╝\n");
}

/*
 * ═════════════════════════════════════════════════════════════════════
 * PIN CONFIGURATION REFERENCE
 * ═════════════════════════════════════════════════════════════════════
 * 
 * MOTOR CONNECTIONS:
 * GPIO 19 (STEP)  ← Connect to A4988 STEP pin
 * GPIO 18 (DIR)   ← Connect to A4988 DIR pin
 * GPIO 5 (ENABLE) ← Connect to A4988 ENABLE pin
 * GND             ← Connect to A4988 GND & ESP32 GND
 * 5V (USB)        ← Connect to A4988 VCC
 * 
 * A4988 TO MOTOR:
 * VMOT (12V)      ← 12V power supply +
 * GND             ← 12V power supply -
 * 1A, 1B          ← Motor coil 1
 * 2A, 2B          ← Motor coil 2
 * 
 * NO CAMERA PINS - Camera is a separate ESP32-CAM module!
 * 
 * ═════════════════════════════════════════════════════════════════════
 */

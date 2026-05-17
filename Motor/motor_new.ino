#include <WiFi.h>
#include <WebServer.h>

// --- NEMA 17 / A4988 Configuration ---
#define STEP_PIN 19
#define DIR_PIN 18
#define ENABLE_PIN 21

const int totalStepsPerRev = 200;
int currentAngle = 0;
bool stopRequested = false;

// --- Resume State ---
int resumeStepsLeft = 0;
bool resumeClockwise = true;
int resumeIterationsLeft = 0;
float resumeDegPerInterval = 0;

// --- Network Credentials ---
const char* ssid = "*****";
const char* password = "*****";

WebServer server(80);

// -------------------------------------------------------
// CHANGE 1: Helper to add CORS headers to every response.
// WHY: The hosted frontend (on Vercel/Render) is a
// different "origin" than the ESP32's IP. Browsers block
// cross-origin requests unless the SERVER explicitly
// allows them via these headers. Without this, every
// fetch() call from your hosted page would be rejected
// by the browser before it even reaches the ESP32.
// Nothing about motor logic changes — this is purely
// a network/browser policy fix.
// -------------------------------------------------------
void addCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// --- Helper: Core Motor Movement with resume tracking ---
// NO CHANGES from original
void moveStepper(int steps, bool clockwise) {
  digitalWrite(DIR_PIN, clockwise ? HIGH : LOW);
  for (int i = 0; i < steps; i++) {

    if (stopRequested) {
      resumeStepsLeft = steps - i;
      resumeClockwise = clockwise;
      Serial.print("Motor paused. Steps left: ");
      Serial.println(resumeStepsLeft);
      return;
    }

    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(1000);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(1000);

    if (i % 50 == 0) {
      server.handleClient();
      yield();
    }
  }
  resumeStepsLeft = 0;
}

// --- Shared rotation runner (used by both handleRotate and handleResume) ---
// NO CHANGES from original
void runRotation(int iterations, float degPerInterval) {
  bool clockwise = (degPerInterval > 0);
  int stepsToMove = abs((degPerInterval / 360.0) * totalStepsPerRev);

  for (int i = 0; i < iterations; i++) {
    if (stopRequested) {
      resumeIterationsLeft = iterations - i;
      resumeDegPerInterval = degPerInterval;
      Serial.println("Rotation sequence paused between intervals.");
      return;
    }

    int steps = stepsToMove;
    moveStepper(steps, clockwise);

    if (stopRequested) {
      resumeIterationsLeft = iterations - i;
      resumeDegPerInterval = degPerInterval;
      return;
    }

    currentAngle += (int)degPerInterval;

    unsigned long startPause = millis();
    while (millis() - startPause < 30000) {
      if (stopRequested) {
        resumeIterationsLeft = iterations - i - 1;
        resumeDegPerInterval = degPerInterval;
        return;
      }
      server.handleClient();
      yield();
    }
  }

  resumeIterationsLeft = 0;
  resumeStepsLeft = 0;
}

// --- Handler: Start Rotation ---
// CHANGE 2: Added addCORSHeaders() before every server.send()
// WHY: Same reason as above — browser needs the header on
// every single response, not just one.
void handleRotate() {
  addCORSHeaders();
  if (server.hasArg("num") && server.hasArg("deg")) {
    int iterations = server.arg("num").toInt();
    float degPerInterval = server.arg("deg").toFloat();

    stopRequested = false;
    resumeStepsLeft = 0;
    resumeIterationsLeft = 0;

    server.send(200, "text/plain", "OK");
    runRotation(iterations, degPerInterval);
  } else {
    server.send(400, "text/plain", "Missing args");
  }
}

// --- Handler: Resume from paused state ---
// CHANGE 2 applied here too
void handleResume() {
  addCORSHeaders();
  if (resumeIterationsLeft == 0 && resumeStepsLeft == 0) {
    server.send(200, "text/plain", "Nothing to resume.");
    return;
  }

  stopRequested = false;
  server.send(200, "text/plain", "MOTOR RESUMED — Continuing from paused position.");
  Serial.println("Resuming motor...");

  if (resumeStepsLeft > 0) {
    moveStepper(resumeStepsLeft, resumeClockwise);
    resumeStepsLeft = 0;

    if (stopRequested) return;

    currentAngle += (int)resumeDegPerInterval;

    unsigned long startPause = millis();
    while (millis() - startPause < 30000) {
      if (stopRequested) return;
      server.handleClient();
      yield();
    }
  }

  if (resumeIterationsLeft > 0) {
    runRotation(resumeIterationsLeft, resumeDegPerInterval);
  }
}

// --- Handler: Return to 0 ---
// CHANGE 2 applied here too
void handleGoHome() {
  addCORSHeaders();
  server.send(200, "text/plain", "Returning Home...");

  if (currentAngle == 0) return;

  int stepsBack = abs((currentAngle / 360.0) * totalStepsPerRev);
  bool returnDirection = (currentAngle < 0);

  stopRequested = false;
  resumeStepsLeft = 0;
  resumeIterationsLeft = 0;

  moveStepper(stepsBack, returnDirection);
  currentAngle = 0;
  Serial.println("Home position reached.");
}

// --- Handler: Pause ---
// CHANGE 2 applied here too
void handlePause() {
  addCORSHeaders();
  stopRequested = true;
  server.send(200, "text/plain", "MOTOR PAUSED — Rotation stopped.");
  Serial.println("Pause requested via web.");
}

// --- Handler: Get Angle ---
// CHANGE 2 applied here too
void handleGetAngle() {
  addCORSHeaders();
  server.send(200, "text/plain", String(currentAngle));
}

// -------------------------------------------------------
// CHANGE 3: Added OPTIONS preflight handler.
// WHY: Before sending a real cross-origin request, browsers
// first send an OPTIONS "preflight" request to check if
// the server allows it. If the ESP32 doesn't respond to
// OPTIONS, the browser never sends the actual command.
// -------------------------------------------------------
void handleOptions() {
  addCORSHeaders();
  server.send(204);
}

void setup() {
  Serial.begin(115200);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());

  // -------------------------------------------------------
  // CHANGE 4: Removed the "/" route (embedded HTML page).
  // WHY: The frontend is now hosted on Vercel — the ESP32
  // no longer needs to serve it. All other routes are
  // identical to original, just with CORS headers added
  // and the getAngle lambda replaced with handleGetAngle().
  // -------------------------------------------------------
  server.on("/getAngle", HTTP_GET, handleGetAngle);
  server.on("/run",      HTTP_GET, handleRotate);
  server.on("/goHome",   HTTP_GET, handleGoHome);
  server.on("/pause",    HTTP_GET, handlePause);
  server.on("/resume",   HTTP_GET, handleResume);

  // Handle OPTIONS preflight for all routes
  server.on("/getAngle", HTTP_OPTIONS, handleOptions);
  server.on("/run",      HTTP_OPTIONS, handleOptions);
  server.on("/goHome",   HTTP_OPTIONS, handleOptions);
  server.on("/pause",    HTTP_OPTIONS, handleOptions);
  server.on("/resume",   HTTP_OPTIONS, handleOptions);

  server.begin();
}

void loop() {
  server.handleClient();
}

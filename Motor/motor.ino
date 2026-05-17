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
int resumeStepsLeft = 0;         // Steps remaining when paused
bool resumeClockwise = true;     // Direction when paused
int resumeIterationsLeft = 0;    // Iterations remaining when paused
float resumeDegPerInterval = 0;  // Degrees per interval when paused

// --- Network Credentials ---
const char* ssid = "*****";
const char* password = "*****";

WebServer server(80);

// --- Website UI ---
const char index_html[] PROGMEM = R"rawliteral(
<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>
<style>
  body{font-family:'Courier New', monospace; background:#0a0a0a; color:#00ff00; padding:20px; text-align:center;}
  .container{max-width:600px; margin:auto; border:1px solid #00ff00; padding:20px; border-radius:10px; background:#111;}
  #terminal{height:250px; overflow-y:auto; background:#000; border:1px solid #333; padding:15px; text-align:left; margin-top:20px; font-size:14px;}
  .controls { display: flex; flex-direction: column; align-items: center; gap: 10px; }
  input{background:#000; color:#00ff00; border:1px solid #00ff00; padding:10px; width:120px; border-radius:4px;}
  button{background:#00ff00; color:#000; border:none; padding:12px 24px; cursor:pointer; font-weight:bold; border-radius:4px; width: 200px;}
  .btn-home{background:#0088ff; color:white;}
  .btn-pause{background:#ff8800; color:white;}
  .btn-resume{background:#aa00ff; color:white;}
  label { font-size: 12px; margin-bottom: -5px; }
</style>
<script>
  let lastAngle = -361;
  let isPaused = false; // Track pause/resume state in UI

  setInterval(function() {
    fetch('/getAngle').then(r => r.text()).then(angle => {
      if(angle != lastAngle) {
        let term = document.getElementById('terminal');
        let timestamp = new Date().toLocaleTimeString();
        term.innerHTML += '[' + timestamp + '] > Current Angle: ' + angle + '&deg;<br>';
        term.scrollTop = term.scrollHeight;
        lastAngle = angle;
      }
    });
  }, 500);

  function togglePauseResume() {
    let btn = document.getElementById('pauseResumeBtn');
    let term = document.getElementById('terminal');
    let timestamp = new Date().toLocaleTimeString();

    if (!isPaused) {
      // Currently running → send PAUSE
      fetch('/pause').then(r => r.text()).then(msg => {
        term.innerHTML += '[' + timestamp + '] > ⚠ ' + msg + '<br>';
        term.scrollTop = term.scrollHeight;
      });
      btn.textContent = '▶ RESUME MOTOR';
      btn.classList.remove('btn-pause');
      btn.classList.add('btn-resume');
      isPaused = true;
    } else {
      // Currently paused → send RESUME
      fetch('/resume').then(r => r.text()).then(msg => {
        term.innerHTML += '[' + timestamp + '] > ▶ ' + msg + '<br>';
        term.scrollTop = term.scrollHeight;
      });
      btn.textContent = '❚❚ PAUSE MOTOR';
      btn.classList.remove('btn-resume');
      btn.classList.add('btn-pause');
      isPaused = false;
    }
  }

  // Reset button to PAUSE state when START is clicked
  function onStart() {
    let btn = document.getElementById('pauseResumeBtn');
    btn.textContent = '❚❚ PAUSE MOTOR';
    btn.classList.remove('btn-resume');
    btn.classList.add('btn-pause');
    isPaused = false;
  }
</script>
</head><body><div class='container'>
  <h2>ANTENNA COMMAND CENTER</h2>
  <form class='controls' action='/run' method='GET' target='hidden-frame' onsubmit='onStart()'>
    <label>Degrees per Move</label>
    <input type='number' name='deg' value='10'>
    <label>Repeat Count</label>
    <input type='number' name='num' value='1'>
    <button type='submit'>START ROTATION SEQUENCE</button>
  </form>
  <div class='controls' style='margin-top:10px;'>
    <button class="btn-home" onclick="fetch('/goHome')">RETURN TO 0&deg; (HOME)</button>
    <button id='pauseResumeBtn' class='btn-pause' onclick='togglePauseResume()'>&#9646;&#9646; PAUSE MOTOR</button>
  </div>
  <div id='terminal'>> System Online. WiFi Connected<br></div>
  <iframe name='hidden-frame' style='display:none;'></iframe>
</div></body></html>
)rawliteral";

// --- Helper: Core Motor Movement with resume tracking ---
void moveStepper(int steps, bool clockwise) {
  digitalWrite(DIR_PIN, clockwise ? HIGH : LOW);
  for (int i = 0; i < steps; i++) {

    if (stopRequested) {
      resumeStepsLeft = steps - i;  // Save exactly how many steps remain
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
  resumeStepsLeft = 0;  // Completed cleanly, no steps left
}

// --- Shared rotation runner (used by both handleRotate and handleResume) ---
void runRotation(int iterations, float degPerInterval) {
  bool clockwise = (degPerInterval > 0);
  int stepsToMove = abs((degPerInterval / 360.0) * totalStepsPerRev);

  for (int i = 0; i < iterations; i++) {
    if (stopRequested) {
      // Save remaining iterations for resume
      resumeIterationsLeft = iterations - i;
      resumeDegPerInterval = degPerInterval;
      Serial.println("Rotation sequence paused between intervals.");
      return;
    }

    // First iteration may use overridden step count (resumed mid-move)
    int steps = stepsToMove;

    moveStepper(steps, clockwise);

    if (stopRequested) {
      // Paused mid-move — save remaining iterations
      resumeIterationsLeft = iterations - i;  // current iteration not done
      resumeDegPerInterval = degPerInterval;
      return;
    }

    currentAngle += (int)degPerInterval;

    unsigned long startPause = millis();
    while (millis() - startPause < 30000) {
      if (stopRequested) {
        resumeIterationsLeft = iterations - i - 1;  // current done, rest remain
        resumeDegPerInterval = degPerInterval;
        return;
      }
      server.handleClient();
      yield();
    }
  }

  // All done cleanly
  resumeIterationsLeft = 0;
  resumeStepsLeft = 0;
}

// --- Handler: Start Rotation ---
void handleRotate() {
  if (server.hasArg("num") && server.hasArg("deg")) {
    int iterations = server.arg("num").toInt();
    float degPerInterval = server.arg("deg").toFloat();

    stopRequested = false;
    resumeStepsLeft = 0;
    resumeIterationsLeft = 0;

    server.send(200, "text/plain", "OK");
    runRotation(iterations, degPerInterval);
  }
}

// --- Handler: Resume from paused state ---
void handleResume() {
  if (resumeIterationsLeft == 0 && resumeStepsLeft == 0) {
    server.send(200, "text/plain", "Nothing to resume.");
    return;
  }

  stopRequested = false;
  server.send(200, "text/plain", "MOTOR RESUMED — Continuing from paused position.");
  Serial.println("Resuming motor...");

  if (resumeStepsLeft > 0) {
    // Resume the incomplete step burst first
    moveStepper(resumeStepsLeft, resumeClockwise);
    resumeStepsLeft = 0;

    if (stopRequested) return;  // Paused again mid-resume

    currentAngle += (int)resumeDegPerInterval;

    // Wait 1 second before continuing remaining iterations
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
void handleGoHome() {
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
void handlePause() {
  stopRequested = true;
  server.send(200, "text/plain", "MOTOR PAUSED — Rotation stopped.");
  Serial.println("Pause requested via web.");
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

  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });
  server.on("/getAngle", []() {
    server.send(200, "text/plain", String(currentAngle));
  });
  server.on("/run", handleRotate);
  server.on("/goHome", handleGoHome);
  server.on("/pause", handlePause);
  server.on("/resume", handleResume);  // ← NEW

  server.begin();
}

void loop() {
  server.handleClient();
}
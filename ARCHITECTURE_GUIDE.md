# 🏗️ SYSTEM ARCHITECTURE & DEPLOYMENT GUIDE

## 📐 System Architecture Diagram

```
┌────────────────────────────────────────────────────────────────────┐
│                        INTERNET / CLOUD                             │
│  ┌──────────────────────────┐    ┌─────────────────────────────┐   │
│  │   VERCEL (Website)       │    │  RENDER (OCR API)           │   │
│  │  antenna-system.vercel.app    │  antenna-ocr-api.onrender.com  │
│  │                          │    │                             │   │
│  │  React.js App           │    │  Python FastAPI             │   │
│  │  - UI Controls          │    │  - Pytesseract OCR          │   │
│  │  - Data Display         │    │  - Image Processing         │   │
│  │  - Visualization        │    │  - Value Extraction         │   │
│  └──────────────────────────┘    └─────────────────────────────┘   │
│           △                               △                         │
│           │                               │                         │
│           │ (HTTP REST)                   │ (Image Upload)          │
│           │                               │                         │
└───────────┼───────────────────────────────┼─────────────────────────┘
            │                               │
            │ (WiFi Local Network)          │
            │                               │
┌───────────▼───────────────────────────────▼─────────────────────────┐
│                      LOCAL NETWORK (Home/Lab)                        │
│                                                                      │
│    ┌──────────────────────────────────────────────────────────┐    │
│    │            ESP32 Microcontroller (192.168.1.XXX)         │    │
│    │                                                           │    │
│    │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │    │
│    │  │  A4988 Motor │  │  ESP32-CAM   │  │  Web Server  │   │    │
│    │  │   Driver     │  │   Module     │  │              │   │    │
│    │  │              │  │              │  │  (Port 80)   │   │    │
│    │  │  Controls    │  │  Captures    │  │              │   │    │
│    │  │  NEMA 17     │  │  Images      │  │  Hosts UI    │   │    │
│    │  │  Stepper     │  │  for OCR     │  │              │   │    │
│    │  └──────────────┘  └──────────────┘  └──────────────┘   │    │
│    │         △                △                    △             │    │
│    │         │                │                    │             │    │
│    │         └────────────────┼────────────────────┘             │    │
│    │                          │                                  │    │
│    │                   Rotates Motor                             │    │
│    │                   Captures Images                           │    │
│    │                   Sends to API                              │    │
│    │                                                              │    │
│    └──────────────────────────────────────────────────────────┘    │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
            △
            │ (REST API Calls)
            │
    ┌───────▼──────────────────┐
    │  FIREBASE (Database)     │
    │  Realtime Database       │
    │  - Measurements Storage  │
    │  - Real-time Sync        │
    │  - Accessible to Web     │
    └──────────────────────────┘
```

---

## 🔄 Data Flow Diagram

```
USER CLICKS "START" ON WEBSITE
    ↓
WEBSITE SENDS HTTP REQUEST TO ESP32
    ↓
ESP32 ROTATES MOTOR TO 0°
    ↓
WAIT 30 SECONDS (field stabilization)
    ↓
ESP32-CAM CAPTURES IMAGE
    ↓
IMAGE SENT TO PYTHON API ON RENDER
    ↓
PYTESSERACT EXTRACTS NUMBER
    ↓
API RETURNS EXTRACTED VALUE
    ↓
ESP32 RECEIVES VALUE
    ↓
STORES IN FIREBASE (angle + value + timestamp)
    ↓
WEBSITE QUERIES FIREBASE
    ↓
DATA DISPLAYED IN TABLE & GRAPH
    ↓
REPEAT FOR NEXT ANGLE (10°, 20°, 30°... 360°)
    ↓
MEASUREMENT CYCLE COMPLETE
```

---

## 📍 Component Locations

### **Vercel (React Website)**
```
URL: https://antenna-system.vercel.app
Purpose: User Interface & Control Panel
Contains: 
  - Start/Pause/Reset buttons
  - Settings input fields
  - Real-time data display
  - CSV export
  - Firebase integration

Access: Browser (any device on internet)
Deployment: Git push to Vercel
Uptime: 99.9% SLA
```

### **Render (Python API)**
```
URL: https://antenna-ocr-api.onrender.com
Purpose: OCR Image Processing
Contains:
  - FastAPI server
  - Pytesseract integration
  - Image preprocessing
  - Number extraction

Access: HTTP POST requests (local + cloud)
Deployment: GitHub push to Render
Uptime: 99.5% on free tier
```

### **Firebase (Database)**
```
URL: https://your-project.firebaseio.com
Purpose: Data Storage & Real-time Sync
Contains:
  - Measurement records
  - Angles & readings
  - Timestamps

Access: REST API (Vercel + ESP32)
Deployment: Firebase Console
Storage: 1GB free tier
```

### **Local Network (ESP32)**
```
IP: 192.168.1.XXX (find in Serial Monitor)
Purpose: Hardware Control & Image Capture
Contains:
  - Motor driver (A4988)
  - Stepper motor (NEMA 17)
  - Camera module (ESP32-CAM)

Access: HTTP on local WiFi only
Deployment: Arduino IDE upload
Uptime: Depends on power supply
```

---

## 🚀 Deployment Steps (Visual)

### **Phase 1: Local Setup** (Week 1)

```
Day 1-2: Get Hardware
├─ ESP32 + A4988 + NEMA 17
├─ ESP32-CAM module
└─ Power supply

Day 3-4: Setup Development
├─ Install Node.js
├─ Install Arduino IDE
├─ Install Python
└─ Create GitHub account

Day 5: Code Setup
├─ Download all 19 files
├─ Organize folder structure
└─ No modifications yet
```

### **Phase 2: Cloud Deployment** (Week 2)

```
Day 1: React on Vercel
├─ Push react-app to GitHub
├─ Connect GitHub to Vercel
├─ Deploy (auto on push)
└─ Live at: https://antenna-system.vercel.app

Day 2: API on Render
├─ Push render-api to GitHub
├─ Connect to Render
├─ Deploy (auto on push)
└─ Live at: https://antenna-ocr-api.onrender.com

Day 3: Firebase Setup
├─ Create Firebase project
├─ Setup Realtime Database
├─ Get credentials
└─ Add to .env.local
```

### **Phase 3: Hardware Setup** (Week 2-3)

```
Day 1-2: Circuit Assembly
├─ ESP32 + A4988 + NEMA 17 wiring
├─ ESP32-CAM module connection
├─ Power supply connections
└─ Double-check all connections

Day 3-4: Arduino Upload
├─ Update WiFi credentials
├─ Update GPIO pin assignments
├─ Upload to ESP32
├─ Check Serial Monitor
└─ Note down IP address

Day 5: Component Testing
├─ Test motor rotation
├─ Test camera capture
├─ Test web server
└─ Verify all works
```

### **Phase 4: Integration** (Week 3-4)

```
Day 1: Website Connection
├─ Open React website
├─ Enter ESP32 IP
├─ Click Connect
└─ Should show "Connected"

Day 2: API Testing
├─ Capture test image
├─ Send to OCR API
├─ Verify extraction
└─ Check API responds

Day 3: Full System Test
├─ Set degrees per move: 10
├─ Set repeat count: 36
├─ Click Start
├─ Monitor all components
└─ Check Firebase data

Day 4: Verification
├─ Website shows data ✓
├─ Firebase has records ✓
├─ CSV export works ✓
└─ System ready ✓
```

---

## 🔐 Network Architecture

### **Public Access** (Internet)
```
User's Browser → Vercel → Google Cloud
                    ↓
              Firebase (Realtime DB)
```

### **Local Access** (Same WiFi)
```
Website (Browser) → ESP32 (192.168.1.XXX)
    ↓
ESP32 → Render API (HTTPS → Internet)
    ↓
ESP32 → Firebase (HTTPS → Internet)
```

### **Isolation Layer**
```
Firebase Realtime DB acts as intermediary
- Website gets data FROM Firebase
- ESP32 puts data TO Firebase
- No direct connection needed
- Firewall-friendly
```

---

## 📊 Technology Stack by Layer

### **Frontend Layer** (Vercel)
```
Technology: React 18.2
Language: JavaScript/JSX
Styling: CSS3 + Gradients
State: React Hooks
API: Firebase SDK
Hosting: Vercel (automatic CI/CD)
Domain: *.vercel.app
```

### **Backend Layer** (Render)
```
Technology: Python 3.11
Framework: FastAPI
Libraries: OpenCV, Pytesseract, Pillow
Async: Uvicorn ASGI server
Hosting: Render (automatic CI/CD)
Domain: *.onrender.com
```

### **Embedded Layer** (Local)
```
Microcontroller: ESP32
Language: C++
Framework: Arduino IDE
Drivers: A4988 (motor), ESP32-CAM (camera)
Network: WiFi 802.11b/g/n (2.4GHz)
Port: 80 (HTTP)
```

### **Data Layer** (Firebase)
```
Service: Firebase Realtime Database
Database: NoSQL JSON
SDK: Firebase Admin SDK
Protocol: HTTPS
Sync: Real-time listeners
Authentication: None (test mode)
```

---

## ⚡ Performance Specs

| Metric | Value | Notes |
|--------|-------|-------|
| Motor Speed | 1000 μs/step | Adjustable |
| Motor Resolution | 3.33 steps/degree | ~10.8° per step |
| Field Stabilization | 30 seconds | Per angle |
| OCR Processing | <5 seconds | Render API |
| Database Sync | <100ms | Firebase real-time |
| Website Load Time | <2 seconds | Vercel CDN |
| Full 360° Cycle | ~20-30 minutes | 36 positions × 30-35s each |

---

## 🔌 Connectivity Matrix

```
┌─────────────┬─────────────┬─────────────┬──────────────┐
│ Component   │ Vercel      │ Render      │ Firebase     │
├─────────────┼─────────────┼─────────────┼──────────────┤
│ ESP32       │ WiFi HTTP   │ WiFi HTTPS  │ WiFi HTTPS   │
│ React       │ Local       │ WiFi HTTPS  │ Local SDK    │
│ Render API  │ HTTPS       │ Local       │ HTTPS        │
│ Firebase    │ Local SDK   │ HTTPS       │ Local        │
└─────────────┴─────────────┴─────────────┴──────────────┘
```

---

## 🎯 File Distribution Across Platforms

### **Vercel Hosts**
- AntennaControl.jsx
- AntennaControl.css
- firebase-config.js
- App.jsx, App.css
- index.js, index.css
- public/index.html
- package.json

### **Render Hosts**
- app.py
- requirements.txt
- Dockerfile
- render.yaml

### **Local Storage (ESP32)**
- antenna_esp32.ino (uploaded to microcontroller)

### **Your Computer**
- All configuration files
- .env.local (NOT uploaded to any platform)
- Local development versions

### **Firebase Hosts**
- Database data (measurements)
- No code stored here

---

## 🔄 Deployment Pipeline

### **GitHub → Vercel**
```
Push to GitHub
        ↓
Vercel webhook triggered
        ↓
Vercel builds project
        ↓
npm install & npm build
        ↓
Deploy to CDN
        ↓
Live update (60 seconds)
```

### **GitHub → Render**
```
Push to GitHub
        ↓
Render webhook triggered
        ↓
Render builds project
        ↓
pip install from requirements.txt
        ↓
Start uvicorn server
        ↓
Live update (2-3 minutes)
```

### **Local → ESP32**
```
Edit antenna_esp32.ino
        ↓
Connect ESP32 via USB
        ↓
Arduino IDE compiles
        ↓
Arduino IDE uploads
        ↓
ESP32 restarts
        ↓
Live update (instant)
```

---

## 🛡️ Security Layers

### **Public Layer** (Vercel)
- HTTPS enforced
- DDoS protection
- Auto-scaling
- Firewall included

### **API Layer** (Render)
- HTTPS enforced
- Basic rate limiting available
- Container isolated
- Port 8000 open

### **Database Layer** (Firebase)
- HTTPS only
- Test mode (no auth)
- Rules-based access control
- Backup automatic

### **Local Layer** (ESP32)
- Local WiFi only
- HTTP (no HTTPS needed on LAN)
- No authentication
- Firewall-protected by router

---

## 📈 Scalability

### **Current Setup**
- 1 ESP32 (1 antenna)
- ~1000 measurements/cycle
- Firebase free tier (1GB)
- Render free tier (sync only)

### **Scaling to Multiple Antennas**
```
Multiple ESP32 boards
        ↓
Each submits to Firebase
        ↓
Website queries all boards
        ↓
Compare results
        ↓
Generate reports
```

### **Scaling to Production**
```
Multiple measurement sites
        ↓
Upgrade Firebase to paid
        ↓
Upgrade Render to dedicated instance
        ↓
Add CDN for faster access
        ↓
Add monitoring/logging
```

---

## 🧪 Testing Endpoints

### **Test Vercel Website**
```
https://antenna-system.vercel.app
Expected: Login page loads
```

### **Test Render API**
```
https://antenna-ocr-api.onrender.com/health
Expected: {"status":"healthy",...}
```

### **Test ESP32**
```
http://192.168.1.XXX/
Expected: Control panel HTML
```

### **Test Firebase**
```
Firebase Console → Realtime Database
Expected: measurements folder (initially empty)
```

---

## ✅ Deployment Verification Checklist

- [ ] React website loads at Vercel URL
- [ ] API responds to /health endpoint
- [ ] ESP32 accessible at local IP
- [ ] Firebase database created
- [ ] .env.local updated in Vercel settings
- [ ] Motor rotates on command
- [ ] Camera captures images
- [ ] OCR extraction works
- [ ] Data saves to Firebase
- [ ] Website displays measurements

---

## 📞 Troubleshooting by Layer

### **Vercel Issues**
- Check deployment logs in dashboard
- Verify environment variables added
- Clear browser cache
- Check Firebase credentials

### **Render Issues**
- Check logs in Render dashboard
- Verify requirements.txt is complete
- Test with curl or Postman
- Free tier has 15-minute auto-sleep

### **ESP32 Issues**
- Check Serial Monitor (9600 baud)
- Verify WiFi credentials
- Confirm GPIO pins
- Check power supply voltage

### **Firebase Issues**
- Verify database rules
- Check credentials format
- Confirm database URL
- Check real-time listeners

---

## 🎉 System Ready for Production!

Once all tests pass:
1. ✅ Fully automated
2. ✅ Cloud-based processing
3. ✅ Real-time data sync
4. ✅ No laptop needed after startup
5. ✅ Scalable architecture
6. ✅ Professional grade

**Total Setup Time**: 2-4 weeks  
**Total Cost**: ~$50 (hardware only)  
**Operational Cost**: Free (using free tiers)

---

**Architecture Version**: 1.0.0  
**Status**: Production Ready ✅

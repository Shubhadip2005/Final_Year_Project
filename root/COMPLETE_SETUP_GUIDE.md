# 🛰️ RF ANTENNA AUTOMATION SYSTEM - COMPLETE SETUP GUIDE

## 📋 Table of Contents
1. [React.js Website (Vercel)](#1-reactjs-website-vercel)
2. [Python API (Render)](#2-python-api-render)
3. [ESP32 Firmware](#3-esp32-firmware)
4. [Firebase Database](#4-firebase-database)
5. [Integration & Testing](#5-integration--testing)

---

## 1️⃣ React.js Website (Vercel)

### Prerequisites
- Node.js 16+ installed ([Download](https://nodejs.org))
- Git account ([GitHub](https://github.com))
- Vercel account ([Sign up](https://vercel.com))

### Step 1: Setup React App Locally

```bash
# Navigate to react-app directory
cd react-app

# Install dependencies
npm install

# Create .env.local file with Firebase credentials
cp .env.local.example .env.local

# Edit .env.local with your Firebase config
nano .env.local
```

### Step 2: Test Locally

```bash
# Start development server
npm start

# Should open http://localhost:3000
# You should see the antenna control interface
```

### Step 3: Deploy to Vercel

**Option A: Using Vercel CLI**

```bash
# Install Vercel CLI
npm i -g vercel

# Login to Vercel
vercel login

# Deploy
vercel

# Follow prompts:
# - Scope: Your personal account
# - Project name: antenna-control-system
# - Framework: Create React App
```

**Option B: Using GitHub (Recommended)**

```bash
# 1. Create GitHub repository
# Go to https://github.com/new
# Name: antenna-control-system
# Make it PUBLIC

# 2. Push code to GitHub
git init
git add .
git commit -m "Initial commit"
git branch -M main
git remote add origin https://github.com/YOUR_USERNAME/antenna-control-system.git
git push -u origin main

# 3. Deploy on Vercel
# Go to https://vercel.com/new
# Import from GitHub
# Select your repository
# Click Deploy
```

### Step 4: Add Environment Variables to Vercel

1. Go to Vercel Dashboard
2. Click on your project
3. **Settings → Environment Variables**
4. Add these variables:
   - `REACT_APP_FIREBASE_API_KEY`
   - `REACT_APP_FIREBASE_AUTH_DOMAIN`
   - `REACT_APP_FIREBASE_PROJECT_ID`
   - etc. (all from your `.env.local`)

### ✅ Result
Your website will be live at: `https://your-project-name.vercel.app`

---

## 2️⃣ Python API (Render)

### Prerequisites
- GitHub account with code pushed
- Render account ([Sign up](https://render.com))

### Step 1: Push Python API to GitHub

```bash
# Create new GitHub repository for API
# Go to https://github.com/new
# Name: antenna-ocr-api

cd render-api

git init
git add .
git commit -m "Initial OCR API"
git branch -M main
git remote add origin https://github.com/YOUR_USERNAME/antenna-ocr-api.git
git push -u origin main
```

### Step 2: Deploy on Render

1. Go to **https://render.com**
2. Click **New +** → **Web Service**
3. Select **GitHub** (connect your account if needed)
4. Search for `antenna-ocr-api` repository
5. Click **Connect**
6. Configuration:
   - **Name**: `antenna-ocr-api`
   - **Environment**: `Python 3`
   - **Region**: `Ohio` (closest to you)
   - **Branch**: `main`
   - **Build Command**: `pip install -r requirements.txt`
   - **Start Command**: `uvicorn app:app --host 0.0.0.0 --port 8000`
7. Click **Create Web Service**

### Step 3: Wait for Deployment

- Render will build and deploy automatically
- Check **Logs** tab for progress
- Once deployed, you'll see: "Your service is live"

### ✅ Result
Your API will be live at: `https://antenna-ocr-api.onrender.com`

Test it:
```bash
curl https://antenna-ocr-api.onrender.com/health

# Should return:
# {"status":"healthy","service":"Antenna OCR API","version":"1.0.0"}
```

---

## 3️⃣ ESP32 Firmware

### Prerequisites
- Arduino IDE ([Download](https://www.arduino.cc/en/software))
- ESP32 board drivers installed
- USB cable for ESP32

### Step 1: Install Arduino IDE & Drivers

```bash
# 1. Download Arduino IDE
# https://www.arduino.cc/en/software

# 2. Open Arduino IDE
# Go to Preferences
# Add this URL under "Additional Boards Manager URLs":
# https://dl.espressif.com/dl/package_esp32_index.json

# 3. Go to Tools → Board Manager
# Search "esp32"
# Install "esp32" by Espressif Systems
```

### Step 2: Install Required Libraries

In Arduino IDE:
- **Sketch → Include Library → Manage Libraries**
- Search and install:
  - `ArduinoJson` (by Benoit Blanchon)
  - `ESP32 CAM` (built-in)

### Step 3: Update Firmware with Your Settings

Open `antenna_esp32.ino` and edit:

```cpp
// Line 13-14: Your WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Line 32: GPIO pin for motor control (adjust if needed)
#define STEP_PIN 19
#define DIR_PIN 18
#define ENABLE_PIN 5

// Line 37: Steps per degree (adjust based on your motor)
#define STEPS_PER_DEGREE 3.33
```

### Step 4: Upload to ESP32

1. Connect ESP32 to computer via USB
2. **Tools → Board → ESP32 Dev Module**
3. **Tools → Port** → Select your COM port
4. Click **Upload** (→ arrow button)
5. Wait for upload to complete

### Step 5: Open Serial Monitor

1. **Tools → Serial Monitor**
2. Set baud rate to **115200**
3. Press ESP32 reset button
4. You should see:
```
[ANTENNA SYSTEM] Initializing...
[WIFI] Connecting to network...
[CAMERA] Initialized!
[SERVER] Web server started on port 80
[ANTENNA SYSTEM] Ready!
```

### Step 6: Find ESP32 IP Address

In Serial Monitor, you'll see:
```
[WIFI] Connected!
IP Address: 192.168.1.XXX
```

**Note this IP - you'll need it in the React app!**

### ✅ Test ESP32

Open browser and go to:
```
http://192.168.1.XXX/
```

You should see a control panel to test motor and camera.

---

## 4️⃣ Firebase Database

### Step 1: Create Firebase Project

Follow the guide in `firebase-config/FIREBASE_SETUP.md`

Key steps:
1. Create Firebase project
2. Set up Realtime Database (Test Mode)
3. Update database rules
4. Get Firebase config

### Step 2: Update React App

In your React app's `.env.local`:

```
REACT_APP_FIREBASE_API_KEY=your_key_here
REACT_APP_FIREBASE_AUTH_DOMAIN=your-project.firebaseapp.com
REACT_APP_FIREBASE_PROJECT_ID=your-project-id
REACT_APP_FIREBASE_STORAGE_BUCKET=your-project.appspot.com
REACT_APP_FIREBASE_MESSAGING_SENDER_ID=123456789
REACT_APP_FIREBASE_APP_ID=1:123456789:web:abc123
REACT_APP_FIREBASE_DATABASE_URL=https://your-project.firebaseio.com
```

### ✅ Test Firebase

In React app, you should see connection logs in browser console.

---

## 5️⃣ Integration & Testing

### Prerequisites Checklist

- ✅ React app deployed on Vercel
- ✅ Python API deployed on Render
- ✅ ESP32 firmware uploaded
- ✅ Firebase database created
- ✅ All environment variables set

### Integration Test

1. **Open React website** (Vercel link)

2. **Find ESP32 IP Address**
   - Check Serial Monitor from Arduino IDE
   - Should be something like `192.168.1.100`

3. **Connect to ESP32**
   - In website, enter IP address: `192.168.1.100`
   - Click **Connect**
   - Should show "Connected ✓"

4. **Test Motor Rotation**
   - Click **Reset to 0°**
   - Motor should rotate to 0°
   - Website should update angle display

5. **Test Full Cycle**
   - Set **Degrees per Move**: 90
   - Set **Repeat Count**: 4
   - Click **Start**
   - System should:
     - Rotate to 0°, wait 30s, capture image, extract data
     - Rotate to 90°, wait 30s, capture image, extract data
     - Continue for 180°, 270°, 360°

6. **Check Firebase**
   - In Firebase Console → Realtime Database
   - You should see measurements appearing:
     ```
     measurements/
     ├── 1234567890/
     │   ├── angle: 0
     │   ├── current_reading: 14.7871
     │   └── timestamp: "2024-01-15T..."
     ```

7. **View Data in Website**
   - Go to **Measurements** section
   - Should show table with angles and readings
   - Can export as CSV

---

## 🚨 Troubleshooting

### React App Issues

**Problem**: Blank page or errors  
**Solution**:
1. Check browser console (F12)
2. Verify `.env.local` has all Firebase variables
3. Run `npm install` again
4. Clear browser cache

**Problem**: Can't connect to ESP32  
**Solution**:
1. Verify ESP32 IP is correct
2. Check ESP32 is on same WiFi network
3. Verify motors are powered on
4. Check Serial Monitor for ESP32 status

### Python API Issues

**Problem**: "Service unavailable" error  
**Solution**:
1. Check Render dashboard for deployment status
2. Look at Logs tab for errors
3. Verify all dependencies in `requirements.txt`

**Problem**: OCR not extracting numbers  
**Solution**:
1. Check image quality from camera
2. Adjust image processing in `app.py`
3. Test with different meter types

### ESP32 Issues

**Problem**: Upload fails  
**Solution**:
1. Try different USB cable
2. Change board to "ESP32 Dev Module"
3. Reduce upload speed (Tools → Upload Speed → 115200)

**Problem**: WiFi connection fails  
**Solution**:
1. Double-check SSID and password
2. Make sure WiFi is 2.4GHz (not 5GHz)
3. Check ESP32 antenna placement

### Firebase Issues

**Problem**: "Permission denied" error  
**Solution**:
1. Update database rules (from FIREBASE_SETUP.md)
2. Check API key is correct
3. Verify Firebase URL format

---

## 📊 System Architecture

```
┌─────────────────────────────────────────────┐
│  React.js Website (Vercel)                  │
│  https://antenna-system.vercel.app          │
│  - User Interface                           │
│  - Motor Control                            │
│  - Data Visualization                       │
└──────────────┬──────────────────────────────┘
               │ HTTP REST
               ▼
┌─────────────────────────────────────────────┐
│  ESP32 Microcontroller (Local Network)      │
│  http://192.168.1.XXX:80                    │
│  - Motor Control (A4988 + NEMA17)           │
│  - Camera (ESP32-CAM)                       │
│  - Image Capture & Send                     │
└──────┬────────────────────────┬─────────────┘
       │ Image Transfer          │ Store Data
       ▼                         ▼
┌─────────────────────────┐ ┌─────────────────────────────┐
│ Python OCR API (Render) │ │ Firebase Realtime Database  │
│ https://antenna-ocr-api │ │ - Measurements             │
│ .onrender.com          │ │ - Angles & Readings        │
│ - Pytesseract OCR      │ │ - Timestamps               │
│ - Image Processing     │ └─────────────────────────────┘
└────────────────────────┘
```

---

## 📝 File Structure

```
antenna-system/
├── react-app/                 # Website
│   ├── src/
│   │   ├── AntennaControl.jsx
│   │   ├── AntennaControl.css
│   │   ├── firebase-config.js
│   │   ├── App.jsx
│   │   └── index.js
│   ├── public/
│   │   └── index.html
│   ├── package.json
│   └── .env.local.example
│
├── render-api/                # Python OCR API
│   ├── app.py
│   ├── requirements.txt
│   ├── Dockerfile
│   ├── render.yaml
│   └── .gitignore
│
├── esp32-firmware/            # ESP32 Code
│   └── antenna_esp32.ino
│
├── firebase-config/           # Firebase Setup
│   └── FIREBASE_SETUP.md
│
└── COMPLETE_SETUP_GUIDE.md   # This file
```

---

## 🎓 API Endpoints

### React Website
- `GET / ` - Main interface
- `POST /start` - Start measurement cycle
- `POST /pause` - Pause measurement
- `POST /resume` - Resume measurement

### ESP32 (Local)
- `GET /` - Control panel
- `GET /status` - Current status
- `POST /control?cmd=rotate&angle=45` - Rotate to angle
- `GET /capture` - Capture image
- `POST /settings` - Update settings

### Python API (Render)
- `POST /extract-ocr` - Extract from single image
- `POST /extract-ocr-batch` - Extract from multiple images
- `GET /health` - Health check
- `GET /` - API documentation

### Firebase
- `measurements/` - All measurements
- `measurements/{timestamp}/` - Individual measurement

---

## 🔒 Security Notes

- This setup uses Firebase Test Mode (no authentication)
- For production, implement Firebase authentication
- Consider rate limiting on the Python API
- Protect ESP32 IP address (local network only)

---

## 📞 Support

If you encounter issues:
1. Check troubleshooting section above
2. Review service logs (Vercel, Render dashboards)
3. Check browser DevTools Console (F12)
4. Check Arduino IDE Serial Monitor

---

## 🎉 Success!

If everything is working:
1. ✅ Website loads and shows UI
2. ✅ Can enter ESP32 IP and connect
3. ✅ Motor rotates when you click buttons
4. ✅ Images captured and OCR extracts values
5. ✅ Data appears in Firebase and website

**Congratulations! Your RF Antenna Automation System is ready!** 🛰️
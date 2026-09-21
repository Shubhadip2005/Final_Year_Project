import React, { useState, useEffect } from 'react';
import { db } from './firebase-config';
import { ref, onValue, set, remove } from 'firebase/database';
import './AntennaControl.css';

const AntennaControl = () => {
  // ═══════════════════════════════════════════════════════════════════
  // STATIC IP ADDRESSES
  // ═══════════════════════════════════════════════════════════════════
  const MAIN_ESP32_IP = '10.135.98.50';        // Main ESP32 (Motor)
  const ESP32_CAM_IP = '10.135.98.51';         // ESP32-CAM (Camera)
  
  // State Management
  const [isConnected, setIsConnected] = useState(false);
  const [isRunning, setIsRunning] = useState(false);
  const [currentAngle, setCurrentAngle] = useState(0);
  const [degreesPerMove, setDegreesPerMove] = useState(10);
  const [repeatCount, setRepeatCount] = useState(36);
  const [measurements, setMeasurements] = useState([]);
  const [status, setStatus] = useState('Ready to connect');
  const [progress, setProgress] = useState(0);
  const [currentMeasurementIndex, setCurrentMeasurementIndex] = useState(0);
  const isRunningRef = React.useRef(false);
  // Firebase: Listen to measurements
  useEffect(() => {
    const measurementsRef = ref(db, 'measurements');
    const unsubscribe = onValue(measurementsRef, (snapshot) => {
      if (snapshot.exists()) {
        const data = snapshot.val();
        const measurementsArray = Object.keys(data).map((key) => ({
          id: key,
          ...data[key],
        }));
        setMeasurements(measurementsArray.sort((a, b) => a.angle - b.angle));
      }
    });
    return () => unsubscribe();
  }, []);

  // ═══════════════════════════════════════════════════════════════════
  // NON-BLOCKING WAIT WITH COUNTDOWN
  // ═══════════════════════════════════════════════════════════════════
  const waitWithProgress = (seconds, angle) => {
    return new Promise((resolve) => {
      let remaining = seconds;
      const interval = setInterval(() => {
        remaining--;
        setStatus(`⏳ Waiting ${remaining}s at ${angle}° for field stabilization...`);
        if (remaining <= 0) {
          clearInterval(interval);
          resolve();
        }
      }, 1000);
    });
  };

  // ═══════════════════════════════════════════════════════════════════
  // VALIDATE DEVICE CONNECTION
  // ═══════════════════════════════════════════════════════════════════
  const validateMotor = async () => {
    try {
      const res = await fetch(`http://${MAIN_ESP32_IP}/ping`, { 
        signal: AbortSignal.timeout(5000) 
      });
      console.log('[VALIDATE] Motor ping:', res.ok);
      return res.ok;
    } catch (error) { 
      console.error('[VALIDATE] Motor failed:', error.message);
      return false; 
    }
  };

  const validateCamera = async () => {
    try {
      console.log('[VALIDATE] Camera - waiting up to 10 seconds...');
      const res = await fetch(`http://${ESP32_CAM_IP}/ping`, { 
        signal: AbortSignal.timeout(10000)  // ✅ INCREASED FROM 5000
      });
      console.log('[VALIDATE] Camera ping:', res.ok);
      return res.ok;
    } catch (error) { 
      console.error('[VALIDATE] Camera failed:', error.message);
      return false; 
    }
  };

  // ═══════════════════════════════════════════════════════════════════
  // 📡 CONNECTION
  // ═══════════════════════════════════════════════════════════════════

  const handleConnect = async () => {
    try {
      setStatus('Connecting to 10.135.98.50...');
      
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 5000);
      
      const response = await fetch(`http://${MAIN_ESP32_IP}/status`, {
        signal: controller.signal
      });
      
      clearTimeout(timeoutId);
      
      if (response.ok) {
        setIsConnected(true);
        setStatus('✓ Connected! Static IPs: Main=10.135.98.50, Camera=10.135.98.51');
      } else {
        throw new Error('No response from ESP32');
      }
    } catch (error) {
      setStatus('✗ Connection Failed - Check WiFi and ESP32 power');
      alert(`Error: ${error.message}\n\nMake sure:\n1. ESP32s are powered on\n2. Same WiFi network\n3. IPs: Main=10.135.98.50, Camera=10.135.98.51`);
    }
  };

  // ═══════════════════════════════════════════════════════════════════
  // SEND COMMANDS TO MOTOR ESP32
  // ═══════════════════════════════════════════════════════════════════

  const sendCommand = async (command, angle = null) => {
    if (!isConnected) {
      console.error('[ERROR] Not connected to ESP32');
      setStatus('❌ Not connected. Click Connect first.');
      return false;
    }

    try {
      let url;
      const timeout = 15000;
      
      if (angle !== null) {
        url = `http://${MAIN_ESP32_IP}/rotate?angle=${angle}`;
      } else if (command === 'reset') {
        url = `http://${MAIN_ESP32_IP}/rotate?angle=0`;
      } else if (command === 'enable') {
        url = `http://${MAIN_ESP32_IP}/enable-motor`;
      } else if (command === 'disable') {
        url = `http://${MAIN_ESP32_IP}/disable-motor`;
      } else {
        throw new Error(`Unknown command: ${command}`);
      }

      console.log(`[MOTOR] Sending: ${url}`);
      
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), timeout);
      
      const response = await fetch(url, { signal: controller.signal });
      clearTimeout(timeoutId);
      
      if (!response.ok) {
        console.error(`[ERROR] Motor command failed: HTTP ${response.status}`);
        throw new Error(`HTTP ${response.status}`);
      }
      
      const data = await response.json();
      console.log('[OK] Command response:', data);
      return true;
      
    } catch (error) {
      console.error(`[ERROR] sendCommand failed:`, error.message);
      setStatus(`❌ Motor Error: ${error.message}`);
      return false;
    }
  };

  // ═══════════════════════════════════════════════════════════════════
  // CAPTURE AND EXTRACT FROM CAMERA
  // ═══════════════════════════════════════════════════════════════════

  const captureAndExtract = async () => {
    try {
      // Step 1: Capture image
      console.log('[CAMERA] Starting capture...');
      setStatus(`📷 Capturing image...`);
      
      const captureResponse = await fetch(`http://${ESP32_CAM_IP}/capture`, {
        method: 'GET',
        signal: AbortSignal.timeout(15000)  // ✅ INCREASED FROM 10000
      });
      
      if (!captureResponse.ok) {
        console.error(`[ERROR] Capture failed: HTTP ${captureResponse.status}`);
        throw new Error(`Capture HTTP ${captureResponse.status}`);
      }
      console.log('[OK] Image captured');

      // Step 2: Extract with OCR
      console.log('[OCR] Starting extraction...');
      setStatus(`📤 Sending to OCR API...`);
      
      const extractResponse = await fetch(`http://${ESP32_CAM_IP}/extract`, {
        method: 'GET',
        signal: AbortSignal.timeout(40000)  // ✅ INCREASED FROM 20000
      });
      
      if (!extractResponse.ok) {
        console.error(`[ERROR] OCR request failed: HTTP ${extractResponse.status}`);
        throw new Error(`OCR HTTP ${extractResponse.status}`);
      }
      
      const data = await extractResponse.json();
      
      if (!data.success) {
        console.error('[ERROR] OCR returned error:', data.error);
        throw new Error(`OCR Error: ${data.error}`);
      }
      
      const value = parseFloat(data.extractedValue);
      console.log('[OK] OCR extracted:', value, 'mA');
      return value;
      
    } catch (error) {
      console.error('[FATAL] Capture/Extract failed:', error.message);
      setStatus(`❌ OCR ERROR: ${error.message}`);
      return null;
    }
  };

  // ═══════════════════════════════════════════════════════════════════
  // SAVE MEASUREMENT TO FIREBASE
  // ═══════════════════════════════════════════════════════════════════

  const saveMeasurement = async (angle, value) => {
    try {
      const measurementRef = ref(db, `measurements/${Date.now()}`);
      await set(measurementRef, {
        angle,
        current_reading: value,
        timestamp: new Date().toISOString(),
      });
      console.log('[FIREBASE] Saved:', angle, value);
    } catch (error) {
      console.error('[FIREBASE] Save error:', error);
    }
  };

  // ═══════════════════════════════════════════════════════════════════
  // FULL MEASUREMENT CYCLE
  // ═══════════════════════════════════════════════════════════════════

  const runMeasurementCycle = async (startIndex = 0) => {
    console.log(`[CYCLE] STARTING - startIndex: ${startIndex}, repeatCount: ${repeatCount}`);
    
    for (let i = startIndex; i < repeatCount; i++) {
      // ✅ CHECK REF INSTEAD OF STATE
      if (!isRunningRef.current) {
        console.log('[CYCLE] Paused by user');
        setCurrentMeasurementIndex(i);
        setStatus('⏸ Paused');
        return;
      }

      const angle = (i * degreesPerMove) % 360;
      setCurrentAngle(angle);
      
      const currentProgress = Math.round((measurements.length / repeatCount) * 100);
      setProgress(Math.min(currentProgress, 99));
      
      console.log(`[CYCLE] Step ${i + 1}/${repeatCount} at ${angle}°`);

      // Step 1: Move to angle
      setStatus(`🔄 Rotating to ${angle}°...`);
      const moved = await sendCommand('rotate', angle);
      if (!moved) {
        throw new Error(`Failed to move to ${angle}°`);
      }

      // Step 2: Wait for field stabilization
      await waitWithProgress(30, angle);

      // Step 3: Capture and extract
      setStatus(`📷 Capturing at ${angle}°...`);
      const value = await captureAndExtract();
      if (value === null) {
        throw new Error(`Failed to extract data at ${angle}°`);
      }

      // Step 4: Save to Firebase
      await saveMeasurement(angle, value);
      setStatus(`✅ Angle ${angle}°: ${value.toFixed(4)} mA`);
    }

    console.log(`[CYCLE] ALL COMPLETE`);
    setStatus('✨ Measurement cycle complete!');
    setProgress(100);
    isRunningRef.current = false;  // ✅ STOP REF
    setIsRunning(false);
    setCurrentMeasurementIndex(0);
  };

  // ═══════════════════════════════════════════════════════════════════
  // START MEASUREMENT CYCLE (WITH VALIDATION)
  // ═══════════════════════════════════════════════════════════════════

  const handleStart = async () => {
    if (!isConnected) {
      alert('Not connected to ESP32');
      return;
    }

    setStatus('🔍 Validating devices...');
    console.log('[START] Validating motor...');
    const motorOk = await validateMotor();
    
    console.log('[START] Validating camera...');
    const cameraOk = await validateCamera();
    
    if (!motorOk) {
      setStatus('❌ Motor ESP32 not responding');
      alert('Motor ESP32 not responding. Check power and connection.');
      return;
    }
    if (!cameraOk) {
      setStatus('❌ Camera ESP32 not responding');
      alert('Camera ESP32 not responding. Check power and connection.');
      return;
    }

    // ✅ SET REF FIRST
    isRunningRef.current = true;
    setIsRunning(true);
    setCurrentMeasurementIndex(0);
    setProgress(0);
    setStatus('Starting measurement cycle...');
    
    try {
      await runMeasurementCycle(0);
    } catch (error) {
      console.error('[FATAL] Cycle error:', error.message);
      setStatus(`❌ ERROR: ${error.message}`);
      alert(`Measurement failed: ${error.message}`);
    } finally {
      isRunningRef.current = false;  // ✅ STOP REF
      setIsRunning(false);
    }
  };

  // ═══════════════════════════════════════════════════════════════════
  // PAUSE MEASUREMENT
  // ═══════════════════════════════════════════════════════════════════

  const handlePause = () => {
    console.log('[PAUSE] User paused');
    isRunningRef.current = false;  // ✅ USE REF
    setIsRunning(false);
    setStatus('⏸ Paused');
  };

  // ═══════════════════════════════════════════════════════════════════
  // RESUME MEASUREMENT
  // ═══════════════════════════════════════════════════════════════════

const handleResume = async () => {
  if (isRunningRef.current) return;  // ✅ USE REF
  
  console.log('[RESUME] Resuming from index:', currentMeasurementIndex);
  isRunningRef.current = true;  // ✅ USE REF
  setIsRunning(true);
  setStatus('▶ Resuming...');
  
  try {
    await runMeasurementCycle(currentMeasurementIndex);
  } catch (error) {
    console.error('[FATAL] Resume error:', error.message);
    setStatus(`❌ ERROR: ${error.message}`);
    alert(`Resume failed: ${error.message}`);
  } finally {
    isRunningRef.current = false;  // ✅ STOP REF
    setIsRunning(false);
  }
};

  // ═══════════════════════════════════════════════════════════════════
  // RESET TO 0°
  // ═══════════════════════════════════════════════════════════════════

  const handleReset = async () => {
    console.log('[RESET] Resetting to 0°');
    const success = await sendCommand('reset');
    if (success) {
      setCurrentAngle(0);
      setStatus('↺ Reset to 0° ✓');
    }
  };

  // ═══════════════════════════════════════════════════════════════════
  // CLEAR ALL DATA
  // ═══════════════════════════════════════════════════════════════════

  const handleClearData = async () => {
  if (window.confirm('Clear all measurements from the database?')) {
    try {
      setStatus('Clearing data from database...');
      const measurementsRef = ref(db, 'measurements');
      await remove(measurementsRef);
      
      setMeasurements([]);
      setStatus('✓ Data cleared from database');
      console.log('[CLEAR] Measurements deleted from Firebase');
    } catch (error) {
      console.error('[CLEAR] Failed to delete from Firebase:', error);
      setStatus(`❌ Delete error: ${error.message}`);
      alert(`Failed to delete: ${error.message}`);
    }
  }
};

  // ═══════════════════════════════════════════════════════════════════
  // EXPORT DATA AS CSV
  // ═══════════════════════════════════════════════════════════════════

  const handleExportCSV = () => {
    if (measurements.length === 0) {
      alert('No data to export');
      return;
    }

    const csv = [
      ['Angle (°)', 'Current (mA)', 'Timestamp'],
      ...measurements.map((m) => [m.angle, m.current_reading, m.timestamp]),
    ]
      .map((row) => row.join(','))
      .join('\n');

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `antenna_measurements_${Date.now()}.csv`;
    a.click();
    console.log('[EXPORT] CSV exported');
  };

  // ═══════════════════════════════════════════════════════════════════
  // RENDER UI
  // ═══════════════════════════════════════════════════════════════════

  return (
    <div className="antenna-container">
      <header className="antenna-header">
        <h1>🛰️ RF Antenna Automation System</h1>
        <p>Automated Antenna Radiation Pattern Measurement</p>
        <p style={{fontSize: '0.9rem', opacity: 0.8}}>
          ⭐ Static IPs: Main=10.135.98.50 | Camera=10.135.98.51
        </p>
      </header>

      <div className="main-content">
        {/* LEFT SECTION: Connection & Controls */}
        <div className="control-panel">
          <section className="card">
            <h2>📡 ESP32 Connection</h2>
            <div className="input-group">
              <input
                type="text"
                placeholder="10.135.98.50 (Auto-filled)"
                value={MAIN_ESP32_IP}
                disabled={true}
                style={{backgroundColor: '#f0f0f0', cursor: 'not-allowed'}}
              />
              <button
                onClick={handleConnect}
                disabled={isConnected}
                className={`btn btn-primary ${isConnected ? 'connected' : ''}`}
              >
                {isConnected ? '✓ Connected' : 'Connect'}
              </button>
            </div>
            <p className="status-text">{status}</p>
            <p style={{fontSize: '0.85rem', color: '#666', marginTop: '10px'}}>
              ℹ️ IP addresses are FIXED (static). No need to check Serial Monitor every time!<br/>
              Main ESP32: <strong>10.135.98.50</strong><br/>
              ESP32-CAM: <strong>10.135.98.51</strong>
            </p>
          </section>

          <section className="card">
            <h2>⚙️ Settings</h2>
            <div className="setting-group">
              <label>
                Degrees per Move:
                <input
                  type="number"
                  min="1"
                  max="90"
                  value={degreesPerMove}
                  onChange={(e) => setDegreesPerMove(Number(e.target.value))}
                  disabled={isRunning}
                />
              </label>
            </div>
            <div className="setting-group">
              <label>
                Repeat Count:
                <input
                  type="number"
                  min="1"
                  max="360"
                  value={repeatCount}
                  onChange={(e) => setRepeatCount(Number(e.target.value))}
                  disabled={isRunning}
                />
              </label>
            </div>
          </section>

          <section className="card">
            <h2>🎮 Control</h2>
            <div className="button-group">
              <button
                onClick={handleStart}
                disabled={!isConnected || isRunning}
                className="btn btn-success"
              >
                ▶ Start
              </button>
              <button
                onClick={handlePause}
                disabled={!isRunning}
                className="btn btn-warning"
              >
                ⏸ Pause
              </button>
              <button
                onClick={handleResume}
                disabled={isRunning || currentMeasurementIndex === 0}
                className="btn btn-info"
              >
                ▶ Resume
              </button>
            </div>
            <div className="button-group">
              <button onClick={handleReset} disabled={!isConnected} className="btn btn-danger">
                ↺ Reset to 0°
              </button>
            </div>
          </section>

          <section className="card">
            <h2>📊 Progress</h2>
            <div className="current-angle">
              <span className="angle-value">{currentAngle}°</span>
            </div>
            <div className="progress-bar">
              <div className="progress-fill" style={{ width: `${progress}%` }}></div>
            </div>
            <p className="progress-text">{progress}% Complete</p>
          </section>
        </div>

        {/* RIGHT SECTION: Data Display */}
        <div className="data-panel">
          <section className="card">
            <h2>📈 Measurements</h2>
            <div className="data-actions">
              <button onClick={handleExportCSV} className="btn btn-small btn-primary">
                📥 Export CSV
              </button>
              <button onClick={handleClearData} className="btn btn-small btn-danger">
                🗑 Clear
              </button>
            </div>

            {measurements.length === 0 ? (
              <p className="no-data">No measurements yet. Start a cycle to collect data.</p>
            ) : (
              <div className="data-table">
                <table>
                  <thead>
                    <tr>
                      <th>Angle (°)</th>
                      <th>Current (mA)</th>
                      <th>Time</th>
                    </tr>
                  </thead>
                  <tbody>
                    {measurements.map((m) => (
                      <tr key={m.id}>
                        <td>{m.angle}</td>
                        <td>{m.current_reading.toFixed(4)}</td>
                        <td>{new Date(m.timestamp).toLocaleTimeString()}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </section>

          <section className="card">
            <h2>📉 System Info</h2>
            <p style={{fontSize: '0.9rem', lineHeight: '1.6'}}>
              <strong>Main ESP32:</strong> 10.135.98.50<br/>
              <strong>ESP32-CAM:</strong> 10.135.98.51<br/>
              <strong>Render API:</strong> antenna-ocr-api.onrender.com<br/>
              <strong>Database:</strong> Firebase Realtime DB<br/>
              <br/>
              ✨ <strong>All IPs are STATIC!</strong><br/>
              Each time you power on, the ESP32s will have the same IPs.<br/>
              No need to check Serial Monitor.
              <br/>
              <br/>
              💡 <strong>Debug tip:</strong> Open browser DevTools (F12) → Console tab<br/>
              You'll see detailed logs of every operation!
            </p>
          </section>
        </div>
      </div>
    </div>
  );
};

export default AntennaControl;
import React, { useState, useEffect } from 'react';
import { db } from './firebase-config';
import { ref, onValue, set } from 'firebase/database';
import './AntennaControl.css';

const AntennaControl = () => {
  // ═══════════════════════════════════════════════════════════════════
  // STATIC IP ADDRESSES (Always the same - no need to check Serial!)
  // ═══════════════════════════════════════════════════════════════════
  const MAIN_ESP32_IP = '192.168.1.50';        // Main ESP32 (Motor)
  const ESP32_CAM_IP = '192.168.1.51';         // ESP32-CAM (Camera)
  
  // State Management
  const [isConnected, setIsConnected] = useState(false);
  const [isRunning, setIsRunning] = useState(false);
  const [currentAngle, setCurrentAngle] = useState(0);
  const [degreesPerMove, setDegreesPerMove] = useState(10);
  const [repeatCount, setRepeatCount] = useState(36);
  const [measurements, setMeasurements] = useState([]);
  const [status, setStatus] = useState('Ready to connect');
  const [progress, setProgress] = useState(0);

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

  // Connect to ESP32 (uses static IP)
  const handleConnect = async () => {
    try {
      setStatus('Connecting to 192.168.1.50...');
      
      // Test connection to Main ESP32
      const response = await fetch(`http://${MAIN_ESP32_IP}/status`, { 
        timeout: 5000 
      });
      
      if (response.ok) {
        setIsConnected(true);
        setStatus('✓ Connected! Static IPs: Main=192.168.1.50, Camera=192.168.1.51');
      } else {
        throw new Error('No response from ESP32');
      }
    } catch (error) {
      setStatus('✗ Connection Failed - Check WiFi and ESP32 power');
      alert(`Error: ${error.message}\n\nMake sure:\n1. ESP32s are powered on\n2. Same WiFi network\n3. IPs: Main=192.168.1.50, Camera=192.168.1.51`);
    }
  };

  // Send command to Main ESP32
  const sendCommand = async (command, angle = null) => {
    if (!isConnected) {
      alert('Not connected to ESP32. Click "Connect" button first.');
      return;
    }

    try {
      const url = angle !== null 
        ? `http://${MAIN_ESP32_IP}/rotate?angle=${angle}`
        : `http://${MAIN_ESP32_IP}/control?cmd=${command}`;

      const response = await fetch(url);
      if (!response.ok) throw new Error('Command failed');
      return true;
    } catch (error) {
      alert(`Error sending command: ${error.message}`);
      return false;
    }
  };

  // Start measurement cycle
  const handleStart = async () => {
    if (!isConnected) {
      alert('Not connected to ESP32');
      return;
    }

    setIsRunning(true);
    setStatus('Starting measurement cycle...');
    
    try {
      await runMeasurementCycle();
    } catch (error) {
      setStatus(`Error: ${error.message}`);
      alert(error.message);
    } finally {
      setIsRunning(false);
    }
  };

  // Run full measurement cycle
  const runMeasurementCycle = async () => {
    for (let i = 0; i < repeatCount; i++) {
      if (!isRunning) {
        setStatus('Paused');
        break;
      }

      const angle = (i * degreesPerMove) % 360;
      setCurrentAngle(angle);
      setStatus(`📍 Measuring at ${angle}°...`);
      setProgress(Math.round((i / repeatCount) * 100));

      // Step 1: Move to angle
      const moved = await sendCommand('rotate', angle);
      if (!moved) throw new Error(`Failed to move to ${angle}°`);

      // Step 2: Wait for field stabilization
      setStatus(`⏳ Waiting 30s at ${angle}° for field stabilization...`);
      await new Promise((resolve) => setTimeout(resolve, 30000));

      // Step 3: Capture and extract
      setStatus(`📷 Capturing image at ${angle}°...`);
      const value = await captureAndExtract();

      if (value === null) {
        throw new Error(`Failed to extract data at ${angle}°`);
      }

      // Step 4: Save to Firebase
      await saveMeasurement(angle, value);
      setStatus(`✅ Saved: ${angle}° = ${value.toFixed(4)} mA`);
    }

    setStatus('✨ Measurement cycle complete!');
    setProgress(100);
    setIsRunning(false);
  };

  // Capture image and extract via API
  const captureAndExtract = async () => {
    try {
      // Step 1: Tell ESP32-CAM to capture image
      const captureResponse = await fetch(`http://${ESP32_CAM_IP}/capture`);
      if (!captureResponse.ok) throw new Error('Failed to capture image');

      // Step 2: Tell ESP32-CAM to extract (sends to Render API)
      const extractResponse = await fetch(`http://${ESP32_CAM_IP}/extract`);
      if (!extractResponse.ok) throw new Error('OCR extraction failed');
      
      const data = await extractResponse.json();
      if (!data.success) throw new Error('Extraction returned error');
      
      return parseFloat(data.extractedValue);
    } catch (error) {
      console.error('Capture/Extract error:', error);
      return null;
    }
  };

  // Save measurement to Firebase
  const saveMeasurement = async (angle, value) => {
    try {
      const measurementRef = ref(db, `measurements/${Date.now()}`);
      await set(measurementRef, {
        angle,
        current_reading: value,
        timestamp: new Date().toISOString(),
      });
    } catch (error) {
      console.error('Firebase save error:', error);
    }
  };

  // Pause measurement
  const handlePause = () => {
    setIsRunning(false);
    setStatus('⏸ Paused');
  };

  // Resume measurement
  const handleResume = async () => {
    setIsRunning(true);
    setStatus('▶ Resuming...');
    await runMeasurementCycle();
  };

  // Reset to 0
  const handleReset = async () => {
    const success = await sendCommand('reset');
    if (success) {
      setCurrentAngle(0);
      setStatus('↺ Reset to 0° ✓');
    }
  };

  // Clear all data
  const handleClearData = async () => {
    if (window.confirm('Clear all measurements?')) {
      setMeasurements([]);
      setStatus('Data cleared');
    }
  };

  // Export data as CSV
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
  };

  return (
    <div className="antenna-container">
      <header className="antenna-header">
        <h1>🛰️ RF Antenna Automation System</h1>
        <p>Automated Antenna Radiation Pattern Measurement</p>
        <p style={{fontSize: '0.9rem', opacity: 0.8}}>
          ⭐ Static IPs: Main=192.168.1.50 | Camera=192.168.1.51
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
                placeholder="192.168.1.50 (Auto-filled)"
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
              Main ESP32: <strong>192.168.1.50</strong><br/>
              ESP32-CAM: <strong>192.168.1.51</strong>
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
                disabled={!isRunning || isRunning}
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
              <strong>Main ESP32:</strong> 192.168.1.50<br/>
              <strong>ESP32-CAM:</strong> 192.168.1.51<br/>
              <strong>Render API:</strong> antenna-ocr-api.onrender.com<br/>
              <strong>Database:</strong> Firebase Realtime DB<br/>
              <br/>
              ✨ <strong>All IPs are STATIC!</strong><br/>
              Each time you power on, the ESP32s will have the same IPs.<br/>
              No need to check Serial Monitor.
            </p>
          </section>
        </div>
      </div>
    </div>
  );
};

export default AntennaControl;
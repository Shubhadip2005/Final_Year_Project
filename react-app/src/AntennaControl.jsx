import React, { useState, useEffect } from 'react';
import { db } from './firebase-config';
import { ref, onValue, set } from 'firebase/database';
import './AntennaControl.css';

const AntennaControl = () => {
  // State Management
  const [esp32IP, setEsp32IP] = useState('');
  const [isConnected, setIsConnected] = useState(false);
  const [isRunning, setIsRunning] = useState(false);
  const [currentAngle, setCurrentAngle] = useState(0);
  const [degreesPerMove, setDegreesPerMove] = useState(10);
  const [repeatCount, setRepeatCount] = useState(36);
  const [measurements, setMeasurements] = useState([]);
  const [status, setStatus] = useState('Ready');
  const [progress, setProgress] = useState(0);
  const [apiEndpoint, setApiEndpoint] = useState('https://antenna-ocr-api.onrender.com');

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

  // Connect to ESP32
  const handleConnect = async () => {
    if (!esp32IP) {
      alert('Please enter ESP32 IP address');
      return;
    }

    try {
      setStatus('Connecting...');
      const response = await fetch(`http://${esp32IP}/status`, { timeout: 5000 });
      if (response.ok) {
        setIsConnected(true);
        setStatus('Connected ✓');
      } else {
        throw new Error('No response from ESP32');
      }
    } catch (error) {
      setStatus('Connection Failed ✗');
      alert(`Error: ${error.message}\nMake sure ESP32 is on and IP is correct`);
    }
  };

  // Send command to ESP32
  const sendCommand = async (command, angle = null) => {
    if (!isConnected) {
      alert('Not connected to ESP32');
      return;
    }

    try {
      const url = angle !== null 
        ? `http://${esp32IP}/control?cmd=${command}&angle=${angle}`
        : `http://${esp32IP}/control?cmd=${command}`;

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
      setStatus(`Measuring at ${angle}°...`);
      setProgress(Math.round((i / repeatCount) * 100));

      // Move to angle
      const moved = await sendCommand('rotate', angle);
      if (!moved) throw new Error(`Failed to move to ${angle}°`);

      // Wait for field stabilization
      setStatus(`Waiting 30s at ${angle}° for field stabilization...`);
      await new Promise((resolve) => setTimeout(resolve, 30000));

      // Capture and extract
      setStatus(`Capturing and extracting data at ${angle}°...`);
      const value = await captureAndExtract();

      if (value === null) {
        throw new Error(`Failed to extract data at ${angle}°`);
      }

      // Save to Firebase
      await saveMeasurement(angle, value);
      setStatus(`✓ Saved: ${angle}° = ${value} mA`);
    }

    setStatus('Measurement cycle complete!');
    setProgress(100);
    setIsRunning(false);
  };

  // Capture image and extract via API
  const captureAndExtract = async () => {
    try {
      // Step 1: Tell ESP32 to capture image
      const captureResponse = await fetch(`http://${esp32IP}/capture`);
      if (!captureResponse.ok) throw new Error('Failed to capture image');

      const { imagePath } = await captureResponse.json();
      
      // Step 2: Send image to Render API for OCR
      const ocrResponse = await fetch(`${apiEndpoint}/extract-ocr`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ imagePath }),
      });

      if (!ocrResponse.ok) throw new Error('OCR extraction failed');
      
      const { extractedValue } = await ocrResponse.json();
      return parseFloat(extractedValue);
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
    setStatus('Paused');
    sendCommand('pause');
  };

  // Resume measurement
  const handleResume = async () => {
    setIsRunning(true);
    setStatus('Resuming...');
    await runMeasurementCycle();
  };

  // Reset to 0
  const handleReset = async () => {
    const success = await sendCommand('reset');
    if (success) {
      setCurrentAngle(0);
      setStatus('Reset to 0° ✓');
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
      </header>

      <div className="main-content">
        {/* LEFT SECTION: Connection & Controls */}
        <div className="control-panel">
          <section className="card">
            <h2>📡 ESP32 Connection</h2>
            <div className="input-group">
              <input
                type="text"
                placeholder="e.g., 192.168.1.100"
                value={esp32IP}
                onChange={(e) => setEsp32IP(e.target.value)}
                disabled={isConnected}
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
            <div className="setting-group">
              <label>
                API Endpoint:
                <input
                  type="text"
                  value={apiEndpoint}
                  onChange={(e) => setApiEndpoint(e.target.value)}
                  disabled={isRunning}
                  placeholder="https://antenna-ocr-api.onrender.com"
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
            <h2>📉 Real-time Graph</h2>
            {measurements.length > 0 ? (
              <canvas id="dataChart"></canvas>
            ) : (
              <p className="no-data">Graph will appear here once data is collected</p>
            )}
          </section>
        </div>
      </div>
    </div>
  );
};

export default AntennaControl;
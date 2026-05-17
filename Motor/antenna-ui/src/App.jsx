import { useState, useEffect, useRef, useCallback } from 'react'

/* ─── IP Validator ───────────────────────────────────── */
function validateIP(raw) {
  const url = raw.trim().replace(/\/$/, '')

  // Must start with http:// or https://
  if (!url.startsWith('http://') && !url.startsWith('https://')) {
    return { valid: false, reason: 'Must start with http:// (e.g. http://192.168.1.45)' }
  }

  let hostname
  try {
    hostname = new URL(url).hostname
  } catch {
    return { valid: false, reason: 'Invalid URL format.' }
  }

  // Must be an IP address, not a domain or localhost pointing to self
  const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/
  if (!ipRegex.test(hostname)) {
    return { valid: false, reason: `"${hostname}" is not a valid IP address.` }
  }

  // Each octet must be 0–255
  const octets = hostname.split('.').map(Number)
  if (octets.some(o => o > 255)) {
    return { valid: false, reason: 'IP address has an octet above 255.' }
  }

  // Warn if it looks like loopback (127.x or the page's own host)
  if (octets[0] === 127) {
    return { valid: false, reason: '127.x.x.x is loopback — enter your ESP32\'s local network IP.' }
  }

  return { valid: true, url }
}

/* ─── tiny style helpers ─────────────────────────────── */
const css = (obj) => obj

const S = {
  page: css({
    minHeight: '100vh',
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'center',
    padding: '24px 16px 48px',
    backgroundImage: `
      radial-gradient(ellipse 80% 40% at 50% 0%, rgba(0,255,65,0.06) 0%, transparent 70%),
      repeating-linear-gradient(0deg, transparent, transparent 3px, rgba(0,0,0,0.06) 3px, rgba(0,0,0,0.06) 4px)
    `,
    gap: '16px',
  }),
  wrap: css({ width: '100%', maxWidth: '680px', display: 'flex', flexDirection: 'column', gap: '14px' }),

  /* header */
  header: css({ textAlign: 'center', padding: '8px 0 4px' }),
  h1: css({
    fontFamily: "'Bebas Neue', monospace",
    fontSize: 'clamp(32px, 8vw, 52px)',
    letterSpacing: '8px',
    color: 'var(--green)',
    textShadow: '0 0 30px rgba(0,255,65,0.5), 0 0 60px rgba(0,255,65,0.2)',
    lineHeight: 1,
  }),
  subtitle: css({ fontSize: '10px', letterSpacing: '4px', color: 'var(--green-dim)', marginTop: '6px' }),

  /* panel */
  panel: css({
    background: 'var(--surface)',
    border: '1px solid var(--border)',
    borderRadius: '6px',
    padding: '20px',
    position: 'relative',
  }),
  panelLabel: css({
    position: 'absolute',
    top: '-9px',
    left: '14px',
    background: 'var(--surface)',
    padding: '0 8px',
    fontSize: '9px',
    letterSpacing: '3px',
    color: 'var(--green-dim)',
    textTransform: 'uppercase',
  }),

  /* inputs */
  input: css({
    background: '#000',
    color: 'var(--green)',
    border: '1px solid var(--border-lit)',
    borderRadius: '4px',
    padding: '11px 14px',
    fontFamily: "'DM Mono', monospace",
    fontSize: '14px',
    outline: 'none',
    width: '100%',
    transition: 'border-color 0.2s, box-shadow 0.2s',
  }),

  /* terminal */
  terminal: css({
    height: '220px',
    overflowY: 'auto',
    background: '#000',
    border: '1px solid var(--border)',
    borderRadius: '4px',
    padding: '12px 14px',
    fontSize: '12px',
    lineHeight: '1.8',
  }),
}

/* ─── Reusable Panel ──────────────────────────────────── */
function Panel({ label, children, style }) {
  return (
    <div style={{ ...S.panel, ...style }}>
      {label && <span style={S.panelLabel}>{label}</span>}
      {children}
    </div>
  )
}

/* ─── Button ──────────────────────────────────────────── */
const BTN_VARIANTS = {
  green:  { background: 'var(--green)',  color: '#000', boxShadow: '0 0 18px rgba(0,255,65,0.25)' },
  amber:  { background: 'var(--amber)',  color: '#000' },
  blue:   { background: 'var(--blue)',   color: '#000' },
  red:    { background: 'var(--red)',    color: '#fff' },
  purple: { background: 'var(--purple)', color: '#000' },
  ghost:  { background: 'transparent',  color: 'var(--green)', border: '1px solid var(--border-lit)' },
}

function Btn({ variant = 'green', children, onClick, disabled, style }) {
  const [hover, setHover] = useState(false)
  return (
    <button
      onClick={onClick}
      disabled={disabled}
      onMouseEnter={() => setHover(true)}
      onMouseLeave={() => setHover(false)}
      style={{
        fontFamily: "'Bebas Neue', monospace",
        fontSize: '15px',
        letterSpacing: '2px',
        border: 'none',
        borderRadius: '4px',
        padding: '13px 20px',
        cursor: disabled ? 'not-allowed' : 'pointer',
        opacity: disabled ? 0.35 : hover ? 0.85 : 1,
        transform: hover && !disabled ? 'translateY(-1px)' : 'none',
        transition: 'opacity 0.15s, transform 0.15s',
        width: '100%',
        ...BTN_VARIANTS[variant],
        ...style,
      }}
    >
      {children}
    </button>
  )
}

/* ─── Gauge / Dial ────────────────────────────────────── */
function AngleDial({ angle }) {
  const num = parseInt(angle) || 0
  // clamp for visual: wrap 0-360
  const deg = ((num % 360) + 360) % 360
  const rad = (deg - 90) * (Math.PI / 180)
  const cx = 80, cy = 80, r = 60
  const nx = cx + r * Math.cos(rad)
  const ny = cy + r * Math.sin(rad)

  // arc from 0 to current
  const arcRad0 = -90 * (Math.PI / 180)
  const arcRad1 = rad
  const largeArc = deg > 180 ? 1 : 0
  const ax0 = cx + r * Math.cos(arcRad0)
  const ay0 = cy + r * Math.sin(arcRad0)

  return (
    <svg viewBox="0 0 160 160" style={{ width: '160px', height: '160px' }}>
      {/* tick marks */}
      {Array.from({ length: 36 }).map((_, i) => {
        const a = (i * 10 - 90) * (Math.PI / 180)
        const r1 = i % 9 === 0 ? 52 : 56
        return (
          <line
            key={i}
            x1={cx + r1 * Math.cos(a)} y1={cy + r1 * Math.sin(a)}
            x2={cx + 60 * Math.cos(a)} y2={cy + 60 * Math.sin(a)}
            stroke={i % 9 === 0 ? 'var(--green-dim)' : 'var(--border-lit)'}
            strokeWidth={i % 9 === 0 ? 1.5 : 0.8}
          />
        )
      })}
      {/* base ring */}
      <circle cx={cx} cy={cy} r={r} fill="none" stroke="var(--border-lit)" strokeWidth="1" />
      {/* arc fill */}
      {deg !== 0 && (
        <path
          d={`M ${ax0} ${ay0} A ${r} ${r} 0 ${largeArc} 1 ${nx} ${ny}`}
          fill="none"
          stroke="var(--green)"
          strokeWidth="2.5"
          strokeLinecap="round"
          style={{ filter: 'drop-shadow(0 0 4px var(--green))' }}
        />
      )}
      {/* needle */}
      <line
        x1={cx} y1={cy}
        x2={nx} y2={ny}
        stroke="var(--green)" strokeWidth="2" strokeLinecap="round"
        style={{ filter: 'drop-shadow(0 0 6px var(--green))' }}
      />
      <circle cx={cx} cy={cy} r={4} fill="var(--green)" style={{ filter: 'drop-shadow(0 0 6px var(--green))' }} />
      {/* label */}
      <text x={cx} y={cy + 26} textAnchor="middle" fill="var(--green)" fontSize="20" fontFamily="'Bebas Neue'" letterSpacing="1">
        {angle === '---' ? '---' : `${angle}°`}
      </text>
    </svg>
  )
}

/* ─── Terminal Log ────────────────────────────────────── */
const LOG_COLORS = {
  info:  'var(--green)',
  warn:  'var(--amber)',
  error: 'var(--red)',
  ok:    'var(--blue)',
  sys:   'var(--green-dim)',
}

function TerminalLog({ logs }) {
  const ref = useRef(null)
  useEffect(() => {
    if (ref.current) ref.current.scrollTop = ref.current.scrollHeight
  }, [logs])

  return (
    <div ref={ref} style={S.terminal}>
      {logs.map((l, i) => (
        <div key={i}>
          <span style={{ color: 'var(--text-mute)' }}>[{l.time}] </span>
          <span style={{ color: LOG_COLORS[l.type] || 'var(--green)' }}>{l.msg}</span>
        </div>
      ))}
    </div>
  )
}

/* ─── Connection Status Badge ─────────────────────────── */
function StatusBadge({ status }) {
  const map = {
    connected:    { color: 'var(--green)',  label: 'ONLINE',      dot: true },
    disconnected: { color: 'var(--red)',    label: 'OFFLINE',     dot: false },
    connecting:   { color: 'var(--amber)',  label: 'CONNECTING…', dot: false },
    idle:         { color: 'var(--green-dim)', label: 'NOT CONNECTED', dot: false },
  }
  const { color, label, dot } = map[status] || map.idle
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
      <div style={{
        width: '8px', height: '8px', borderRadius: '50%',
        background: color,
        boxShadow: dot ? `0 0 8px ${color}` : 'none',
        animation: dot ? 'pulse 2s ease-in-out infinite' : 'none',
      }} />
      <span style={{ fontSize: '10px', letterSpacing: '2px', color }}>{label}</span>
      <style>{`@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.4}}`}</style>
    </div>
  )
}

/* ─── Field ───────────────────────────────────────────── */
function Field({ label, children }) {
  return (
    <div>
      <div style={{ fontSize: '9px', letterSpacing: '3px', color: 'var(--green-dim)', marginBottom: '8px' }}>{label}</div>
      {children}
    </div>
  )
}

/* ═══════════════════════════════════════════════════════
   MAIN APP
═══════════════════════════════════════════════════════ */
export default function App() {
  const [ip, setIp] = useState(() => localStorage.getItem('esp32ip') || '')
  const [status, setStatus] = useState('idle')
  const [angle, setAngle] = useState('---')
  const [deg, setDeg] = useState(10)
  const [num, setNum] = useState(1)
  const [isPaused, setIsPaused] = useState(false)
  const [logs, setLogs] = useState([
    { time: 'BOOT', msg: '> System online. Enter ESP32 IP and press CONNECT.', type: 'sys' }
  ])

  const pollRef = useRef(null)

  /* ── log helper ───────────────────────────────────── */
  const log = useCallback((msg, type = 'info') => {
    const time = new Date().toLocaleTimeString()
    setLogs(prev => [...prev.slice(-120), { time, msg, type }])
  }, [])

  /* ── fetch wrapper ────────────────────────────────── */
  const call = useCallback(async (path, label) => {
    const base = ip.trim().replace(/\/$/, '')
    if (!base) { log('⚠ No IP set.', 'warn'); return null }
    try {
      const res = await fetch(base + path)
      const text = await res.text()
      if (label) log(`▶ ${label}: ${text}`, 'ok')
      return text
    } catch {
      log(`✖ ${label || path} failed — check IP / network.`, 'error')
      setStatus('disconnected')
      return null
    }
  }, [ip, log])

  /* ── polling ─────────────────────────────────────── */
  const startPolling = useCallback(() => {
    if (pollRef.current) clearInterval(pollRef.current)
    pollRef.current = setInterval(async () => {
      const base = ip.trim().replace(/\/$/, '')
      if (!base) return
      try {
        const res = await fetch(base + '/getAngle')
        const val = await res.text()
        setAngle(prev => { if (prev !== val) log(`⟳ Angle: ${val}°`, 'sys'); return val })
        setStatus('connected')
      } catch {
        setStatus('disconnected')
      }
    }, 600)
  }, [ip, log])

  useEffect(() => () => { if (pollRef.current) clearInterval(pollRef.current) }, [])

  /* ── connect ─────────────────────────────────────── */
  async function connect() {
    if (!ip.trim()) { log('⚠ Enter an IP address first.', 'warn'); return }

    const check = validateIP(ip)
    if (!check.valid) {
      log(`✖ Invalid IP — ${check.reason}`, 'error')
      setStatus('disconnected')
      return
    }

    const base = check.url
    localStorage.setItem('esp32ip', base)
    setStatus('connecting')
    log(`Connecting to ${base} …`, 'sys')
    try {
      const res = await fetch(base + '/getAngle')
      const val = await res.text()

      // If we got HTML back, the IP pointed to a webpage not an ESP32
      if (val.trim().startsWith('<')) {
        setStatus('disconnected')
        log('✖ Got a web page back, not an ESP32. Double-check the IP address.', 'error')
        return
      }

      setAngle(val)
      setStatus('connected')
      log(`✔ Connected! Angle: ${val}°`, 'ok')
      startPolling()
    } catch {
      setStatus('disconnected')
      log('✖ Connection failed — ESP32 unreachable. Check IP and WiFi.', 'error')
    }
  }

  /* ── motor commands ──────────────────────────────── */
  async function startRotation() {
    log(`Starting: ${deg}° × ${num} repeat(s)`, 'info')
    setIsPaused(false)
    await call(`/run?deg=${deg}&num=${num}`, 'Rotation')
  }

  async function goHome() {
    log('Returning to 0° home…', 'warn')
    await call('/goHome', 'Home')
  }

  async function togglePause() {
    if (!isPaused) {
      log('⚠ Pause requested.', 'warn')
      await call('/pause', 'Pause')
      setIsPaused(true)
    } else {
      log('▶ Resuming motor…', 'ok')
      await call('/resume', 'Resume')
      setIsPaused(false)
    }
  }

  /* ─── RENDER ───────────────────────────────────────── */
  return (
    <div style={S.page}>
      <div style={S.wrap}>

        {/* ── Header ── */}
        <div style={S.header}>
          <div style={S.h1}>ANTENNA COMMAND CENTER</div>
          <div style={S.subtitle}>ESP32 · A4988 · NEMA 17 STEPPER CONTROL</div>
        </div>

        {/* ── Connection ── */}
        <Panel label="ESP32 CONNECTION">
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <StatusBadge status={status} />
            <div style={{ display: 'flex', gap: '10px' }}>
              <input
                style={{ ...S.input, flex: 1 }}
                placeholder="http://192.168.x.x"
                value={ip}
                onChange={e => setIp(e.target.value)}
                onKeyDown={e => e.key === 'Enter' && connect()}
              />
              <Btn
                variant={status === 'connected' ? 'ghost' : 'green'}
                onClick={connect}
                disabled={status === 'connecting'}
                style={{ width: 'auto', padding: '11px 20px', flexShrink: 0, fontSize: '13px' }}
              >
                {status === 'connecting' ? 'WAIT…' : status === 'connected' ? 'RECONNECT' : 'CONNECT'}
              </Btn>
            </div>
          </div>
        </Panel>

        {/* ── Angle Display ── */}
        <Panel label="CURRENT POSITION">
          <div style={{ display: 'flex', alignItems: 'center', gap: '24px', justifyContent: 'center', flexWrap: 'wrap', padding: '8px 0' }}>
            <AngleDial angle={angle} />
            <div style={{ textAlign: 'left' }}>
              <div style={{ fontSize: '9px', letterSpacing: '3px', color: 'var(--green-dim)', marginBottom: '4px' }}>LIVE READOUT</div>
              <div style={{
                fontFamily: "'Bebas Neue', monospace",
                fontSize: 'clamp(48px, 12vw, 72px)',
                color: 'var(--green)',
                lineHeight: 1,
                textShadow: '0 0 30px rgba(0,255,65,0.4)',
              }}>
                {angle}<span style={{ fontSize: '0.4em', color: 'var(--green-dim)' }}>°</span>
              </div>
              <div style={{ fontSize: '9px', letterSpacing: '3px', color: 'var(--green-dim)' }}>DEGREES FROM HOME</div>
            </div>
          </div>
        </Panel>

        {/* ── Rotation Controls ── */}
        <Panel label="ROTATION SEQUENCE">
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '14px', marginBottom: '16px' }}>
            <Field label="DEGREES PER MOVE">
              <input
                type="number"
                style={S.input}
                value={deg}
                min={1} max={360}
                onChange={e => setDeg(e.target.value)}
              />
            </Field>
            <Field label="REPEAT COUNT">
              <input
                type="number"
                style={S.input}
                value={num}
                min={1}
                onChange={e => setNum(e.target.value)}
              />
            </Field>
          </div>
          <Btn variant="green" onClick={startRotation} disabled={status !== 'connected'}>
            ▶ START ROTATION SEQUENCE
          </Btn>
        </Panel>

        {/* ── Motor Controls ── */}
        <Panel label="MOTOR CONTROLS">
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
            <Btn variant="blue" onClick={goHome} disabled={status !== 'connected'}>
              ⌂ RETURN TO 0°
            </Btn>
            <Btn
              variant={isPaused ? 'purple' : 'amber'}
              onClick={togglePause}
              disabled={status !== 'connected'}
            >
              {isPaused ? '▶ RESUME MOTOR' : '❚❚ PAUSE MOTOR'}
            </Btn>
          </div>
        </Panel>

        {/* ── Terminal ── */}
        <Panel label="SYSTEM LOG" style={{ padding: '20px 20px 16px' }}>
          <TerminalLog logs={logs} />
        </Panel>

      </div>
    </div>
  )
}
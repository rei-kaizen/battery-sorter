# E-Waste Battery Sorter — Project Context & Technical Specification

## Overview

An embedded systems final project that automatically classifies and sorts mixed batteries (Li-ion, Alkaline, NiMH) into separate bins. An IR sensor detects battery presence on a conveyor belt; an inductive proximity sensor and a voltage divider circuit identify the chemistry type. Servo gates divert batteries into the correct bin. Every sort event is published to HiveMQ Cloud via MQTT and appears in real time on a Vue.js + Firebase web dashboard.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  NodeMCU ESP8266                         │
│                                                          │
│  IR Sensor ──────> detects battery on belt              │
│  Inductive ──────> detects metallic casing              │
│  Voltage Div ────> reads terminal voltage (production)   │
│           or ────> simulatedVoltage() 0–1.5V (demo)     │
│                                                          │
│  Classify battery type                                   │
│  Trigger Servo A or Servo B                              │
│  Publish JSON event via MQTT TLS (port 8883)             │
└────────────────────────┬────────────────────────────────┘
                         │ MQTT (ewaste/sort/events)
                         ▼
                 ┌──────────────┐
                 │ HiveMQ Cloud  │
                 └──────┬───────┘
                        │ WebSocket (wss://:8884)
                        ▼
            ┌───────────────────────┐
            │   Vue 3 Dashboard     │
            │                       │
            │  Firebase Auth ───── Login / RBAC     │
            │  MQTT subscriber ─── Live events      │
            │  Firebase RTDB ───── Persist events   │
            │  Chart.js ────────── Voltage chart    │
            │                       Bar chart       │
            │                       Event table     │
            └───────────────────────┘
```

---

## Technology Stack

| Layer | Technology | Version |
|-------|-----------|---------|
| Microcontroller | NodeMCU ESP8266 (ESP-12E) | — |
| Firmware language | Arduino C++ | ESP8266 Core ≥ 3.1 |
| MQTT broker | HiveMQ Cloud | TLS, port 8883/8884 |
| Frontend framework | Vue 3 + Vite | ^3.5 / ^6.0 |
| Charting | Chart.js + vue-chartjs | ^4.5 / ^5.3 |
| Authentication | Firebase Auth (email/password) | ^12.x |
| Database | Firebase Realtime Database | ^12.x |
| Hosting | Firebase Hosting | — |
| CI/CD | GitHub Actions | — |

---

## MQTT Payload Schema

```json
{
  "type":      "Li-ion" | "Alkaline" | "NiMH" | "Unknown",
  "voltage":   1.23,
  "magnetic":  false,
  "timestamp": 1748700000000
}
```

- `voltage` — real battery terminal voltage in production; random 0.00–1.50V in demo mode
- `timestamp` — Unix epoch in milliseconds from NTP sync; falls back to `millis()` if NTP unavailable
- Published to topic: `ewaste/sort/events`
- Belt control received from topic: `ewaste/control/belt` (`"ON"` / `"OFF"`)

---

## Classification Logic

| Battery | Inductive Sensor | Voltage (production) | Servo |
|---------|-----------------|---------------------|-------|
| Li-ion | No metal | 3.0–4.2V | None — passes to end of belt |
| Alkaline | No metal | 0.8–1.6V | Servo A → Bin A |
| NiMH | Metal detected | 1.0–1.5V | Servo B → Bin B |

### Demo Mode vs Production Mode

| | Demo (current) | Production |
|---|---|---|
| Voltage source | `simulatedVoltage()` — random 0.00–1.50V | ADC on A0 via 100kΩ/10kΩ divider |
| Classification | Cyclic: Alkaline → NiMH → Li-ion | IR + inductive + voltage thresholds |
| Toggle | Replace `simulatedVoltage()` call in `control.ino` | Wire voltage divider to A0 |

### Voltage Divider (Production)

```
Battery (+) ─── 100kΩ ─┬─── A0 (NodeMCU)
                        └─── 10kΩ ─── GND / Battery (−)

V_battery = (analogRead(A0) / 1023.0) × 3.3 × 11.0
Max safe input: 36.3V
```

---

## RBAC (Role-Based Access Control)

| Role | Capabilities |
|------|-------------|
| Viewer | View live charts, voltage chart, event table |
| Admin | All viewer access + Start/Stop conveyor belt + User Management tab |

- Default role on sign-up: **Viewer**
- First admin: set manually in Firebase Realtime Database (`/users/{uid}/role = "admin"`)
- Subsequent promotions: Admin user via User Management tab in dashboard
- Firebase RTDB rules enforce write restrictions at the database level

---

## Firebase Realtime Database Schema

```
root/
├── battery_events/
│   └── {auto-push-id}/
│       ├── type:      string
│       ├── voltage:   number
│       ├── magnetic:  boolean
│       └── timestamp: number (Unix ms)
└── users/
    └── {uid}/
        ├── email:     string
        ├── role:      "viewer" | "admin"
        └── createdAt: number (Unix ms)
```

---

## Project Timeline & Milestones

| Week | Milestone | Status |
|------|-----------|--------|
| 5 | HiveMQ broker setup; ESP8266 MQTT publish; Firebase RTDB schema | ✅ Done |
| 6 | Vue.js dashboard — live table, charts, Firebase + MQTT integration | ✅ Done |
| 7 | Firebase Auth + RBAC; MQTT belt control; Firebase Hosting + CI/CD | ✅ Done |
| 8 | Stress test 50+ batteries; accuracy report; demo video; submission | 🔄 In progress |

---

## Success Criteria

| Criterion | Target | Notes |
|-----------|--------|-------|
| Classification accuracy | ≥ 90% (45/50 batteries) | Across all 3 chemistry types |
| Sort cycle time | ≤ 5 seconds | IR detect → servo complete |
| MQTT latency | ≤ 2 seconds | Sort event → dashboard visible |
| Belt remote control | Reliable within 3 seconds | 10 consecutive tests, no missed commands |
| Zero false Li-ion positives | 0 Alkaline/NiMH routed to Li-ion bin | Highest-priority safety criterion |

---

## Known Limitations & Design Decisions

| Decision | Reason |
|----------|--------|
| Simulated voltage in demo mode | Voltage divider not physically wired during testing phase; ensures dashboard shows realistic data |
| `setInsecure()` on TLS connection | Skips certificate validation — acceptable for academic scope, not for production |
| Credentials hardcoded in source | Academic project; in production use Vite `import.meta.env` and Arduino secure storage |
| ESP8266 instead of ESP32 | ESP32 DevKit lacked auto-reset circuit; upload failed repeatedly; NodeMCU has built-in auto-reset |
| RGB LED removed | Insufficient GPIO pins on ESP8266 after assigning all required peripherals |
| No ADC2 conflict concern | ESP8266 has only one ADC (A0); no WiFi/ADC conflict unlike ESP32 |

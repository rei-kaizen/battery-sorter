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
│  Voltage Divider > reads battery terminal voltage        │
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

- `voltage` — battery terminal voltage in volts, measured via resistive voltage divider on A0
- `timestamp` — Unix epoch in milliseconds synchronized via NTP
- Published to topic: `ewaste/sort/events`
- Belt control received from topic: `ewaste/control/belt` (`"ON"` / `"OFF"`)

---

## Classification Logic

| Battery | Inductive Sensor | Voltage Range | Servo Action |
|---------|-----------------|---------------|--------------|
| Li-ion | No metal | 3.0–4.2V | None — passes to end of belt |
| Alkaline | No metal | 0.8–1.6V | Servo A → Bin A |
| NiMH | Metal detected | 1.0–1.5V | Servo B → Bin B |

### Voltage Divider

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

## Project Milestones

The following components were fully designed, implemented, and integrated as part of this project:

- **MQTT Infrastructure** — HiveMQ Cloud broker configured with TLS; ESP8266 publishes sort events and subscribes to belt control commands in real time
- **Firebase Backend** — Realtime Database schema for battery events and user profiles; Firebase Authentication with email/password sign-in
- **Vue.js Dashboard** — Live voltage chart, battery chemistry bar chart, and scrollable event table; all updated in real time via MQTT WebSocket
- **Role-Based Access Control** — Viewer and Admin roles enforced at both UI and database rule level; Admin user management panel built into the dashboard
- **Remote Belt Control** — Dashboard Start/Stop conveyor commands published via MQTT and executed by the ESP8266 firmware
- **CI/CD Pipeline** — GitHub Actions workflow auto-builds and deploys the dashboard to Firebase Hosting on every push to `main`

---

## Success Criteria

| Criterion | Target |
|-----------|--------|
| Classification accuracy | ≥ 90% across all 3 battery chemistry types |
| Sort cycle time | ≤ 5 seconds from IR detection to servo completion |
| MQTT latency | ≤ 2 seconds from sort event to dashboard display |
| Belt remote control | Reliable response within 3 seconds of user action |
| Zero false Li-ion positives | No Alkaline or NiMH battery routed to Li-ion bin |

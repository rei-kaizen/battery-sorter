# E-Waste Battery Sorter — Embedded System & Dashboard

An embedded systems final project that automatically classifies and sorts mixed batteries (Li-ion, Alkaline, NiMH) into separate bins using an IR sensor and servo gates. Sorted events are published via MQTT and visualized in real time on a Vue.js web dashboard backed by Firebase.

---

## Repository Structure

```
battery-sorter/
├── control.ino              # ESP8266 firmware (Arduino)
├── ewaste-dashboard/        # Vue 3 + Vite web dashboard
│   ├── src/
│   │   ├── App.vue          # Root component, MQTT client, RBAC logic
│   │   ├── firebase.js      # Firebase singleton (db + auth)
│   │   ├── components/
│   │   │   ├── LoginOverlay.vue   # Auth UI + RTDB profile creation
│   │   │   ├── AdminPanel.vue     # User role management (admin only)
│   │   │   ├── LiveVoltageChart.vue
│   │   │   ├── BatteryBarChart.vue
│   │   │   └── EventTable.vue
│   │   └── style.css
│   ├── firebase.json        # Firebase Hosting config
│   ├── .firebaserc          # Firebase project binding
│   └── package.json
├── .github/
│   └── workflows/
│       └── firebase-deploy.yml   # CI/CD: auto-deploy on push to main
├── requirements.txt         # Full dependency manifest
└── proposal_context.md      # Project proposal and success criteria
```

---

## Technology Stack

| Layer | Technology |
|-------|-----------|
| Microcontroller | NodeMCU ESP8266 (ESP-12E) |
| Firmware | Arduino C++ (ESP8266 Arduino Core 3.x) |
| MQTT Broker | HiveMQ Cloud (TLS, port 8883) |
| Frontend | Vue 3 + Vite + Chart.js |
| Auth | Firebase Authentication (email/password) |
| Database | Firebase Realtime Database |
| Hosting | Firebase Hosting (auto-deploy via GitHub Actions) |

---

## Hardware Components

| Component | Purpose |
|-----------|---------|
| NodeMCU ESP8266 | Main controller, WiFi, MQTT |
| IR Obstacle Sensor (FC-51) | Detects battery presence on belt |
| Inductive Proximity Sensor (NPN) | Detects metallic casing (NiMH) |
| Voltage Divider (100kΩ / 10kΩ) | Reads battery terminal voltage via A0 |
| L298N Motor Driver | Controls conveyor belt DC motor |
| 2× SG90 Servo Motors | Divert gates for Alkaline and NiMH bins |
| 16×2 LCD with I2C Backpack | Local status display |
| 12V DC Adapter | Motor power supply |
| 5V USB Power Bank | Logic power supply (ESP8266 + sensors) |

### Voltage Divider (Battery Measurement)

The system uses a resistive voltage divider to safely read battery terminal voltage through the ESP8266's A0 ADC pin (0–3.3V range):

```
Battery (+) ─── 100kΩ ─── A0 (NodeMCU)
                       └── 10kΩ ─── Battery (−) / GND

Formula: V_battery = (ADC_raw / 1023.0) × 3.3 × 11.0
Max safe input: 3.3 × 11 = 36.3V
```

> **Demo mode:** The current firmware uses a simulated random voltage (0.00–1.50V) in place of the ADC reading for rapid testing. To enable real voltage sensing, replace `simulatedVoltage()` in `control.ino` with the ADC formula above and wire A0 to the voltage divider output.

---

## Pin Assignments (NodeMCU ESP8266)

| NodeMCU Pin | GPIO | Function |
|-------------|------|---------|
| D7 | GPIO 13 | IR Sensor OUT |
| D6 | GPIO 12 | Inductive Sensor OUT |
| A0 | ADC | Voltage Divider (production) |
| D5 | GPIO 14 | L298N ENA (PWM) |
| D0 | GPIO 16 | L298N IN1 |
| D3 | GPIO 0 | L298N IN2 |
| D8 | GPIO 15 | Servo A Signal (Alkaline gate) |
| D4 | GPIO 2 | Servo B Signal (NiMH gate) |
| D2 | GPIO 4 | LCD SDA (I2C) |
| D1 | GPIO 5 | LCD SCL (I2C) |

---

## MQTT Topics

| Topic | Direction | Payload |
|-------|-----------|---------|
| `ewaste/sort/events` | ESP8266 → Dashboard | `{"type":"Li-ion","voltage":1.23,"magnetic":false,"timestamp":1748700000000}` |
| `ewaste/control/belt` | Dashboard → ESP8266 | `"ON"` or `"OFF"` |

---

## Classification Logic

| Battery Type | Inductive Sensor | Voltage Range | Servo Action |
|-------------|-----------------|---------------|--------------|
| Li-ion | No metal (LOW) | 3.0–4.2V | Pass through (no servo) |
| Alkaline | No metal (LOW) | 0.8–1.6V | Servo A diverts to Bin A |
| NiMH | Metal (HIGH) | 1.0–1.5V | Servo B diverts to Bin B |

> In demo mode the firmware cycles Alkaline → NiMH → Li-ion with simulated voltages instead of reading live sensor data.

---

## Role-Based Access Control (RBAC)

| Role | Permissions |
|------|------------|
| Viewer | View dashboard, charts, event table |
| Admin | All Viewer permissions + conveyor control + user management |

- All new sign-ups receive **Viewer** role by default.
- First admin must be set manually in Firebase Realtime Database console: `users/{uid}/role = "admin"`.
- Admins can promote/demote other users from the User Management tab.

### Firebase Security Rules (Realtime Database)

```json
{
  "rules": {
    "users": {
      "$uid": {
        ".read": "auth != null && (auth.uid === $uid || root.child('users').child(auth.uid).child('role').val() === 'admin')",
        ".write": "auth != null && root.child('users').child(auth.uid).child('role').val() === 'admin'"
      }
    },
    "battery_events": {
      ".read": "auth != null",
      ".write": "auth != null"
    }
  }
}
```

---

## Local Development Setup

### Prerequisites

- Node.js ≥ 20.x
- npm ≥ 10.x
- Arduino IDE 2.x with ESP8266 board package
- Firebase CLI (`npm install -g firebase-tools`)

### Dashboard

```bash
git clone https://github.com/Zendrii04/Embedded-Final-Project.git
cd Embedded-Final-Project/ewaste-dashboard
npm install
npm run dev
```

Open `http://localhost:5173`

### Firmware

1. Install ESP8266 board package — add to Arduino IDE Board Manager URLs:
   ```
   http://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
2. Install required libraries (Library Manager):
   - `PubSubClient` by Nick O'Leary
   - `LiquidCrystal I2C` by Frank de Brabander
3. Set board: **NodeMCU 1.0 (ESP-12E Module)**
4. Fill in WiFi and MQTT credentials in `control.ino`
5. Upload — no BOOT button required (NodeMCU has auto-reset circuit)

---

## CI/CD — Firebase Hosting Auto-Deploy

Every push to `main` triggers the GitHub Actions workflow (`.github/workflows/firebase-deploy.yml`):

```
push to main → npm ci → npm run build → firebase deploy --only hosting
```

**Required GitHub Secret:**

| Secret Name | Value |
|-------------|-------|
| `FIREBASE_TOKEN` | Output of `npx firebase login:ci` |

Live URL: `https://embedded-final-project-29234.web.app`

---

## Environment Configuration

All credentials are hardcoded in source for this academic project. For production use, move these to environment variables:

| Variable | Location | Description |
|----------|----------|-------------|
| Firebase config | `src/firebase.js` | API key, project ID, database URL |
| MQTT broker URL | `src/App.vue` | HiveMQ WebSocket URL |
| MQTT credentials | `src/App.vue` | Username and password |
| WiFi SSID/pass | `control.ino` | Network credentials |
| MQTT credentials | `control.ino` | Broker username and password |

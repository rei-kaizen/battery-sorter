# E-Waste Sorter Dashboard

Real-time web dashboard for the E-Waste Battery Sorter system. Built with Vue 3 + Vite, connected to HiveMQ Cloud via MQTT over WebSockets, and backed by Firebase Realtime Database and Firebase Authentication.

---

## Prerequisites

| Tool | Version |
|------|---------|
| Node.js | ≥ 20.x |
| npm | ≥ 10.x |
| Firebase CLI | Latest (`npm i -g firebase-tools`) |

---

## Project Structure

```
ewaste-dashboard/
├── src/
│   ├── main.js                  # App entry point
│   ├── App.vue                  # Root: auth guard, MQTT client, RBAC, dashboard layout
│   ├── firebase.js              # Firebase singleton — exports db and auth
│   ├── style.css                # Global reset and base styles
│   └── components/
│       ├── LoginOverlay.vue     # Email/password auth + RTDB profile write on signup
│       ├── AdminPanel.vue       # Real-time user list, role promotion/demotion
│       ├── LiveVoltageChart.vue # Chart.js line chart — live voltage over time
│       ├── BatteryBarChart.vue  # Chart.js bar chart — count per chemistry
│       └── EventTable.vue       # Scrollable table of recent sort events
├── index.html                   # HTML shell, loads Inter font
├── vite.config.js               # Vite + Vue plugin config
├── firebase.json                # Hosting config — SPA rewrite rule, dist/ public dir
├── .firebaserc                  # Project alias binding
└── package.json
```

---

## Install and Run

```bash
# Install dependencies
npm install

# Start dev server (hot reload)
npm run dev

# Production build
npm run build

# Preview production build locally
npm run preview

# Deploy to Firebase Hosting
npx firebase deploy --only hosting
```

---

## Key Dependencies

| Package | Version | Purpose |
|---------|---------|---------|
| `vue` | ^3.5 | UI framework |
| `vite` | ^6.0 | Build tool and dev server |
| `@vitejs/plugin-vue` | ^5.2 | Vue SFC support in Vite |
| `mqtt` | ^5.15 | MQTT client over WebSockets |
| `firebase` | ^12.x | Auth, Realtime Database |
| `chart.js` | ^4.5 | Chart rendering |
| `vue-chartjs` | ^5.3 | Vue wrapper for Chart.js |

---

## Configuration

### Firebase (`src/firebase.js`)

Replace all placeholder values with your own Firebase project credentials obtained from the Firebase console:

```js
const firebaseConfig = {
  apiKey:            "YOUR_API_KEY",
  authDomain:        "YOUR_PROJECT_ID.firebaseapp.com",
  databaseURL:       "https://YOUR_PROJECT_ID-default-rtdb.REGION.firebasedatabase.app",
  projectId:         "YOUR_PROJECT_ID",
  storageBucket:     "YOUR_PROJECT_ID.firebasestorage.app",
  messagingSenderId: "YOUR_SENDER_ID",
  appId:             "YOUR_APP_ID"
}
```

### MQTT (`src/App.vue`)

Replace with your HiveMQ Cloud cluster details:

```js
mqtt.connect('wss://YOUR_HIVEMQ_CLUSTER_URL:8884/mqtt', {
  username: 'YOUR_MQTT_USERNAME',
  password: 'YOUR_MQTT_PASSWORD'
})
```

> Never commit real credentials. Fill these in locally only.

---

## Realtime Database Schema

```
/
├── battery_events/
│   └── {auto-push-id}/
│       ├── type:      "Li-ion" | "Alkaline" | "NiMH" | "Unknown"
│       ├── voltage:   float (V) — simulated 0.00–1.50 in demo mode
│       ├── magnetic:  boolean
│       └── timestamp: number (Unix ms)
│
└── users/
    └── {uid}/
        ├── email:     string
        ├── role:      "viewer" | "admin"
        └── createdAt: number (Unix ms)
```

---

## MQTT Topics

| Topic | Direction | Format |
|-------|-----------|--------|
| `ewaste/sort/events` | Subscribe | `{"type":"Li-ion","voltage":1.23,"magnetic":false,"timestamp":1748700000000}` |
| `ewaste/control/belt` | Publish | `"ON"` or `"OFF"` |

---

## Authentication and RBAC

- Firebase Auth (email/password)
- On sign-up: user profile written to `/users/{uid}` with `role: "viewer"`
- On login: role fetched from RTDB; profile created if missing (legacy accounts)
- **Viewer**: read-only dashboard access
- **Admin**: conveyor control + User Management tab
- First admin must be set manually in Firebase console: `/users/{uid}/role = "admin"`

### Auth State Flow

```
App mounts
  → onAuthStateChanged fires
      → user null     → show LoginOverlay
      → user exists   → fetchUserRole() → connectMQTT()
                         role = "admin" → show admin controls + User Management tab
                         role = "viewer" → show read-only dashboard
```

---

## Voltage Data — Demo vs Production

| Mode | Source | Value |
|------|--------|-------|
| **Demo (current)** | ESP8266 firmware `simulatedVoltage()` | Random 0.00–1.50V |
| **Production** | Voltage divider on A0 | Real battery terminal voltage |

The dashboard receives and displays whatever voltage value arrives in the MQTT payload — no dashboard changes needed when switching between demo and production firmware.

### Production Voltage Divider Circuit

```
Battery (+) ─── 100kΩ ─── A0 (NodeMCU) ─── 10kΩ ─── GND
Formula: V_battery = (ADC_raw / 1023.0) × 3.3 × 11.0
```

---

## Build and Deploy

### Manual Deploy

```bash
npm run build
npx firebase deploy --only hosting
```

### Automatic Deploy (GitHub Actions)

Every push to `main` triggers `.github/workflows/firebase-deploy.yml`:

```
npm ci → npm run build → firebase deploy --only hosting --token $FIREBASE_TOKEN
```

Required secret in GitHub repository settings:

| Secret | How to get |
|--------|-----------|
| `FIREBASE_TOKEN` | Run `npx firebase login:ci` and copy the output token |

---

## Development Notes

- **Bundle size** — `mqtt` + `chart.js` + `firebase` produces a ~975KB bundle. Expected for this stack given the real-time data requirements.
- **LiquidCrystal I2C AVR warning** — appears in Arduino IDE but is harmless; library works on ESP8266.
- **Firebase security rules** — apply RTDB rules from the root README to restrict role writes to admin users only.

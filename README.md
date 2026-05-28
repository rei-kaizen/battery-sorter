# Embedded Final Project: E-Waste Sorter Dashboard

An advanced IoT-enabled E-Waste sorting system. This repository contains the system configurations, battery metadata, and the web-based monitoring dashboard. The dashboard receives live sensor telemetry over MQTT (WebSockets) and records sorting events in a Firebase Realtime Database for visualization and tracking.

---

## 📁 Project Structure

```text
Embedded-Final-Project/
├── .gitignore                   # Root Git ignore configuration
├── README.md                    # Main project documentation (this file)
├── Battery handling.json        # Embedded system battery handling parameters
└── ewaste-dashboard/            # Vue.js 3 + Vite 6 web application
    ├── package.json             # Frontend dependencies & scripts
    ├── vite.config.js           # Vite development server configuration
    ├── index.html               # Main application entry point
    └── src/                     # Vue source code
        ├── main.js              # Vue app initialization
        ├── App.vue              # Main Dashboard view (MQTT + Firebase integrations)
        └── assets/              # Standard assets and styles
```

---

## 🚀 How to Clone the Repository (For Co-Developers)

To get a local copy of this project on your machine, open your terminal (Git Bash, Command Prompt, or PowerShell) and run:

```bash
# Clone the repository
git clone https://github.com/Zendrii04/Embedded-Final-Project.git

# Navigate into the project root directory
cd Embedded-Final-Project
```

---

## 🛠️ How to Set It Up Properly

### 1. Prerequisites
Ensure you have **Node.js** installed on your system:
* **Recommended Node version**: `v20.10.0` or higher.
* To check your Node version, run `node -v` in your terminal.

### 2. Install Dependencies
Navigate into the dashboard project directory and install the required npm packages:

```bash
# Move into the frontend application directory
cd ewaste-dashboard

# Install all project dependencies
npm install
```
This will install `vue`, `vite`, `firebase`, `mqtt`, `chart.js`, and `vue-chartjs` cleanly.

### 3. Database & MQTT Credentials Configuration
Open the file [`ewaste-dashboard/src/App.vue`](file:///c:/Users/Dan%20Zendrich%20Reyes/OneDrive/Documents/Embedded%20Final%20Project/ewaste-dashboard/src/App.vue) and update the configuration placeholders with your actual project credentials:

#### A. Firebase Realtime Database
Replace the credentials in `firebaseConfig`:
```javascript
const firebaseConfig = {
  apiKey: "YOUR_API_KEY",
  authDomain: "YOUR_PROJECT.firebaseapp.com",
  databaseURL: "https://YOUR_PROJECT.firebaseio.com",
  projectId: "YOUR_PROJECT",
};
```

#### B. HiveMQ / MQTT Broker
Update the WebSocket broker connection details in `onMounted()`:
```javascript
// Connect to HiveMQ Cloud via secure WebSockets
const client = mqtt.connect('wss://YOUR_HIVEMQ_CLUSTER_URL:8884/mqtt', {
  username: 'YOUR_USERNAME',
  password: 'YOUR_PASSWORD'
})
```

---

## 🖥️ How to Run It Properly

All the running commands must be executed from inside the **`ewaste-dashboard`** directory.

### 1. Start the Local Development Server
Launch the local dev environment with hot-reload enabled:

```bash
npm run dev
```
Once started, open your browser and navigate to `http://localhost:5173/` (or the URL shown in your console).

### 2. Compile and Build for Production
To generate a fully optimized build folder ready for deployment (under the `dist/` directory):

```bash
npm run build
```

### 3. Preview Production Build Locally
Verify the production build locally before hosting:

```bash
npm run preview
```

---

## ⚡ Tech Stack & Libraries Used

* **Frontend Framework**: Vue.js 3 (Composition API)
* **Build System**: Vite 6 (Configured for compatibility with older Node versions)
* **Realtime Database**: Firebase Realtime Database SDK
* **MQTT Telemetry**: MQTT.js (via WebSockets for browser-based subscriptions)
* **Charting & Visualization**: Chart.js & vue-chartjs

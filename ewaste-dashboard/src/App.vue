<script setup>
import { ref, onMounted } from 'vue'
import mqtt from 'mqtt'
import { initializeApp } from 'firebase/app'
import { getDatabase, ref as dbRef, push } from 'firebase/database'
import { getAuth, onAuthStateChanged, signOut, signInWithEmailAndPassword } from 'firebase/auth'

import LiveVoltageChart from './components/LiveVoltageChart.vue'
import BatteryBarChart from './components/BatteryBarChart.vue'
import EventTable from './components/EventTable.vue'

const firebaseConfig = {
  apiKey: "AIzaSyCTpypQQmGy43uiavAd6FrUcph-Ro6tvsw",
  authDomain: "embedded-final-project-29234.firebaseapp.com",
  databaseURL: "https://embedded-final-project-29234-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "embedded-final-project-29234",
  storageBucket: "embedded-final-project-29234.firebasestorage.app",
  messagingSenderId: "859318591733",
  appId: "1:859318591733:web:13d1ec2b45b054346da5bc",
  measurementId: "G-RLCE356SPC"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig)
const db = getDatabase(app)
const auth = getAuth(app)

// Authentication state
const isAuthenticated = ref(false)
const isAuthReady = ref(false)
const email = ref('')
const password = ref('')
const loginError = ref('')

const handleLogin = async () => {
  try {
    loginError.value = ''
    await signInWithEmailAndPassword(auth, email.value, password.value)
    email.value = ''
    password.value = ''
  } catch (error) {
    loginError.value = error.message
  }
}

// Dashboard state
const connectionStatus = ref('Connecting...')
const mqttClient = ref(null)

// Chart Data State
const events = ref([])
const voltageData = ref([])
const voltageLabels = ref([])
const chemistryCounts = ref({ 'Li-ion': 0, 'Alkaline': 0, 'NiMH': 0 })
const latestEvent = ref(null)

// Max data points to keep in memory for the live chart
const MAX_DATA_POINTS = 20

onMounted(() => {
  // Listen for authentication changes
  onAuthStateChanged(auth, (user) => {
    isAuthReady.value = true
    isAuthenticated.value = !!user
    
    // Connect to MQTT if logged in
    if (user && !mqttClient.value) {
      connectMQTT()
    }
  })
})

const connectMQTT = () => {
  // Connect to HiveMQ Cloud via WebSockets
  const client = mqtt.connect('wss://YOUR_HIVEMQ_CLUSTER_URL:8884/mqtt', {
    username: 'YOUR_USERNAME',
    password: 'YOUR_PASSWORD'
  })

  mqttClient.value = client

  client.on('connect', () => {
    connectionStatus.value = 'Connected to MQTT'
    client.subscribe('ewaste/sort/events')
  })

  client.on('message', (topic, message) => {
    if (topic === 'ewaste/sort/events') {
      try {
        const data = JSON.parse(message.toString())
        handleIncomingData(data)
      } catch (e) {
        console.error("Error parsing MQTT message", e)
      }
    }
  })
}

const handleIncomingData = (data) => {
  latestEvent.value = data
  
  // 1. Add to events list for the table (add to beginning)
  events.value.unshift(data)
  
  // 2. Update voltage chart
  const timeLabel = new Date(data.timestamp).toLocaleTimeString()
  voltageLabels.value.push(timeLabel)
  voltageData.value.push(data.voltage || 0)
  
  // Keep arrays limited to MAX_DATA_POINTS
  if (voltageLabels.value.length > MAX_DATA_POINTS) {
    voltageLabels.value.shift()
    voltageData.value.shift()
  }
  
  // 3. Update chemistry counts for the bar chart
  if (data.type) {
    if (chemistryCounts.value[data.type] !== undefined) {
      chemistryCounts.value[data.type]++
    }
  }

  // 4. Push event to Firebase Realtime Database
  const eventsListRef = dbRef(db, 'battery_events')
  push(eventsListRef, data)
}

const toggleBelt = (state) => {
  if (mqttClient.value && mqttClient.value.connected) {
    mqttClient.value.publish('ewaste/control/belt', state)
    console.log(`Command sent to ewaste/control/belt: ${state}`)
  } else {
    console.warn("Cannot send command: MQTT not connected")
  }
}

const handleLogout = async () => {
  await signOut(auth)
}
</script>

<template>
  <div v-if="!isAuthReady" class="loading-state">
    Initializing...
  </div>
  
  <div v-else>
    <!-- Simple Login UI overlay -->
    <div v-if="!isAuthenticated" class="login-overlay-container">
      <div class="login-panel">
        <h2>Admin Login</h2>
        <div class="login-form">
          <input type="email" v-model="email" placeholder="Email" class="auth-input" />
          <input type="password" v-model="password" placeholder="Password" class="auth-input" />
          <button @click="handleLogin" class="btn btn-primary">Log In</button>
        </div>
        <p v-if="loginError" class="error-msg">{{ loginError }}</p>
      </div>
    </div>
    
    <div v-else class="dashboard">
      <header class="header">
        <h1>E-Waste Sorter Dashboard</h1>
        <div class="header-controls">
          <span class="status-badge" :class="{'connected': connectionStatus === 'Connected to MQTT'}">
            {{ connectionStatus }}
          </span>
          <button @click="handleLogout" class="logout-btn">Log Out</button>
        </div>
      </header>
      
      <div class="control-panel">
        <button class="btn btn-primary" @click="toggleBelt('ON')">Start Conveyor</button>
        <button class="btn btn-danger" @click="toggleBelt('OFF')">Stop Conveyor</button>
      </div>

      <div class="grid-container">
      <!-- Top Row: Charts -->
      <div class="grid-item">
        <LiveVoltageChart :voltageData="voltageData" :labels="voltageLabels" />
      </div>
      <div class="grid-item">
        <BatteryBarChart :counts="chemistryCounts" />
      </div>
      
      <!-- Bottom Row: Event Table -->
      <div class="grid-item full-width">
        <EventTable :events="events" />
      </div>
    </div>
  </div>
  </div>
</template>

<style>
/* Global reset applied at App level to avoid side effects */
body {
  margin: 0;
  padding: 0;
  background-color: #0f172a;
  color: #f8fafc;
}
</style>

<style scoped>
.loading-state {
  display: flex;
  justify-content: center;
  align-items: center;
  height: 100vh;
  font-size: 1.5rem;
  color: #94a3b8;
  background-color: #0f172a;
}

.dashboard {
  font-family: 'Inter', -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  padding: 20px;
  max-width: 1200px;
  margin: 0 auto;
}

.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 25px;
  padding-bottom: 15px;
  border-bottom: 1px solid #1e293b;
}

.header h1 {
  margin: 0;
  color: #f8fafc;
  font-size: 1.8em;
  font-weight: 700;
  letter-spacing: -0.5px;
}

.header-controls {
  display: flex;
  align-items: center;
  gap: 15px;
}

.status-badge {
  padding: 6px 14px;
  border-radius: 20px;
  background-color: rgba(239, 68, 68, 0.2);
  color: #ef4444;
  border: 1px solid rgba(239, 68, 68, 0.3);
  font-size: 0.85em;
  font-weight: 600;
  letter-spacing: 0.3px;
}

.status-badge.connected {
  background-color: rgba(16, 185, 129, 0.2);
  color: #10b981;
  border: 1px solid rgba(16, 185, 129, 0.3);
}

.control-panel {
  background: #1e293b;
  padding: 20px;
  border-radius: 12px;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.2);
  margin-bottom: 25px;
  display: flex;
  gap: 15px;
  border: 1px solid #334155;
}

.btn {
  padding: 12px 24px;
  border: none;
  border-radius: 8px;
  font-size: 1em;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s ease;
}

.btn:active {
  transform: scale(0.98);
}

.btn-primary {
  background-color: #38bdf8;
  color: #0f172a;
}
.btn-primary:hover {
  background-color: #0ea5e9;
  box-shadow: 0 0 15px rgba(56, 189, 248, 0.4);
}

.btn-danger {
  background-color: #ef4444;
  color: white;
}
.btn-danger:hover {
  background-color: #dc2626;
  box-shadow: 0 0 15px rgba(239, 68, 68, 0.4);
}

.logout-btn {
  padding: 8px 16px;
  border: 1px solid #334155;
  background-color: #1e293b;
  color: #cbd5e1;
  border-radius: 8px;
  cursor: pointer;
  font-weight: 500;
  transition: all 0.2s;
}

.logout-btn:hover {
  background-color: #334155;
  color: #f8fafc;
}

.grid-container {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(450px, 1fr));
  gap: 25px;
}

.full-width {
  grid-column: 1 / -1;
}

.login-overlay-container {
  display: flex;
  justify-content: center;
  align-items: center;
  height: 100vh;
  background-color: #0f172a;
}

.login-panel {
  background: #1e293b;
  padding: 40px;
  border-radius: 12px;
  box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.5);
  border: 1px solid #334155;
  text-align: center;
  width: 100%;
  max-width: 350px;
}

.login-panel h2 {
  margin-top: 0;
  margin-bottom: 25px;
  font-size: 1.5em;
  color: #f8fafc;
}

.login-form {
  display: flex;
  flex-direction: column;
  gap: 15px;
}

.auth-input {
  padding: 10px 15px;
  border-radius: 8px;
  border: 1px solid #334155;
  background-color: #0f172a;
  color: #f8fafc;
  font-size: 1em;
}

.auth-input:focus {
  outline: none;
  border-color: #38bdf8;
}

.error-msg {
  color: #ef4444;
  margin-top: 10px;
  font-size: 0.9em;
  display: block;
}

@media (max-width: 768px) {
  .grid-container {
    grid-template-columns: 1fr;
  }
}
</style>
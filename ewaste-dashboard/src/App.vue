<script setup>
import { ref, onMounted } from 'vue'
import mqtt from 'mqtt'
import { initializeApp } from 'firebase/app'
import { getDatabase, ref as dbRef, push } from 'firebase/database'
import { getAuth, onAuthStateChanged, signOut } from 'firebase/auth'

import LoginOverlay from './components/LoginOverlay.vue'
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
    <LoginOverlay v-if="!isAuthenticated" />
    
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
  background-color: #f5f7fa;
}
</style>

<style scoped>
.loading-state {
  display: flex;
  justify-content: center;
  align-items: center;
  height: 100vh;
  font-size: 1.5rem;
  color: #666;
}

.dashboard {
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  padding: 20px;
  max-width: 1200px;
  margin: 0 auto;
}

.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
  padding-bottom: 10px;
  border-bottom: 1px solid #e1e4e8;
}

.header h1 {
  margin: 0;
  color: #2c3e50;
}

.header-controls {
  display: flex;
  align-items: center;
  gap: 15px;
}

.status-badge {
  padding: 6px 12px;
  border-radius: 20px;
  background-color: #e74c3c;
  color: white;
  font-size: 0.85em;
  font-weight: bold;
}

.status-badge.connected {
  background-color: #42b883;
}

.control-panel {
  background: white;
  padding: 15px;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
  margin-bottom: 20px;
  display: flex;
  gap: 15px;
}

.btn {
  padding: 10px 20px;
  border: none;
  border-radius: 6px;
  font-size: 1em;
  font-weight: bold;
  cursor: pointer;
  transition: opacity 0.2s;
}

.btn:hover {
  opacity: 0.85;
}

.btn-primary {
  background-color: #42b883;
  color: white;
}

.btn-danger {
  background-color: #e74c3c;
  color: white;
}

.logout-btn {
  padding: 6px 12px;
  border: 1px solid #ccc;
  background-color: transparent;
  border-radius: 6px;
  cursor: pointer;
}

.logout-btn:hover {
  background-color: #eee;
}

.grid-container {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(400px, 1fr));
  gap: 20px;
}

.full-width {
  grid-column: 1 / -1;
}

@media (max-width: 768px) {
  .grid-container {
    grid-template-columns: 1fr;
  }
}
</style>
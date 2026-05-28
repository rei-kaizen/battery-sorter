<script setup>
import { ref, onMounted } from 'vue'
import mqtt from 'mqtt'
import { initializeApp } from 'firebase/app'
import { getDatabase, ref as dbRef, push } from 'firebase/database'

// Your Firebase config (grab this from your Firebase project settings)
const firebaseConfig = {
  apiKey: "YOUR_API_KEY",
  authDomain: "YOUR_PROJECT.firebaseapp.com",
  databaseURL: "https://YOUR_PROJECT.firebaseio.com",
  projectId: "YOUR_PROJECT",
};

const app = initializeApp(firebaseConfig)
const db = getDatabase(app)

const latestEvent = ref(null)
const connectionStatus = ref('Connecting...')

onMounted(() => {
  // Connect to HiveMQ Cloud via WebSockets
  const client = mqtt.connect('wss://YOUR_HIVEMQ_CLUSTER_URL:8884/mqtt', {
    username: 'YOUR_USERNAME',
    password: 'YOUR_PASSWORD'
  })

  client.on('connect', () => {
    connectionStatus.value = 'Connected to MQTT'
    client.subscribe('ewaste/sort/events')
  })

  client.on('message', (topic, message) => {
    if (topic === 'ewaste/sort/events') {
      const data = JSON.parse(message.toString())
      latestEvent.value = data
      
      // Push event to Firebase
      const eventsListRef = dbRef(db, 'battery_events')
      push(eventsListRef, data)
    }
  })
})

const toggleBelt = (state) => {
  // We will add the MQTT publish logic for the belt control topic here
  console.log(`Belt turned ${state}`)
}
</script>

<template>
  <div class="dashboard">
    <h1>E-Waste Sorter Dashboard</h1>
    <p>Status: {{ connectionStatus }}</p>
    
    <div v-if="latestEvent" class="event-card">
      <h2>Latest Battery Scanned</h2>
      <p>Type: {{ latestEvent.type }}</p>
      <p>Voltage: {{ latestEvent.voltage }}V</p>
      <p>Magnetic: {{ latestEvent.magnetic ? 'Yes' : 'No' }}</p>
    </div>

    <button @click="toggleBelt('ON')">Start Conveyor</button>
    <button @click="toggleBelt('OFF')">Stop Conveyor</button>
  </div>
</template>

<style scoped>
.dashboard { font-family: sans-serif; padding: 20px; }
.event-card { border: 1px solid #ccc; padding: 15px; margin: 15px 0; }
button { margin-right: 10px; padding: 10px 15px; cursor: pointer; }
</style>
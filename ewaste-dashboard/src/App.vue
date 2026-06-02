<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import mqtt from 'mqtt'
import { auth, db } from './firebase.js'
import { onAuthStateChanged, signOut } from 'firebase/auth'
import { ref as dbRef, push, get, set, onValue } from 'firebase/database'

import LoginOverlay from './components/LoginOverlay.vue'
import LiveVoltageChart from './components/LiveVoltageChart.vue'
import BatteryBarChart from './components/BatteryBarChart.vue'
import EventTable from './components/EventTable.vue'
import AdminPanel from './components/AdminPanel.vue'

// Auth & role state
const isAuthReady = ref(false)
const isAuthenticated = ref(false)
const currentUser = ref(null)
const userRole = ref('viewer')
const isAdmin = computed(() => userRole.value === 'admin')

// Navigation
const activeTab = ref('dashboard')
const mobileMenuOpen = ref(false)

// MQTT / connection
const connectionStatus = ref('Disconnected')
const mqttClient = ref(null)
const statusClass = computed(() => {
  if (connectionStatus.value === 'Connected') return 'connected'
  if (connectionStatus.value === 'Connecting…') return 'connecting'
  return 'disconnected'
})

// Dashboard data
const events = ref([])
const voltageData = ref([])
const voltageLabels = ref([])
const chemistryCounts = ref({ 'Li-ion': 0, 'Alkaline': 0, 'NiMH': 0 })
const MAX_DATA_POINTS = 20
const MIN_VALID_TS = new Date('2020-01-01').getTime()
const voltageInitialized = ref(false)
let eventsUnsubscribe = null

// Conveyor control state
const beltRunning = ref(false)
const beltSpeedPct = ref(78)
const sessionStartTime = ref(null)
const sessionRuntime = ref(0)
let runtimeInterval = null

const formatRuntime = computed(() => {
  const s = sessionRuntime.value
  const m = Math.floor(s / 60)
  const sec = s % 60
  return `${String(m).padStart(2, '0')}:${String(sec).padStart(2, '0')}`
})

const lastDetectionTime = computed(() => {
  const ts = events.value[0]?.timestamp
  if (!ts || ts < MIN_VALID_TS) return '—'
  return new Date(ts).toLocaleTimeString()
})

const fetchUserRole = async (uid, email) => {
  const roleRef = dbRef(db, `users/${uid}`)
  const snap = await get(roleRef)
  if (snap.exists()) {
    userRole.value = snap.val().role || 'viewer'
  } else {
    await set(roleRef, { email, role: 'viewer', createdAt: Date.now() })
    userRole.value = 'viewer'
  }
}

const initEventsListener = () => {
  if (eventsUnsubscribe) eventsUnsubscribe()
  eventsUnsubscribe = onValue(dbRef(db, 'battery_events'), (snapshot) => {
    const data = snapshot.val()
    if (!data) {
      events.value = []
      chemistryCounts.value = { 'Li-ion': 0, 'Alkaline': 0, 'NiMH': 0 }
      return
    }
    const all = Object.values(data)
      .filter(e => e && e.timestamp)
      .sort((a, b) => b.timestamp - a.timestamp)
    events.value = all
    const counts = { 'Li-ion': 0, 'Alkaline': 0, 'NiMH': 0 }
    all.forEach(e => { if (e.type && counts[e.type] !== undefined) counts[e.type]++ })
    chemistryCounts.value = counts
    if (!voltageInitialized.value) {
      const recent = [...all].reverse().slice(-MAX_DATA_POINTS)
      voltageLabels.value = recent.map(e => new Date(e.timestamp).toLocaleTimeString())
      voltageData.value = recent.map(e => e.voltage || 0)
      voltageInitialized.value = true
    }
  })
}

onMounted(() => {
  onAuthStateChanged(auth, async (user) => {
    isAuthReady.value = true
    isAuthenticated.value = !!user
    currentUser.value = user
    if (user) {
      await fetchUserRole(user.uid, user.email)
      initEventsListener()
      if (!mqttClient.value) connectMQTT()
    } else {
      userRole.value = 'viewer'
      activeTab.value = 'dashboard'
      mobileMenuOpen.value = false
      if (eventsUnsubscribe) { eventsUnsubscribe(); eventsUnsubscribe = null }
      voltageInitialized.value = false
      events.value = []
      voltageData.value = []
      voltageLabels.value = []
      chemistryCounts.value = { 'Li-ion': 0, 'Alkaline': 0, 'NiMH': 0 }
    }
  })
})

onUnmounted(() => {
  if (runtimeInterval) clearInterval(runtimeInterval)
  if (eventsUnsubscribe) eventsUnsubscribe()
  if (mqttClient.value) mqttClient.value.end(true)
})

const connectMQTT = () => {
  // Connect to HiveMQ Cloud via WebSockets
  const client = mqtt.connect('wss://c70e75e155fe4621862844b2c027c528.s1.eu.hivemq.cloud:8884/mqtt', {
    username: 'Zendrii',
    password: 'B0Kmh99D4N!'
  })
  mqttClient.value = client

  client.on('connect', () => {
    connectionStatus.value = 'Connected'
    client.subscribe('ewaste/sort/events')
  })
  client.on('error', () => { connectionStatus.value = 'Error' })
  client.on('close', () => { connectionStatus.value = 'Disconnected' })
  client.on('message', (topic, message) => {
    if (topic !== 'ewaste/sort/events') return
    try { handleIncomingData(JSON.parse(message.toString())) } catch (e) { console.error(e) }
  })
}

const handleIncomingData = (data) => {
  const ts = (!data.timestamp || data.timestamp < MIN_VALID_TS) ? Date.now() : data.timestamp
  const normalized = { ...data, timestamp: ts }
  voltageLabels.value.push(new Date(ts).toLocaleTimeString())
  voltageData.value.push(normalized.voltage || 0)
  if (voltageLabels.value.length > MAX_DATA_POINTS) {
    voltageLabels.value.shift()
    voltageData.value.shift()
  }
  push(dbRef(db, 'battery_events'), normalized)
    .then(() => console.log('[Firebase] event saved:', normalized.type))
    .catch(e => console.error('[Firebase] write failed:', e.code, e.message))
}

const toggleBelt = (state) => {
  if (mqttClient.value?.connected) {
    mqttClient.value.publish('ewaste/control/belt', state)
    if (state === 'ON') {
      beltRunning.value = true
      sessionStartTime.value = Date.now()
      sessionRuntime.value = 0
      if (runtimeInterval) clearInterval(runtimeInterval)
      runtimeInterval = setInterval(() => {
        sessionRuntime.value = Math.floor((Date.now() - sessionStartTime.value) / 1000)
      }, 1000)
    } else {
      beltRunning.value = false
      if (runtimeInterval) { clearInterval(runtimeInterval); runtimeInterval = null }
    }
  } else {
    console.warn('MQTT not connected')
  }
}

const emergencyStop = () => {
  beltRunning.value = false
  if (runtimeInterval) { clearInterval(runtimeInterval); runtimeInterval = null }
  if (mqttClient.value?.connected) {
    mqttClient.value.publish('ewaste/control/belt', 'OFF')
    mqttClient.value.publish('ewaste/control/estop', '1')
  }
}

const onSpeedChange = () => {
  if (mqttClient.value?.connected) {
    mqttClient.value.publish('ewaste/control/speed', String(Math.round(beltSpeedPct.value * 255 / 100)))
  }
}

const handleLogout = async () => {
  if (runtimeInterval) { clearInterval(runtimeInterval); runtimeInterval = null }
  beltRunning.value = false
  sessionRuntime.value = 0
  if (eventsUnsubscribe) { eventsUnsubscribe(); eventsUnsubscribe = null }
  voltageInitialized.value = false
  if (mqttClient.value) {
    mqttClient.value.end(true)
    mqttClient.value = null
  }
  connectionStatus.value = 'Disconnected'
  await signOut(auth)
}

const setTab = (tab) => {
  activeTab.value = tab
  mobileMenuOpen.value = false
}
</script>

<template>
  <!-- Splash screen -->
  <div v-if="!isAuthReady" class="splash">
    <div class="splash-inner">
      <svg class="splash-logo" width="56" height="56" viewBox="0 0 48 48" fill="none" xmlns="http://www.w3.org/2000/svg">
        <rect x="2" y="12" width="36" height="24" rx="5" stroke="#38bdf8" stroke-width="2.5"/>
        <rect x="38" y="20" width="7" height="8" rx="2" fill="#38bdf8"/>
        <rect x="5" y="15" width="20" height="18" rx="2.5" fill="#38bdf8" opacity="0.75"/>
        <path d="M21 12 L16 24 H22 L17 36" stroke="#0f172a" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
        <circle cx="40" cy="38" r="8" fill="#10b981"/>
        <path d="M37 38 L39.5 40.5 L44 36" stroke="white" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>
      </svg>
      <span>Initializing…</span>
    </div>
  </div>

  <!-- Login overlay -->
  <LoginOverlay v-else-if="!isAuthenticated" />

  <!-- Main app -->
  <div v-else class="app-layout">

    <!-- Header -->
    <header class="app-header">
      <div class="header-brand">
        <svg width="36" height="36" viewBox="0 0 48 48" fill="none" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">
          <rect x="2" y="12" width="36" height="24" rx="5" stroke="#38bdf8" stroke-width="2.5"/>
          <rect x="38" y="20" width="7" height="8" rx="2" fill="#38bdf8"/>
          <rect x="5" y="15" width="20" height="18" rx="2.5" fill="#38bdf8" opacity="0.75"/>
          <path d="M21 12 L16 24 H22 L17 36" stroke="#0f172a" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
          <circle cx="40" cy="38" r="8" fill="#10b981"/>
          <path d="M37 38 L39.5 40.5 L44 36" stroke="white" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>
        </svg>
        <div class="brand-text">
          <span class="brand-title">Battery Sorter</span>
          <span class="brand-sub">E-Waste Dashboard</span>
        </div>
      </div>

      <!-- Desktop nav tabs -->
      <nav class="desktop-tabs" v-if="isAdmin">
        <button class="tab-btn" :class="{ active: activeTab === 'dashboard' }" @click="setTab('dashboard')">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/>
            <rect x="14" y="14" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/>
          </svg>
          Dashboard
        </button>
        <button class="tab-btn" :class="{ active: activeTab === 'admin' }" @click="setTab('admin')">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"/>
            <circle cx="9" cy="7" r="4"/>
            <path d="M23 21v-2a4 4 0 0 0-3-3.87"/>
            <path d="M16 3.13a4 4 0 0 1 0 7.75"/>
          </svg>
          User Management
        </button>
      </nav>

      <div class="header-right">
        <span class="status-badge" :class="statusClass">
          <span class="status-dot"></span>
          {{ connectionStatus }}
        </span>
        <div class="user-info">
          <span class="user-email">{{ currentUser?.email }}</span>
          <span class="role-badge" :class="userRole">{{ userRole === 'admin' ? 'Admin' : 'Viewer' }}</span>
        </div>
        <button class="logout-btn" @click="handleLogout" title="Log Out">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/>
            <polyline points="16 17 21 12 16 7"/>
            <line x1="21" y1="12" x2="9" y2="12"/>
          </svg>
          <span class="logout-label">Log Out</span>
        </button>
        <!-- Mobile menu toggle -->
        <button v-if="isAdmin" class="mobile-menu-btn" @click="mobileMenuOpen = !mobileMenuOpen" :aria-expanded="mobileMenuOpen" aria-label="Toggle menu">
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round">
            <line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="18" x2="21" y2="18"/>
          </svg>
        </button>
      </div>
    </header>

    <!-- Mobile nav dropdown -->
    <div class="mobile-nav" v-if="isAdmin && mobileMenuOpen">
      <button class="mobile-tab-btn" :class="{ active: activeTab === 'dashboard' }" @click="setTab('dashboard')">Dashboard</button>
      <button class="mobile-tab-btn" :class="{ active: activeTab === 'admin' }" @click="setTab('admin')">User Management</button>
    </div>

    <!-- Main content -->
    <main class="main-content">

      <!-- Dashboard view -->
      <template v-if="activeTab === 'dashboard'">
        <!-- Admin-only conveyor controls -->
        <section v-if="isAdmin" class="control-section">
          <div class="cs-header">
            <div class="section-label">
              <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round">
                <circle cx="12" cy="12" r="3"/><path d="M19.07 4.93a10 10 0 0 1 0 14.14"/>
                <path d="M4.93 4.93a10 10 0 0 0 0 14.14"/>
              </svg>
              Conveyor Control
            </div>
            <span class="belt-status-badge" :class="beltRunning ? 'running' : 'stopped'">
              <span class="belt-status-dot"></span>
              {{ beltRunning ? 'Running' : 'Stopped' }}
            </span>
          </div>

          <div class="cs-body">
            <!-- Controls column -->
            <div class="cs-controls">
              <div class="main-btns">
                <button class="btn btn-start" :disabled="beltRunning || !mqttClient?.connected" @click="toggleBelt('ON')">
                  <svg width="13" height="13" viewBox="0 0 24 24" fill="currentColor"><polygon points="5 3 19 12 5 21 5 3"/></svg>
                  Start
                </button>
                <button class="btn btn-stop" :disabled="!beltRunning || !mqttClient?.connected" @click="toggleBelt('OFF')">
                  <svg width="13" height="13" viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="4" width="4" height="16"/><rect x="14" y="4" width="4" height="16"/></svg>
                  Stop
                </button>
              </div>

              <div class="speed-row">
                <span class="speed-label">Speed</span>
                <input
                  type="range" min="20" max="100"
                  v-model.number="beltSpeedPct"
                  @change="onSpeedChange"
                  class="speed-slider"
                />
                <span class="speed-val">{{ beltSpeedPct }}%</span>
              </div>

              <button class="btn btn-estop" @click="emergencyStop" :disabled="!mqttClient?.connected">
                <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round">
                  <circle cx="12" cy="12" r="9"/><line x1="9" y1="9" x2="15" y2="15"/><line x1="15" y1="9" x2="9" y2="15"/>
                </svg>
                Emergency Stop
              </button>
            </div>

            <div class="cs-divider"></div>

            <!-- Statistics column -->
            <div class="cs-stats">
              <div class="cs-stat-row">
                <span class="cs-stat-label">Session Runtime</span>
                <span class="cs-stat-val mono">{{ formatRuntime }}</span>
              </div>
              <div class="cs-stat-row">
                <span class="cs-stat-label">Total Sorted</span>
                <span class="cs-stat-val">{{ events.length }}</span>
              </div>
              <div class="cs-stat-row">
                <span class="cs-stat-label">Last Detection</span>
                <span class="cs-stat-val mono">{{ lastDetectionTime }}</span>
              </div>
              <div class="cs-inner-divider"></div>
              <div class="cs-chem-row">
                <span class="chem-chip li-ion">Li-ion <b>{{ chemistryCounts['Li-ion'] }}</b></span>
                <span class="chem-chip alkaline">Alkaline <b>{{ chemistryCounts['Alkaline'] }}</b></span>
                <span class="chem-chip nimh">NiMH <b>{{ chemistryCounts['NiMH'] }}</b></span>
              </div>
            </div>
          </div>
        </section>

        <!-- Viewer info banner -->
        <div v-else class="viewer-banner">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/>
          </svg>
          You have <strong>Viewer</strong> access — monitoring only. Contact an admin to request elevated privileges.
        </div>

        <!-- Stats row -->
        <div class="stats-row">
          <div class="stat-card">
            <span class="stat-value">{{ events.length }}</span>
            <span class="stat-label">Total Events</span>
          </div>
          <div class="stat-card li-ion">
            <span class="stat-value">{{ chemistryCounts['Li-ion'] }}</span>
            <span class="stat-label">Li-ion</span>
          </div>
          <div class="stat-card alkaline">
            <span class="stat-value">{{ chemistryCounts['Alkaline'] }}</span>
            <span class="stat-label">Alkaline</span>
          </div>
          <div class="stat-card nimh">
            <span class="stat-value">{{ chemistryCounts['NiMH'] }}</span>
            <span class="stat-label">NiMH</span>
          </div>
        </div>

        <!-- Charts -->
        <div class="charts-grid">
          <LiveVoltageChart :voltageData="voltageData" :labels="voltageLabels" />
          <BatteryBarChart :counts="chemistryCounts" />
        </div>

        <!-- Event table -->
        <EventTable :events="events" />
      </template>

      <!-- Admin panel view -->
      <template v-else-if="activeTab === 'admin' && isAdmin">
        <AdminPanel :current-user-uid="currentUser?.uid" />
      </template>
    </main>
  </div>
</template>

<style scoped>
/* ── Splash ── */
.splash {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 100vh;
  background: #0f172a;
}
.splash-inner {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 16px;
  color: #64748b;
  font-size: 0.95rem;
  animation: pulse 1.6s ease-in-out infinite;
}
@keyframes pulse {
  0%, 100% { opacity: 0.6; }
  50% { opacity: 1; }
}

/* ── Layout ── */
.app-layout {
  display: flex;
  flex-direction: column;
  min-height: 100vh;
  background: #0f172a;
}

/* ── Header ── */
.app-header {
  position: sticky;
  top: 0;
  z-index: 100;
  display: flex;
  align-items: center;
  gap: 16px;
  padding: 0 24px;
  height: 64px;
  background: rgba(15, 23, 42, 0.95);
  backdrop-filter: blur(12px);
  border-bottom: 1px solid #1e293b;
}

.header-brand {
  display: flex;
  align-items: center;
  gap: 12px;
  flex-shrink: 0;
}

.brand-text {
  display: flex;
  flex-direction: column;
  line-height: 1.2;
}

.brand-title {
  font-size: 1rem;
  font-weight: 700;
  color: #f8fafc;
  letter-spacing: -0.3px;
}

.brand-sub {
  font-size: 0.7rem;
  color: #64748b;
  font-weight: 500;
}

.desktop-tabs {
  display: flex;
  align-items: center;
  gap: 4px;
  margin-left: 8px;
}

.tab-btn {
  display: flex;
  align-items: center;
  gap: 7px;
  padding: 7px 14px;
  border-radius: 8px;
  border: none;
  background: transparent;
  color: #64748b;
  font-size: 0.875rem;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.2s;
  font-family: inherit;
}

.tab-btn:hover {
  background: #1e293b;
  color: #cbd5e1;
}

.tab-btn.active {
  background: rgba(56, 189, 248, 0.12);
  color: #38bdf8;
  font-weight: 600;
}

.header-right {
  margin-left: auto;
  display: flex;
  align-items: center;
  gap: 12px;
  flex-shrink: 0;
}

.status-badge {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 5px 12px;
  border-radius: 99px;
  font-size: 0.8rem;
  font-weight: 600;
  white-space: nowrap;
  border: 1px solid;
}

.status-dot {
  width: 7px;
  height: 7px;
  border-radius: 50%;
  flex-shrink: 0;
}

.status-badge.connected {
  background: rgba(16, 185, 129, 0.12);
  color: #10b981;
  border-color: rgba(16, 185, 129, 0.25);
}
.status-badge.connected .status-dot {
  background: #10b981;
  box-shadow: 0 0 0 3px rgba(16, 185, 129, 0.2);
  animation: ping 2s ease-in-out infinite;
}

.status-badge.connecting {
  background: rgba(251, 191, 36, 0.1);
  color: #fbbf24;
  border-color: rgba(251, 191, 36, 0.25);
}
.status-badge.connecting .status-dot { background: #fbbf24; }

.status-badge.disconnected, .status-badge.error {
  background: rgba(239, 68, 68, 0.1);
  color: #ef4444;
  border-color: rgba(239, 68, 68, 0.25);
}
.status-badge.disconnected .status-dot { background: #ef4444; }

@keyframes ping {
  0%, 100% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.4); }
  50% { box-shadow: 0 0 0 4px rgba(16, 185, 129, 0); }
}

.user-info {
  display: flex;
  flex-direction: column;
  align-items: flex-end;
  gap: 2px;
}

.user-email {
  font-size: 0.8rem;
  color: #94a3b8;
  max-width: 160px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.role-badge {
  display: inline-block;
  padding: 2px 8px;
  border-radius: 99px;
  font-size: 0.7rem;
  font-weight: 700;
  letter-spacing: 0.03em;
  text-transform: uppercase;
}

.role-badge.admin {
  background: rgba(16, 185, 129, 0.15);
  color: #10b981;
}

.role-badge.viewer {
  background: rgba(56, 189, 248, 0.12);
  color: #38bdf8;
}

.logout-btn {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 8px 14px;
  border: 1px solid #334155;
  background: transparent;
  color: #94a3b8;
  border-radius: 8px;
  cursor: pointer;
  font-size: 0.85rem;
  font-weight: 500;
  transition: all 0.2s;
  font-family: inherit;
  white-space: nowrap;
}

.logout-btn:hover {
  border-color: #475569;
  background: #1e293b;
  color: #f8fafc;
}

.mobile-menu-btn {
  display: none;
  align-items: center;
  justify-content: center;
  width: 38px;
  height: 38px;
  border: 1px solid #334155;
  background: transparent;
  color: #94a3b8;
  border-radius: 8px;
  cursor: pointer;
  padding: 0;
  transition: all 0.2s;
}

.mobile-menu-btn:hover {
  background: #1e293b;
  color: #f8fafc;
}

/* ── Mobile nav dropdown ── */
.mobile-nav {
  display: flex;
  flex-direction: column;
  background: #1e293b;
  border-bottom: 1px solid #334155;
  padding: 8px 16px;
  gap: 4px;
}

.mobile-tab-btn {
  padding: 12px 16px;
  background: transparent;
  border: none;
  color: #94a3b8;
  font-size: 0.9rem;
  font-weight: 500;
  text-align: left;
  border-radius: 8px;
  cursor: pointer;
  font-family: inherit;
  transition: all 0.2s;
}

.mobile-tab-btn:hover, .mobile-tab-btn.active {
  background: rgba(56, 189, 248, 0.1);
  color: #38bdf8;
}

/* ── Main content ── */
.main-content {
  flex: 1;
  padding: 24px;
  max-width: 1300px;
  width: 100%;
  margin: 0 auto;
  display: flex;
  flex-direction: column;
  gap: 20px;
}

/* ── Control section ── */
.control-section {
  background: #1e293b;
  border: 1px solid #334155;
  border-radius: 14px;
  padding: 18px 20px;
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.cs-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
}

.section-label {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 0.8rem;
  font-weight: 600;
  color: #64748b;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  white-space: nowrap;
}

.belt-status-badge {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 4px 12px;
  border-radius: 99px;
  font-size: 0.8rem;
  font-weight: 600;
  border: 1px solid;
}

.belt-status-dot {
  width: 7px;
  height: 7px;
  border-radius: 50%;
  flex-shrink: 0;
}

.belt-status-badge.running {
  background: rgba(16, 185, 129, 0.12);
  color: #10b981;
  border-color: rgba(16, 185, 129, 0.3);
}
.belt-status-badge.running .belt-status-dot {
  background: #10b981;
  animation: ping 2s ease-in-out infinite;
}
.belt-status-badge.stopped {
  background: rgba(100, 116, 139, 0.1);
  color: #64748b;
  border-color: rgba(100, 116, 139, 0.2);
}
.belt-status-badge.stopped .belt-status-dot { background: #64748b; }

.cs-body {
  display: flex;
  align-items: flex-start;
  gap: 0;
}

.cs-controls {
  display: flex;
  flex-direction: column;
  gap: 14px;
  min-width: 240px;
  flex-shrink: 0;
}

.main-btns {
  display: flex;
  gap: 10px;
}

.btn {
  display: flex;
  align-items: center;
  gap: 7px;
  padding: 9px 18px;
  border: none;
  border-radius: 9px;
  font-size: 0.875rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s;
  font-family: inherit;
  white-space: nowrap;
}

.btn:active:not(:disabled) { transform: scale(0.97); }
.btn:disabled { opacity: 0.38; cursor: not-allowed; }

.btn-start {
  background: rgba(16, 185, 129, 0.15);
  color: #10b981;
  border: 1px solid rgba(16, 185, 129, 0.3);
}
.btn-start:hover:not(:disabled) {
  background: rgba(16, 185, 129, 0.25);
  box-shadow: 0 0 14px rgba(16, 185, 129, 0.2);
}

.btn-stop {
  background: rgba(239, 68, 68, 0.12);
  color: #ef4444;
  border: 1px solid rgba(239, 68, 68, 0.25);
}
.btn-stop:hover:not(:disabled) {
  background: rgba(239, 68, 68, 0.22);
  box-shadow: 0 0 14px rgba(239, 68, 68, 0.2);
}

.speed-row {
  display: flex;
  align-items: center;
  gap: 10px;
}

.speed-label {
  font-size: 0.75rem;
  font-weight: 600;
  color: #64748b;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  white-space: nowrap;
  width: 42px;
}

.speed-slider {
  flex: 1;
  accent-color: #38bdf8;
  cursor: pointer;
}

.speed-val {
  font-size: 0.82rem;
  font-weight: 700;
  color: #38bdf8;
  font-variant-numeric: tabular-nums;
  width: 34px;
  text-align: right;
}

.btn-estop {
  background: rgba(239, 68, 68, 0.08);
  color: #ef4444;
  border: 1px solid rgba(239, 68, 68, 0.3);
  justify-content: center;
  letter-spacing: 0.02em;
}
.btn-estop:hover:not(:disabled) {
  background: rgba(239, 68, 68, 0.2);
  border-color: #ef4444;
  box-shadow: 0 0 20px rgba(239, 68, 68, 0.25);
}

.cs-divider {
  width: 1px;
  background: #334155;
  align-self: stretch;
  margin: 0 22px;
  flex-shrink: 0;
}

.cs-stats {
  flex: 1;
  display: flex;
  flex-direction: column;
  gap: 9px;
  min-width: 0;
}

.cs-stat-row {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 8px;
}

.cs-stat-label {
  font-size: 0.75rem;
  color: #64748b;
  font-weight: 500;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  white-space: nowrap;
}

.cs-stat-val {
  font-size: 0.95rem;
  font-weight: 700;
  color: #f1f5f9;
}

.cs-stat-val.mono {
  font-size: 0.875rem;
  color: #94a3b8;
  font-variant-numeric: tabular-nums;
}

.cs-inner-divider {
  height: 1px;
  background: #334155;
  margin: 3px 0;
}

.cs-chem-row {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
}

.chem-chip {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  padding: 4px 10px;
  border-radius: 99px;
  font-size: 0.78rem;
  font-weight: 500;
  border: 1px solid;
}

.chem-chip b { font-weight: 800; }

.chem-chip.li-ion {
  background: rgba(248, 121, 121, 0.1);
  color: #f87979;
  border-color: rgba(248, 121, 121, 0.25);
}

.chem-chip.alkaline {
  background: rgba(56, 189, 248, 0.1);
  color: #38bdf8;
  border-color: rgba(56, 189, 248, 0.25);
}

.chem-chip.nimh {
  background: rgba(252, 211, 77, 0.1);
  color: #fcd34d;
  border-color: rgba(252, 211, 77, 0.25);
}

/* ── Viewer banner ── */
.viewer-banner {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 13px 18px;
  background: rgba(56, 189, 248, 0.07);
  border: 1px solid rgba(56, 189, 248, 0.2);
  border-radius: 10px;
  color: #94a3b8;
  font-size: 0.875rem;
}

.viewer-banner strong { color: #38bdf8; }

/* ── Stats row ── */
.stats-row {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 14px;
}

.stat-card {
  background: #1e293b;
  border: 1px solid #334155;
  border-radius: 12px;
  padding: 18px 20px;
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.stat-value {
  font-size: 2rem;
  font-weight: 700;
  color: #f8fafc;
  line-height: 1;
}

.stat-label {
  font-size: 0.8rem;
  color: #64748b;
  font-weight: 500;
}

.stat-card.li-ion .stat-value { color: #f87979; }
.stat-card.alkaline .stat-value { color: #38bdf8; }
.stat-card.nimh .stat-value { color: #fcd34d; }

/* ── Charts grid ── */
.charts-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(420px, 1fr));
  gap: 20px;
}

/* ── Responsive ── */
@media (max-width: 900px) {
  .stats-row {
    grid-template-columns: repeat(2, 1fr);
  }
}

@media (max-width: 768px) {
  .app-header {
    padding: 0 16px;
    gap: 10px;
  }

  .brand-sub { display: none; }

  .desktop-tabs { display: none; }

  .mobile-menu-btn { display: flex; }

  .status-badge {
    font-size: 0;
    padding: 7px;
    gap: 0;
  }
  .status-badge .status-dot {
    width: 9px;
    height: 9px;
  }

  .user-info { display: none; }

  .logout-label { display: none; }

  .logout-btn {
    padding: 8px;
    gap: 0;
  }

  .main-content {
    padding: 16px;
    gap: 16px;
  }

  .cs-body { flex-direction: column; }
  .cs-divider { width: 100%; height: 1px; margin: 4px 0; }
  .cs-controls { min-width: 0; width: 100%; }
  .main-btns { width: 100%; }
  .btn-start, .btn-stop { flex: 1; justify-content: center; }
  .btn-estop { width: 100%; }

  .stats-row {
    grid-template-columns: repeat(2, 1fr);
    gap: 10px;
  }

  .stat-card { padding: 14px 16px; }
  .stat-value { font-size: 1.6rem; }

  .charts-grid {
    grid-template-columns: 1fr;
  }
}

@media (max-width: 480px) {
  .stats-row { grid-template-columns: repeat(2, 1fr); }
}
</style>

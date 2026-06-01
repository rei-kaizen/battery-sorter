<script setup>
import { ref, computed } from 'vue'

const props = defineProps({
  events: { type: Array, default: () => [] }
})

const typeFilter = ref('All')
const dateFilter = ref('all')

const filteredEvents = computed(() => {
  let list = props.events
  if (typeFilter.value !== 'All') {
    list = list.filter(e => e.type === typeFilter.value)
  }
  if (dateFilter.value === 'today') {
    const startOfDay = new Date()
    startOfDay.setHours(0, 0, 0, 0)
    list = list.filter(e => e.timestamp >= startOfDay.getTime())
  } else if (dateFilter.value === 'week') {
    const weekAgo = Date.now() - 7 * 24 * 60 * 60 * 1000
    list = list.filter(e => e.timestamp >= weekAgo)
  }
  return list
})

const MIN_VALID_TS = new Date('2020-01-01').getTime()

const formatTime = (ts) => {
  if (!ts || ts < MIN_VALID_TS) return 'N/A'
  const d = new Date(ts)
  const isToday = d.toDateString() === new Date().toDateString()
  if (isToday) return d.toLocaleTimeString()
  return d.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' }) +
    ' · ' + d.toLocaleTimeString(undefined, { hour: '2-digit', minute: '2-digit' })
}

const typeClass = (type) => type?.toLowerCase() ?? ''

const TYPE_FILTERS = ['All', 'Li-ion', 'Alkaline', 'NiMH']
const DATE_FILTERS = [
  { value: 'all', label: 'All Time' },
  { value: 'today', label: 'Today' },
  { value: 'week', label: 'This Week' }
]
</script>

<template>
  <div class="table-container">
    <div class="table-header">
      <h3>Sort Event History</h3>
      <span class="event-count">{{ filteredEvents.length }} / {{ events.length }} events</span>
    </div>

    <div class="filters">
      <div class="filter-group">
        <button
          v-for="t in TYPE_FILTERS"
          :key="t"
          class="filter-btn"
          :class="[{ active: typeFilter === t }, t !== 'All' ? typeClass(t) : '']"
          @click="typeFilter = t"
        >{{ t }}</button>
      </div>
      <div class="filter-divider"></div>
      <div class="filter-group">
        <button
          v-for="d in DATE_FILTERS"
          :key="d.value"
          class="filter-btn"
          :class="{ active: dateFilter === d.value }"
          @click="dateFilter = d.value"
        >{{ d.label }}</button>
      </div>
    </div>

    <div class="table-scroll">
      <table>
        <thead>
          <tr>
            <th>Time</th>
            <th>Type</th>
            <th>Voltage</th>
            <th>Magnetic</th>
          </tr>
        </thead>
        <tbody>
          <tr
            v-for="(event, index) in filteredEvents"
            :key="index"
            :class="['event-row', typeClass(event.type)]"
          >
            <td class="time-cell">{{ formatTime(event.timestamp) }}</td>
            <td>
              <span :class="['badge', typeClass(event.type)]">
                {{ event.type || 'Unknown' }}
              </span>
            </td>
            <td class="voltage-cell">{{ event.voltage != null ? event.voltage.toFixed(2) + 'V' : '—' }}</td>
            <td>
              <span :class="['magnetic-pill', event.magnetic ? 'yes' : 'no']">
                {{ event.magnetic ? 'Yes' : 'No' }}
              </span>
            </td>
          </tr>
          <tr v-if="filteredEvents.length === 0">
            <td colspan="4" class="empty-state">
              {{ events.length === 0
                ? 'No events yet. Waiting for incoming data...'
                : 'No events match the current filters.' }}
            </td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>
</template>

<style scoped>
.table-container {
  background: #1e293b;
  padding: 20px;
  border-radius: 12px;
  box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.2);
  display: flex;
  flex-direction: column;
  border: 1px solid #334155;
}

.table-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 12px;
}

.table-header h3 {
  margin: 0;
  color: #f8fafc;
  font-weight: 600;
  letter-spacing: -0.3px;
}

.event-count {
  font-size: 0.8rem;
  color: #64748b;
  font-variant-numeric: tabular-nums;
}

.filters {
  display: flex;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
  margin-bottom: 14px;
}

.filter-group {
  display: flex;
  gap: 6px;
  flex-wrap: wrap;
}

.filter-divider {
  width: 1px;
  height: 20px;
  background: #334155;
  flex-shrink: 0;
}

.filter-btn {
  padding: 5px 12px;
  border-radius: 99px;
  border: 1px solid #334155;
  background: transparent;
  color: #64748b;
  font-size: 0.78rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.18s;
  font-family: inherit;
  white-space: nowrap;
}

.filter-btn:hover {
  background: #334155;
  color: #cbd5e1;
}

.filter-btn.active {
  border-color: #475569;
  background: #334155;
  color: #f1f5f9;
}

.filter-btn.li-ion.active {
  background: rgba(248, 121, 121, 0.15);
  border-color: rgba(248, 121, 121, 0.4);
  color: #f87979;
}

.filter-btn.alkaline.active {
  background: rgba(54, 162, 235, 0.15);
  border-color: rgba(56, 189, 248, 0.4);
  color: #38bdf8;
}

.filter-btn.nimh.active {
  background: rgba(255, 205, 86, 0.15);
  border-color: rgba(252, 211, 77, 0.4);
  color: #fcd34d;
}

.table-scroll {
  overflow-y: auto;
  max-height: 400px;
}

table {
  width: 100%;
  border-collapse: collapse;
  text-align: left;
  color: #cbd5e1;
}

th, td {
  padding: 11px 14px;
  border-bottom: 1px solid #334155;
}

th {
  background-color: #0f172a;
  color: #94a3b8;
  position: sticky;
  top: 0;
  z-index: 1;
  font-weight: 600;
  letter-spacing: 0.5px;
  text-transform: uppercase;
  font-size: 0.8rem;
}

.event-row {
  transition: background 0.15s;
}

.event-row:hover {
  background-color: rgba(51, 65, 85, 0.4);
}

.event-row.li-ion { border-left: 2px solid rgba(248, 121, 121, 0.5); }
.event-row.alkaline { border-left: 2px solid rgba(56, 189, 248, 0.5); }
.event-row.nimh { border-left: 2px solid rgba(252, 211, 77, 0.5); }

.time-cell {
  font-size: 0.85rem;
  color: #94a3b8;
  white-space: nowrap;
}

.voltage-cell {
  font-variant-numeric: tabular-nums;
}

.empty-state {
  text-align: center;
  color: #64748b;
  padding: 30px;
  font-style: italic;
}

.badge {
  padding: 4px 10px;
  border-radius: 12px;
  font-size: 0.82em;
  font-weight: 700;
  letter-spacing: 0.5px;
  white-space: nowrap;
}

.badge.li-ion {
  background-color: rgba(248, 121, 121, 0.2);
  color: #f87979;
  border: 1px solid rgba(248, 121, 121, 0.3);
}

.badge.alkaline {
  background-color: rgba(54, 162, 235, 0.2);
  color: #38bdf8;
  border: 1px solid rgba(54, 162, 235, 0.3);
}

.badge.nimh {
  background-color: rgba(255, 205, 86, 0.2);
  color: #fcd34d;
  border: 1px solid rgba(255, 205, 86, 0.3);
}

.magnetic-pill {
  font-size: 0.82rem;
  font-weight: 600;
  padding: 3px 9px;
  border-radius: 99px;
}

.magnetic-pill.yes {
  background: rgba(16, 185, 129, 0.12);
  color: #10b981;
  border: 1px solid rgba(16, 185, 129, 0.25);
}

.magnetic-pill.no {
  background: rgba(100, 116, 139, 0.1);
  color: #64748b;
  border: 1px solid rgba(100, 116, 139, 0.2);
}
</style>

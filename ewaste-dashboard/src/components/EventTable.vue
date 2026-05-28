<script setup>
const props = defineProps({
  events: {
    type: Array,
    default: () => []
  }
})

const formatTime = (timestamp) => {
  if (!timestamp) return 'N/A'
  const d = new Date(timestamp)
  return d.toLocaleTimeString()
}
</script>

<template>
  <div class="table-container">
    <h3>Recent Sort Events</h3>
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
          <tr v-for="(event, index) in events" :key="index">
            <td>{{ formatTime(event.timestamp) }}</td>
            <td>
              <span :class="['badge', event.type?.toLowerCase()]">
                {{ event.type || 'Unknown' }}
              </span>
            </td>
            <td>{{ event.voltage ? event.voltage.toFixed(2) + 'V' : '-' }}</td>
            <td>{{ event.magnetic ? 'Yes' : 'No' }}</td>
          </tr>
          <tr v-if="events.length === 0">
            <td colspan="4" class="empty-state">No events yet. Waiting for incoming data...</td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>
</template>

<style scoped>
.table-container {
  background: white;
  padding: 15px;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
  display: flex;
  flex-direction: column;
  max-height: 400px;
}
.table-container h3 {
  margin-top: 0;
  color: #333;
}
.table-scroll {
  overflow-y: auto;
}
table {
  width: 100%;
  border-collapse: collapse;
  text-align: left;
}
th, td {
  padding: 10px;
  border-bottom: 1px solid #eee;
}
th {
  background-color: #f9f9f9;
  position: sticky;
  top: 0;
  z-index: 1;
}
.empty-state {
  text-align: center;
  color: #999;
  padding: 20px;
}
.badge {
  padding: 4px 8px;
  border-radius: 12px;
  font-size: 0.85em;
  font-weight: bold;
}
.badge.li-ion {
  background-color: #f87979;
  color: white;
}
.badge.alkaline {
  background-color: #36a2eb;
  color: white;
}
.badge.nimh {
  background-color: #ffcd56;
  color: #333;
}
</style>

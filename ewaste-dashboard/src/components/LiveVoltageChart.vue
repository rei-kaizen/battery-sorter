<script setup>
import { computed } from 'vue'
import { Line } from 'vue-chartjs'
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
} from 'chart.js'

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
)

const props = defineProps({
  voltageData: {
    type: Array,
    default: () => []
  },
  labels: {
    type: Array,
    default: () => []
  }
})

const chartData = computed(() => ({
  labels: props.labels,
  datasets: [
    {
      label: 'Battery Voltage (V)',
      backgroundColor: '#42b883',
      borderColor: '#42b883',
      data: props.voltageData,
      tension: 0.3
    }
  ]
}))

const chartOptions = {
  responsive: true,
  maintainAspectRatio: false,
  animation: {
    duration: 500 // smooth transition for live data
  },
  scales: {
    y: {
      beginAtZero: true,
      max: 5,
      title: {
        display: true,
        text: 'Voltage (V)'
      }
    },
    x: {
      title: {
        display: true,
        text: 'Time'
      }
    }
  }
}
</script>

<template>
  <div class="chart-container">
    <h3>Live Voltage Reading</h3>
    <div class="chart-wrapper">
      <Line :data="chartData" :options="chartOptions" />
    </div>
  </div>
</template>

<style scoped>
.chart-container {
  background: white;
  padding: 15px;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
  display: flex;
  flex-direction: column;
}
.chart-container h3 {
  margin-top: 0;
  color: #333;
}
.chart-wrapper {
  position: relative;
  flex-grow: 1;
  min-height: 250px;
}
</style>

<script setup>
import { computed } from 'vue'
import { Bar } from 'vue-chartjs'
import {
  Chart as ChartJS,
  Title,
  Tooltip,
  Legend,
  BarElement,
  CategoryScale,
  LinearScale
} from 'chart.js'

ChartJS.register(CategoryScale, LinearScale, BarElement, Title, Tooltip, Legend)

const props = defineProps({
  counts: {
    type: Object,
    default: () => ({ 'Li-ion': 0, 'Alkaline': 0, 'NiMH': 0 })
  }
})

const chartData = computed(() => ({
  labels: ['Li-ion', 'Alkaline', 'NiMH'],
  datasets: [
    {
      label: 'Sorted Batteries Count',
      backgroundColor: ['#f87979', '#36a2eb', '#ffcd56'],
      data: [
        props.counts['Li-ion'] || 0,
        props.counts['Alkaline'] || 0,
        props.counts['NiMH'] || 0
      ]
    }
  ]
}))

const chartOptions = {
  responsive: true,
  maintainAspectRatio: false,
  scales: {
    y: {
      beginAtZero: true,
      ticks: {
        stepSize: 1
      },
      title: {
        display: true,
        text: 'Count'
      }
    }
  }
}
</script>

<template>
  <div class="chart-container">
    <h3>Battery Chemistry Counts</h3>
    <div class="chart-wrapper">
      <Bar :data="chartData" :options="chartOptions" />
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

<template>
  <div>
    <canvas ref="canvas" width="200" height="100"></canvas>
  </div>
</template>

<script>
import { Chart } from 'chart.js/auto';

export default {
  props: ["readonly", "ikey", "getData", "putData"],
  data() {
    return {
      chart: null,
      alwaysOnData: this.alwaysOn, // Constant always on consumption data
      actualData: this.actualData
    };
  },
  methods: {
    updateChart(alwaysOn, actual) {
      if (this.chart) {
        this.chart.data.datasets[0].data = alwaysOn;
        this.chart.data.datasets[1].data = actual;
        this.chart.update();
      } else {
        this.initializeChart(alwaysOn, actual);
      }
    },
    initializeChart(alwaysOn, actual) {
      const ctx = this.$refs.canvas.getContext('2d');
      this.chart = new Chart(ctx, {
        type: 'line',
        data: {
          labels: Array.from({ length: 24 }, (_, i) => i + 1),
          datasets: [
            {
              label: 'Always On Consumption',
              data: alwaysOn,
              borderColor: 'rgb(255, 99, 132)',
              tension: 0.1
            },
            {
              label: 'Actual Consumption',
              data: actual,
              borderColor: 'rgb(54, 162, 235)',
              tension: 0.1
            }
          ]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          scales: {
            x: {
              title: {
                display: true,
                text: 'Time (hours)'
              }
            },
            y: {
              title: {
                display: true,
                text: 'Consumption (kWh)'
              }
            }
          }
        }
      });
    }
  },
  mounted() {
    const consumption = this.getData(this.ikey) || { alwaysOn: this.alwaysOnData, actual: this.actualData };
    this.updateChart(consumption.alwaysOn, consumption.actual);
  },
  watch: {
    getData: {
      handler(newData) {
        if (newData) {
          const consumption = newData(this.ikey) || { alwaysOn: this.alwaysOnData, actual: this.actualData };
          this.updateChart(consumption.alwaysOn, consumption.actual);
        }
      },
      deep: true
    }
  }
}
</script>

<style scoped>
canvas {
  width: 100% !important;
  height: auto !important;
}
</style>

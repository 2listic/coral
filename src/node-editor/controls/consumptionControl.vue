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
      alwaysOnData: [],
      actualData: [],
      days: []
    };
  },
  methods: {
    updateChart(alwaysOn, actual, days) {
      if (this.chart) {
        this.chart.data.labels = days;
        this.chart.data.datasets[0].data = alwaysOn;
        this.chart.data.datasets[1].data = actual;
        this.chart.update();
      } else {
        this.initializeChart(alwaysOn, actual, days);
      }
    },
    initializeChart(alwaysOn, actual, days) {
      const ctx = this.$refs.canvas.getContext('2d');
      this.chart = new Chart(ctx, {
        type: 'line',
        data: {
          labels: days,
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
                text: 'Days',
                color: '#ffffff',
                font: {
                  size: 16,
                  weight: 'bold'
                }
              },
              ticks: {
                color: '#ffffff',
                font: {
                  size: 14,
                  weight: 'bold'
                }
              }
            },
            y: {
              title: {
                display: true,
                text: 'Consumption (kWh)',
                color: '#ffffff',
                font: {
                  size: 16,
                  weight: 'bold'
                }
              },
              ticks: {
                color: '#ffffff',
                font: {
                  size: 14,
                  weight: 'bold'
                }
              }
            }
          },
          plugins: {
            legend: {
              labels: {
                color: '#ffffff',
                font: {
                  size: 14,
                  weight: 'bold'
                }
              }
            }
          }
        }
      });
    }
  },
  mounted() {
    const consumption = this.getData(this.ikey) || { alwaysOn: this.alwaysOnData, actual: this.actualData, days: this.days };
    this.updateChart(consumption.alwaysOn, consumption.actual, consumption.days);
  },
  watch: {
    getData: {
      handler(newData) {
        if (newData) {
          const consumption = newData(this.ikey) || { alwaysOn: this.alwaysOnData, actual: this.actualData, days: this.days };
          this.updateChart(consumption.alwaysOn, consumption.actual, consumption.days);
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

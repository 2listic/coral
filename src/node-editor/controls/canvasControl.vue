<template>
    <div>
      <canvas ref="canvas" width="300" height="300" style="border:1px solid #000000;"></canvas>
    </div>
  </template>
  
  <script>
  export default {
    props: ['readonly'],
    mounted() {
      this.$emit('mounted', this.$refs.canvas);
    }
  }
  </script>
  
  <style>
  </style>
  
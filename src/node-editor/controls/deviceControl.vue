<template>
  <div :style="{ backgroundColor: status ? 'green' : 'red', color: 'white', padding: '1px', textAlign: 'center' }">
    <img :src="imageSrc" alt="Device Image" style="width: 50px; height: 50px;">
    <div>{{ status ? 'ON' : 'OFF' }}</div>
  </div>
</template>

<script>
export default {
  props: ['readonly', 'ikey', 'getData', 'emitter', 'putData'],
  data() {
    return {
      status: this.status,
      imageSrc: 'cooler.jpeg'
    };
  },
  mounted() {
    this.status = this.getData(this.ikey) || false;
    this.$watch(
      () => this.getData(this.ikey),
      (value) => {
        this.status = value;
      }
    );
  }
};
</script>

<style scoped>
</style>

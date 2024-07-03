<template>
  <div :style="{ backgroundColor: currentStatus ? 'green' : 'red', color: 'white', padding: '1px', textAlign: 'center' }">
    <img :src="imageSrc" alt="Device Image" style="width: 100px; height: 100px;">
    <div>{{ currentStatus ? 'ON' : 'OFF' }}</div>
  </div>
</template>

<script>
export default {
  props: ['readonly', 'ikey', 'getData', 'putData', 'status'],
  data() {
    return {
      imageSrc: 'cooler.jpg', 
      currentStatus: this.status
    };
  },
  watch: {
    status(newVal) {
      this.currentStatus = newVal;
    }
  },
  mounted() {
    // If you want to dynamically set the image source
    const storedSrc = this.getData(this.ikey);
    if (storedSrc) {
      this.imageSrc = storedSrc;
    }
  },
  methods: {
    setImageSrc(src) {
      this.imageSrc = src;
      this.putData(this.ikey, src);
    }
  }
};
</script>

<style scoped>
.image-control img {
  display: block;
  margin: 0 auto;
}
</style>

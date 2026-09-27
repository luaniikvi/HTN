<template>
  <div id="app-root">
    <router-view />
    <ToastContainer />
    <ConfirmDialog />
  </div>
</template>

<script setup>
import ToastContainer from './components/ui/ToastContainer.vue';
import ConfirmDialog from './components/ui/ConfirmDialog.vue';
</script>

<style>
#app-root {
  min-height: 100vh;
}
</style>

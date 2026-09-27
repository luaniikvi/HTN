import { createApp } from 'vue';
import { createPinia } from 'pinia';
import App from './App.vue';
import router from './router';
import './assets/main.css';
import { useNotifyStore } from './stores/notify';

const app = createApp(App);
const pinia = createPinia();

app.use(pinia);
app.use(router);

app.mount('#app');

// Intercept native window.alert so no ugly browser dialogs appear
const notify = useNotifyStore(pinia);
window.alert = (message) => {
  const text = String(message || '');
  if (text.toLowerCase().includes('offline') || text.toLowerCase().includes('warning') || text.toLowerCase().includes('cannot')) {
    notify.warning(text, 'Device Notice');
  } else if (text.toLowerCase().includes('error') || text.toLowerCase().includes('failed') || text.toLowerCase().includes('lỗi')) {
    notify.error(text, 'Error');
  } else if (text.toLowerCase().includes('success') || text.toLowerCase().includes('saved') || text.toLowerCase().includes('thành công')) {
    notify.success(text, 'Success');
  } else {
    notify.info(text, 'Notification');
  }
};


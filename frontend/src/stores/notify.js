import { defineStore } from 'pinia';
import { ref } from 'vue';

export const useNotifyStore = defineStore('notify', () => {
  const toasts = ref([]);

  const confirmState = ref({
    isOpen: false,
    title: '',
    message: '',
    confirmText: 'Confirm',
    cancelText: 'Cancel',
    type: 'danger', // 'danger' | 'warning' | 'info'
    resolve: null
  });

  let nextId = 1;

  function showToast({ title, message, type = 'info', duration = 4000 }) {
    const id = nextId++;
    const toast = { id, title, message, type, duration, createdAt: Date.now() };
    toasts.value.push(toast);

    if (duration > 0) {
      setTimeout(() => {
        dismissToast(id);
      }, duration);
    }
    return id;
  }

  function warning(message, title = 'Device Warning') {
    return showToast({ title, message, type: 'warning', duration: 4500 });
  }

  function error(message, title = 'Operation Failed') {
    return showToast({ title, message, type: 'error', duration: 5000 });
  }

  function success(message, title = 'Success') {
    return showToast({ title, message, type: 'success', duration: 3500 });
  }

  function info(message, title = 'Notice') {
    return showToast({ title, message, type: 'info', duration: 4000 });
  }

  function dismissToast(id) {
    toasts.value = toasts.value.filter(t => t.id !== id);
  }

  /**
   * Promisified Custom Confirmation Dialog
   * @param {Object} options
   * @returns {Promise<boolean>}
   */
  function confirm({
    title = 'Confirm Action',
    message = 'Are you sure you want to proceed?',
    confirmText = 'Confirm',
    cancelText = 'Cancel',
    type = 'danger'
  }) {
    return new Promise((resolve) => {
      confirmState.value = {
        isOpen: true,
        title,
        message,
        confirmText,
        cancelText,
        type,
        resolve: (val) => {
          confirmState.value.isOpen = false;
          resolve(val);
        }
      };
    });
  }

  function handleConfirmResponse(choice) {
    if (confirmState.value.resolve) {
      confirmState.value.resolve(choice);
    }
    confirmState.value.isOpen = false;
  }

  return {
    toasts,
    confirmState,
    showToast,
    warning,
    error,
    success,
    info,
    dismissToast,
    confirm,
    handleConfirmResponse
  };
});

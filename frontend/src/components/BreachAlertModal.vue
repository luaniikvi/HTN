<template>
  <div v-if="alert" class="modal-backdrop">
    <div class="glass-card breach-modal pulsing-alarm">
      <div class="breach-header">
        <div class="alert-icon-wrapper">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class="alert-icon">
            <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"></path>
            <line x1="12" y1="9" x2="12" y2="13"></line>
            <line x1="12" y1="17" x2="12.01" y2="17"></line>
          </svg>
        </div>
        <div>
          <span class="badge badge-armed">CẢNH BÁO ĐỘT NHẬP THỜI GIAN THỰC</span>
          <h2>PHÁT HIỆN VI PHẠM MỞ CỬA!</h2>
        </div>
      </div>

      <div class="breach-body">
        <p class="breach-desc">
          Cảm biến cửa MC-38 đã phát hiện <strong>CỬA BỊ MỞ</strong> khi hệ thống đang ở chế độ bảo vệ
          <span class="badge" :class="alert.mode === 'ARMED' ? 'badge-armed' : 'badge-stay'">{{ alert.mode }}</span>
          mà chưa được xác thực khuôn mặt hợp lệ!
        </p>

        <div class="breach-info font-mono">
          <div><span>Thiết bị:</span> <strong>{{ alert.deviceId || 'dev_01' }}</strong></div>
          <div><span>Thời điểm:</span> <strong>{{ formatTime(alert.timestamp) }}</strong></div>
          <div>
            <span>Trạng thái còi:</span>
            <strong :class="alert.mode === 'ARMED' ? 'text-danger' : 'text-warning'">
              {{ alert.mode === 'ARMED' ? 'BUZZER_ON (Đang hú công suất tối đa)' : 'BUZZER_BEEP (Báo động tại chỗ - beep ~2s)' }}
            </strong>
          </div>
        </div>
      </div>

      <div class="breach-actions">
        <!-- Nút chuyển DISARMED -->
        <button class="btn btn-primary" @click="handleDisarm" :disabled="isProcessing">
          CHUYỂN SANG DISARMED (MỞ CỬA TỰ DO)
        </button>

        <!-- Nút ngắt còi -->
        <button class="btn btn-danger" @click="handleSilenceAlarm" :disabled="isProcessing">
          TẮT CÒI BÁO ĐỘNG (SILENCE ALARM)
        </button>

        <!-- Nút đóng -->
        <button class="btn btn-secondary" @click="system.dismissBreachAlert">
          Đóng thông báo
        </button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, ref } from 'vue';
import { useSystemStore } from '../stores/system';

const system = useSystemStore();
const alert = computed(() => system.activeBreachAlert);
const isProcessing = ref(false);

async function handleDisarm() {
  isProcessing.value = true;
  try {
    await system.changeMode('DISARMED');
  } catch (err) {
    alert('Lỗi: ' + err.message);
  } finally {
    isProcessing.value = false;
  }
}

async function handleSilenceAlarm() {
  isProcessing.value = true;
  try {
    await system.toggleAlarm(false);
  } catch (err) {
    alert('Lỗi: ' + err.message);
  } finally {
    isProcessing.value = false;
  }
}

function formatTime(iso) {
  if (!iso) return 'Vừa xong';
  return new Date(iso).toLocaleTimeString('vi-VN');
}
</script>

<style scoped>
.modal-backdrop {
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.85);
  backdrop-filter: blur(10px);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 2000;
  padding: 20px;
}

.breach-modal {
  width: 100%;
  max-width: 540px;
  background: #181116;
  border: 2px solid #ef4444;
  border-radius: 14px;
  padding: 24px;
  display: flex;
  flex-direction: column;
  gap: 20px;
}

.breach-header {
  display: flex;
  align-items: center;
  gap: 16px;
}

.alert-icon-wrapper {
  width: 48px;
  height: 48px;
  border-radius: 50%;
  background: rgba(239, 68, 68, 0.2);
  display: flex;
  align-items: center;
  justify-content: center;
}

.alert-icon {
  width: 28px;
  height: 28px;
  color: #ef4444;
}

.breach-header h2 {
  font-size: 1.25rem;
  font-weight: 800;
  color: #ef4444;
  margin-top: 4px;
}

.breach-body {
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.breach-desc {
  font-size: 0.95rem;
  line-height: 1.6;
  color: #e5e7eb;
}

.breach-info {
  background: rgba(0, 0, 0, 0.4);
  padding: 12px 16px;
  border-radius: 8px;
  border: 1px solid rgba(239, 68, 68, 0.3);
  font-size: 0.85rem;
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.text-danger {
  color: #f87171;
}

.text-warning {
  color: #fbbf24;
}

.breach-actions {
  display: flex;
  flex-direction: column;
  gap: 10px;
}
</style>

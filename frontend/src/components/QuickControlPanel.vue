<template>
  <div class="glass-card control-panel">
    <div class="panel-header">
      <h3>BẢNG ĐIỀU KHIỂN AN NINH NHANH</h3>
      <div class="status-tags">
        <span class="badge" :class="system.status === 'ONLINE' ? 'badge-online' : 'badge-offline'">
          {{ system.status }}
        </span>
        <span class="badge" :class="system.doorState === 'OPEN' ? 'badge-armed' : 'badge-disarmed'">
          CỬA: {{ system.doorState }}
        </span>
      </div>
    </div>

    <!-- Security Mode Switcher (3 Chế độ: DISARMED, STAY, ARMED) -->
    <div class="mode-section">
      <label class="section-label">CHẾ ĐỘ BẢO VỆ (SECURITY MODE):</label>
      <div class="mode-buttons-grid">
        <!-- DISARMED -->
        <button
          class="mode-btn btn-disarmed"
          :class="{ active: system.securityMode === 'DISARMED' }"
          @click="selectMode('DISARMED')"
          :disabled="isSubmitting"
        >
          <div class="btn-icon">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect>
              <path d="M7 11V7a5 5 0 0 1 9.9-1"></path>
            </svg>
          </div>
          <div class="btn-content">
            <span class="btn-title">DISARMED</span>
            <span class="btn-desc">Mở cửa tự do, còi tắt</span>
          </div>
        </button>

        <!-- STAY -->
        <button
          class="mode-btn btn-stay"
          :class="{ active: system.securityMode === 'STAY' }"
          @click="selectMode('STAY')"
          :disabled="isSubmitting"
        >
          <div class="btn-icon">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"></path>
              <polyline points="9 22 9 12 15 12 15 22"></polyline>
            </svg>
          </div>
          <div class="btn-content">
            <span class="btn-title">STAY</span>
            <span class="btn-desc">Báo động tại chỗ (beep ~2s)</span>
          </div>
        </button>

        <!-- ARMED -->
        <button
          class="mode-btn btn-armed"
          :class="{ active: system.securityMode === 'ARMED' }"
          @click="selectMode('ARMED')"
          :disabled="isSubmitting"
        >
          <div class="btn-icon">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect>
              <path d="M7 11V7a5 5 0 0 1 10 0v4"></path>
            </svg>
          </div>
          <div class="btn-content">
            <span class="btn-title">ARMED</span>
            <span class="btn-desc">Cảnh báo toàn phần (còi hú cực đại)</span>
          </div>
        </button>
      </div>
    </div>

    <!-- Quick Actions: Panic Siren & Register Face -->
    <div class="action-section">
      <!-- Panic Siren Button -->
      <button
        class="btn panic-btn"
        :class="{ 'pulsing-alarm': isAlarmActive }"
        @click="handleToggleAlarm"
        :disabled="isSubmitting"
      >
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class="panic-icon">
          <polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"></polygon>
          <path d="M19.07 4.93a10 10 0 0 1 0 14.14M15.54 8.46a5 5 0 0 1 0 7.07"></path>
        </svg>
        <span>{{ isAlarmActive ? 'TẮT CÒI BÁO ĐỘNG (SILENCE ALARM)' : 'BẬT CÒI KHẨN CẤP (PANIC SIREN)' }}</span>
      </button>

      <!-- Register Face Wizard Trigger -->
      <button class="btn btn-primary register-btn" @click="$emit('open-enroll')">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="width: 18px; height: 18px;">
          <path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"></path>
          <circle cx="12" cy="7" r="4"></circle>
          <line x1="19" y1="8" x2="19" y2="14"></line>
          <line x1="22" y1="11" x2="16" y2="11"></line>
        </svg>
        <span>ĐĂNG KÝ KHUÔN MẶT MỚI (3 BƯỚC)</span>
      </button>
    </div>

    <!-- Grace Period Setting -->
    <div class="config-bar">
      <span class="config-title">Thời gian ân hạn mở cửa (Grace Timeout):</span>
      <div class="config-input-group">
        <input
          type="number"
          min="3"
          max="60"
          v-model.number="graceInput"
          class="input-control grace-input font-mono"
        />
        <span class="unit">giây</span>
        <button class="btn btn-secondary btn-sm" @click="saveGracePeriod" :disabled="isSubmitting">
          Lưu
        </button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useSystemStore } from '../stores/system';

const system = useSystemStore();
const isSubmitting = ref(false);
const graceInput = ref(system.gracePeriod || 10);

defineEmits(['open-enroll']);

const isAlarmActive = computed(() => {
  if (system.securityMode === 'DISARMED') return false;
  if (system.securityMode === 'STAY') {
    return Boolean(system.forcedAlarm || (system.doorState === 'OPEN' && (system.armedLatched || system.activeBreachAlert)));
  }
  return Boolean(system.forcedAlarm || system.armedLatched);
});

async function selectMode(mode) {
  if (system.securityMode === mode) return;
  isSubmitting.value = true;
  try {
    await system.changeMode(mode);
  } catch (err) {
    alert('Không thể đổi chế độ: ' + (err.response?.data?.message || err.message));
  } finally {
    isSubmitting.value = false;
  }
}

async function handleToggleAlarm() {
  isSubmitting.value = true;
  try {
    const nextState = !isAlarmActive.value;
    await system.toggleAlarm(nextState);
  } catch (err) {
    alert('Thao tác còi thất bại: ' + (err.response?.data?.message || err.message));
  } finally {
    isSubmitting.value = false;
  }
}

async function saveGracePeriod() {
  if (graceInput.value < 3 || graceInput.value > 60) {
    alert('Thời gian ân hạn phải từ 3 đến 60 giây');
    return;
  }
  isSubmitting.value = true;
  try {
    await system.updateGracePeriod(graceInput.value);
    alert('Đã cập nhật thời gian ân hạn mở cửa thành công!');
  } catch (err) {
    alert('Lỗi cập nhật: ' + err.message);
  } finally {
    isSubmitting.value = false;
  }
}
</script>

<style scoped>
.control-panel {
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 10px;
}

.panel-header h3 {
  font-size: 0.95rem;
  font-weight: 700;
  letter-spacing: 0.05em;
}

.status-tags {
  display: flex;
  gap: 8px;
}

.section-label {
  font-size: 0.75rem;
  font-weight: 600;
  color: var(--text-muted);
  letter-spacing: 0.05em;
  margin-bottom: 8px;
  display: block;
}

.mode-buttons-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 12px;
}

.mode-btn {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 12px 14px;
  background: rgba(17, 24, 39, 0.6);
  border: 1px solid var(--border-color);
  border-radius: 10px;
  cursor: pointer;
  transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
  text-align: left;
}

.mode-btn .btn-icon {
  width: 28px;
  height: 28px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.mode-btn svg {
  width: 22px;
  height: 22px;
}

.btn-content {
  display: flex;
  flex-direction: column;
}

.btn-title {
  font-size: 0.85rem;
  font-weight: 700;
  letter-spacing: 0.03em;
}

.btn-desc {
  font-size: 0.7rem;
  color: var(--text-muted);
  margin-top: 2px;
}

/* Colors for Modes */
.btn-disarmed svg { color: var(--color-disarmed); }
.btn-stay svg { color: var(--color-stay); }
.btn-armed svg { color: var(--color-armed); }

.btn-disarmed.active {
  background: rgba(16, 185, 129, 0.15);
  border-color: var(--color-disarmed);
  box-shadow: 0 0 15px rgba(16, 185, 129, 0.2);
}

.btn-stay.active {
  background: rgba(245, 158, 11, 0.15);
  border-color: var(--color-stay);
  box-shadow: 0 0 15px rgba(245, 158, 11, 0.2);
}

.btn-armed.active {
  background: rgba(239, 68, 68, 0.15);
  border-color: var(--color-armed);
  box-shadow: 0 0 15px rgba(239, 68, 68, 0.2);
}

/* Action Section */
.action-section {
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
}

.panic-btn {
  flex: 1;
  min-width: 220px;
  background: linear-gradient(135deg, #dc2626 0%, #991b1b 100%);
  color: white;
  padding: 12px 16px;
}

.panic-icon {
  width: 20px;
  height: 20px;
}

.register-btn {
  flex: 1;
  min-width: 220px;
  padding: 12px 16px;
}

/* Grace Period Config Bar */
.config-bar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 10px 14px;
  background: rgba(31, 41, 55, 0.4);
  border-radius: 8px;
  border: 1px solid rgba(75, 85, 99, 0.3);
  font-size: 0.8rem;
  flex-wrap: wrap;
  gap: 10px;
}

.config-input-group {
  display: flex;
  align-items: center;
  gap: 8px;
}

.grace-input {
  width: 70px;
  padding: 4px 8px;
  text-align: center;
}

.unit {
  color: var(--text-muted);
}

.btn-sm {
  padding: 4px 10px;
  font-size: 0.75rem;
}

@media (max-width: 768px) {
  .mode-buttons-grid {
    grid-template-columns: 1fr;
  }
}
</style>

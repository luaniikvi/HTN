<template>
  <div class="modal-backdrop" @click.self="handleClose">
    <div class="glass-card modal-content">
      <div class="modal-header">
        <div class="modal-title">
          <span class="step-badge font-mono">ENROLLMENT WIZARD</span>
          <h3>QUY TRÌNH NẠP KHUÔN MẶT 3 BƯỚC</h3>
        </div>
        <button class="close-btn" @click="handleClose" title="Đóng / Hủy">&times;</button>
      </div>

      <!-- Step Indicator Bar -->
      <div class="wizard-steps">
        <div class="step-item" :class="{ active: currentStep === 1, done: currentStep > 1 }">
          <div class="step-circle">1</div>
          <span>Góc thẳng (1 beep)</span>
        </div>
        <div class="step-line" :class="{ active: currentStep > 1 }"></div>

        <div class="step-item" :class="{ active: currentStep === 2, done: currentStep > 2 }">
          <div class="step-circle">2</div>
          <span>Nghiêng 1 (2 beeps)</span>
        </div>
        <div class="step-line" :class="{ active: currentStep > 2 }"></div>

        <div class="step-item" :class="{ active: currentStep === 3, done: currentStep > 3 }">
          <div class="step-circle">3</div>
          <span>Nghiêng 2 (3 beeps)</span>
        </div>
        <div class="step-line" :class="{ active: currentStep >= 4 }"></div>

        <div class="step-item" :class="{ active: currentStep === 4, done: isCompleted }">
          <div class="step-circle">✓</div>
          <span>Lưu NVS Flash</span>
        </div>
      </div>

      <!-- Phase 1: Initiation -->
      <div v-if="currentStep === 0" class="step-body-init">
        <div class="icon-pulse">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" class="camera-icon">
            <path d="M23 19a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h4l2-3h6l2 3h4a2 2 0 0 1 2 2z"></path>
            <circle cx="12" cy="13" r="4"></circle>
          </svg>
        </div>
        <p class="guide-text">
          Hệ thống sẽ chuyển ESP32 sang chế độ thu nhận 3 góc nhìn của khuôn mặt (Trực diện, Nghiêng trái/phải, Góc còn lại).
          Hãy nhìn thẳng vào camera và chuẩn bị lắng nghe tiếng còi "beep" hướng dẫn từ thiết bị.
        </p>
        <button class="btn btn-primary btn-lg" @click="startEnrollment" :disabled="isStarting">
          {{ isStarting ? 'Đang gửi lệnh đến ESP32...' : 'BẮT ĐẦU ĐĂNG KÝ NGAY' }}
        </button>
      </div>

      <!-- Phase 2: Live Capturing (Steps 1 to 3) -->
      <div v-else-if="currentStep >= 1 && currentStep <= 3" class="step-body-capturing">
        <div class="angle-animation font-mono">
          <div v-if="currentStep === 1" class="angle-indicator">
            <span class="angle-icon">🧍</span>
            <h4>BƯỚC 1: NHÌN THẲNG TRỰC DIỆN</h4>
            <p>ESP32 phát 1 tiếng "beep" và trích xuất vector góc thẳng...</p>
          </div>
          <div v-else-if="currentStep === 2" class="angle-indicator">
            <span class="angle-icon">🗣️</span>
            <h4>BƯỚC 2: QUAY NGHIÊNG TRÁI / PHẢI</h4>
            <p>ESP32 phát 2 tiếng "beep" và trích xuất vector góc nghiêng...</p>
          </div>
          <div v-else-if="currentStep === 3" class="angle-indicator">
            <span class="angle-icon">👤</span>
            <h4>BƯỚC 3: QUAY NGHIÊNG GÓC CÒN LẠI</h4>
            <p>ESP32 phát 3 tiếng "beep" và hoàn tất trích xuất vector mẫu...</p>
          </div>
        </div>
        <div class="capturing-spinner">
          <div class="spinner"></div>
          <span>Đang đồng bộ dữ liệu thời gian thực từ MCU Core 1...</span>
        </div>

        <button class="btn btn-danger btn-cancel" @click="cancelEnrollment" :disabled="isCancelling">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class="btn-icon">
            <circle cx="12" cy="12" r="10"></circle>
            <line x1="15" y1="9" x2="9" y2="15"></line>
            <line x1="9" y1="9" x2="15" y2="15"></line>
          </svg>
          {{ isCancelling ? 'Đang gửi lệnh hủy đến ESP32...' : 'HỦY QUÁ TRÌNH ĐĂNG KÝ' }}
        </button>
      </div>

      <!-- Phase 3: Bind Face Form (Step 4) -->
      <div v-else-if="currentStep === 4" class="step-body-form">
        <div class="success-banner">
          <span class="success-icon">🎉</span>
          <div>
            <h4>ĐÃ GHI THÀNH CÔNG VÀO FLASH NVS</h4>
            <p class="font-mono">Face ID cấp mới: <strong>#{{ enrolledFaceId }}</strong></p>
          </div>
        </div>

        <form @submit.prevent="saveFaceMetadata" class="bind-form">
          <div class="form-group">
            <label>Họ và Tên chủ sở hữu khuôn mặt:</label>
            <input
              type="text"
              v-model="faceName"
              placeholder="VD: Nguyễn Văn A"
              required
              class="input-control"
            />
          </div>

          <div class="form-group">
            <label>Phân quyền truy cập:</label>
            <div class="radio-group">
              <label class="radio-label" :class="{ selected: roleType === 'PERMANENT' }">
                <input type="radio" value="PERMANENT" v-model="roleType" />
                <span class="radio-title">Vĩnh viễn (PERMANENT)</span>
                <span class="radio-desc">Hiệu lực không thời hạn</span>
              </label>

              <label class="radio-label" :class="{ selected: roleType === 'TEMPORARY' }">
                <input type="radio" value="TEMPORARY" v-model="roleType" />
                <span class="radio-title">Tạm thời (TEMPORARY)</span>
                <span class="radio-desc">Tự động xóa qua Cron khi hết hạn</span>
              </label>
            </div>
          </div>

          <div v-if="roleType === 'TEMPORARY'" class="form-group">
            <label>Thời điểm hết hạn (Valid Until):</label>
            <input
              type="datetime-local"
              v-model="validUntil"
              required
              class="input-control font-mono"
            />
          </div>

          <div class="form-actions">
            <button type="button" class="btn btn-secondary" @click="handleClose">
              BỎ QUA LƯU TÊN
            </button>
            <button type="submit" class="btn btn-primary" :disabled="isSaving">
              {{ isSaving ? 'Đang lưu CSDL...' : 'HOÀN TẤT & LƯU THÔNG TIN' }}
            </button>
          </div>
        </form>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch } from 'vue';
import { useSystemStore } from '../stores/system';
import api from '../api/client';

const emit = defineEmits(['close', 'face-saved']);
const system = useSystemStore();

const isStarting = ref(false);
const isCancelling = ref(false);
const isSaving = ref(false);
const isCompleted = ref(false);

const faceName = ref('');
const roleType = ref('PERMANENT');
const validUntil = ref('');

const currentStep = computed(() => system.enrollStep);
const enrolledFaceId = computed(() => system.enrolledFaceId);

// Đồng bộ khi hoàn tất nạp mặt
watch(() => system.enrollStep, (step) => {
  if (step === 4) {
    // Mặc định hết hạn sau 24h nếu chọn tạm thời
    const tomorrow = new Date(Date.now() + 24 * 60 * 60 * 1000);
    validUntil.value = tomorrow.toISOString().slice(0, 16);
  }
});

async function startEnrollment() {
  isStarting.value = true;
  try {
    await api.post('/faces/enroll');
    system.enrollStep = 1;
  } catch (err) {
    alert('Không thể bắt đầu nạp mặt: ' + (err.response?.data?.message || err.message));
  } finally {
    isStarting.value = false;
  }
}

async function cancelEnrollment() {
  isCancelling.value = true;
  try {
    await api.post('/faces/cancel-enroll');
  } catch (err) {
    console.warn('Lỗi gửi lệnh hủy:', err);
  } finally {
    isCancelling.value = false;
    system.enrollStep = 0;
    system.enrolledFaceId = null;
    emit('close');
  }
}

function handleClose() {
  if (currentStep.value >= 1 && currentStep.value <= 3) {
    if (confirm('Bạn có chắc chắn muốn HỦY quá trình đăng ký khuôn mặt trên thiết bị?')) {
      cancelEnrollment();
    }
  } else {
    system.enrollStep = 0;
    system.enrolledFaceId = null;
    emit('close');
  }
}

async function saveFaceMetadata() {
  if (!faceName.value.trim()) return;
  isSaving.value = true;
  try {
    await api.post('/faces/bind', {
      face_id: enrolledFaceId.value,
      name: faceName.value.trim(),
      role_type: roleType.value,
      valid_until: roleType.value === 'TEMPORARY' ? validUntil.value : null
    });

    isCompleted.value = true;
    alert('Đã gán quyền và lưu khuôn mặt thành công!');
    system.enrollStep = 0;
    system.enrolledFaceId = null;
    emit('face-saved');
    emit('close');
  } catch (err) {
    alert('Lỗi lưu khuôn mặt: ' + (err.response?.data?.message || err.message));
  } finally {
    isSaving.value = false;
  }
}
</script>

<style scoped>
.modal-backdrop {
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.75);
  backdrop-filter: blur(8px);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
  padding: 20px;
}

.modal-content {
  width: 100%;
  max-width: 620px;
  padding: 24px;
  display: flex;
  flex-direction: column;
  gap: 20px;
  background: #111827;
  border: 1px solid rgba(75, 85, 99, 0.5);
}

.modal-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
}

.step-badge {
  font-size: 0.7rem;
  color: var(--color-primary);
  letter-spacing: 0.1em;
}

.modal-title h3 {
  font-size: 1.1rem;
  font-weight: 700;
  margin-top: 4px;
}

.close-btn {
  background: transparent;
  border: none;
  font-size: 1.5rem;
  color: var(--text-muted);
  cursor: pointer;
  line-height: 1;
}

.close-btn:hover {
  color: #ffffff;
}

/* Wizard Steps Indicator */
.wizard-steps {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 10px 0;
}

.step-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 6px;
  font-size: 0.7rem;
  color: var(--text-muted);
  text-align: center;
  max-width: 80px;
}

.step-circle {
  width: 32px;
  height: 32px;
  border-radius: 50%;
  background: #1f2937;
  border: 2px solid #374151;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: 700;
  color: #9ca3af;
  transition: all 0.3s ease;
}

.step-item.active .step-circle {
  background: var(--color-primary);
  border-color: #60a5fa;
  color: white;
  box-shadow: 0 0 12px rgba(59, 130, 246, 0.5);
}

.step-item.done .step-circle {
  background: var(--color-disarmed);
  border-color: #34d399;
  color: white;
}

.step-line {
  flex: 1;
  height: 2px;
  background: #374151;
  margin: 0 8px;
  margin-bottom: 22px;
  transition: background 0.3s ease;
}

.step-line.active {
  background: var(--color-primary);
}

/* Step Bodies */
.step-body-init {
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  gap: 16px;
  padding: 20px 0;
}

.camera-icon {
  width: 64px;
  height: 64px;
  color: var(--color-primary);
}

.guide-text {
  font-size: 0.9rem;
  color: #d1d5db;
  max-width: 480px;
  line-height: 1.6;
}

.btn-lg {
  padding: 12px 24px;
  font-size: 0.95rem;
  width: 100%;
}

/* Capturing */
.step-body-capturing {
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  gap: 24px;
  padding: 30px 0;
}

.angle-icon {
  font-size: 3rem;
  display: block;
  margin-bottom: 10px;
}

.angle-indicator h4 {
  font-size: 1.1rem;
  font-weight: 700;
  color: #60a5fa;
  letter-spacing: 0.05em;
}

.angle-indicator p {
  font-size: 0.85rem;
  color: #9ca3af;
  margin-top: 6px;
}

.capturing-spinner {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 0.8rem;
  color: #9ca3af;
}

.spinner {
  width: 18px;
  height: 18px;
  border: 2px solid rgba(255, 255, 255, 0.2);
  border-top-color: var(--color-primary);
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

/* Form */
.step-body-form {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.success-banner {
  display: flex;
  align-items: center;
  gap: 14px;
  padding: 12px 16px;
  background: rgba(16, 185, 129, 0.15);
  border: 1px solid rgba(16, 185, 129, 0.4);
  border-radius: 8px;
  color: #34d399;
}

.success-icon {
  font-size: 1.8rem;
}

.bind-form {
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.form-group {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.form-group label {
  font-size: 0.8rem;
  font-weight: 600;
  color: #d1d5db;
}

.radio-group {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 10px;
}

.radio-label {
  display: flex;
  flex-direction: column;
  padding: 10px 14px;
  background: rgba(31, 41, 55, 0.5);
  border: 1px solid var(--border-color);
  border-radius: 8px;
  cursor: pointer;
}

.radio-label.selected {
  border-color: var(--color-primary);
  background: rgba(59, 130, 246, 0.15);
}

.radio-label input {
  display: none;
}

.radio-title {
  font-size: 0.85rem;
  font-weight: 700;
}

.radio-desc {
  font-size: 0.7rem;
  color: var(--text-muted);
  margin-top: 2px;
}

.btn-cancel {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  padding: 10px 20px;
  font-size: 0.85rem;
  font-weight: 700;
  border-radius: 8px;
  background: rgba(239, 68, 68, 0.2);
  border: 1px solid rgba(239, 68, 68, 0.5);
  color: #f87171;
  transition: all 0.2s ease;
  cursor: pointer;
  margin-top: 10px;
}

.btn-cancel:hover:not(:disabled) {
  background: rgba(239, 68, 68, 0.35);
  border-color: #ef4444;
  color: #ffffff;
  box-shadow: 0 0 12px rgba(239, 68, 68, 0.4);
}

.btn-icon {
  width: 18px;
  height: 18px;
}

.form-actions {
  display: flex;
  gap: 12px;
  justify-content: flex-end;
  margin-top: 10px;
}
</style>

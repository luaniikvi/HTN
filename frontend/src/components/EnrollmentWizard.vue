<template>
  <div class="modal-backdrop" @click.self="handleClose">
    <div ref="modalCardRef" class="clean-card modal-content">
      <!-- Modal Header -->
      <div class="modal-header">
        <div class="modal-title-group">
          <span class="step-badge font-mono">BIOMETRIC ENROLLMENT WIZARD</span>
          <h3 class="modal-heading">3-Angle Face Registration</h3>
        </div>
        <button class="btn-close" @click="handleClose" title="Cancel Enrollment">
          <X :size="20" />
        </button>
      </div>

      <!-- Step Indicator Bar -->
      <div class="wizard-steps font-mono">
        <div class="step-item" :class="{ active: currentStep === 1, done: currentStep > 1 }">
          <div class="step-circle">1</div>
          <span class="step-name">Frontal (1 beep)</span>
        </div>
        <div class="step-line" :class="{ active: currentStep > 1 }"></div>

        <div class="step-item" :class="{ active: currentStep === 2, done: currentStep > 2 }">
          <div class="step-circle">2</div>
          <span class="step-name">Tilt 1 (2 beeps)</span>
        </div>
        <div class="step-line" :class="{ active: currentStep > 2 }"></div>

        <div class="step-item" :class="{ active: currentStep === 3, done: currentStep > 3 }">
          <div class="step-circle">3</div>
          <span class="step-name">Tilt 2 (3 beeps)</span>
        </div>
        <div class="step-line" :class="{ active: currentStep >= 4 }"></div>

        <div class="step-item" :class="{ active: currentStep === 4, done: isCompleted }">
          <div class="step-circle">
            <Check :size="14" />
          </div>
          <span class="step-name">Store NVS</span>
        </div>
      </div>

      <!-- Phase 1: Initiation (Step 0) -->
      <div v-if="currentStep === 0" class="step-body-init">
        <div class="icon-circle-pulse">
          <Camera :size="36" class="camera-icon" />
        </div>
        <div class="init-text-box">
          <h4>Prepare for Camera Acquisition</h4>
          <p class="guide-text">
            The ESP32-S3 will capture three consecutive facial orientations: frontal view, slight tilt left/right, and opposite angle.
            Face directly towards the OV5640 sensor and listen for device buzzer beeps.
          </p>
        </div>
        <button class="btn btn-primary btn-lg" @click="startEnrollment" :disabled="isStarting">
          <span>{{ isStarting ? 'Sending Command to ESP32...' : 'Start Enrollment Process' }}</span>
        </button>
      </div>

      <!-- Phase 2: Live Capturing (Steps 1 to 3) -->
      <div v-else-if="currentStep >= 1 && currentStep <= 3" class="step-body-capturing">
        <div class="angle-animation font-mono">
          <div v-if="currentStep === 1" class="angle-box">
            <UserCheck :size="48" class="angle-symbol blue" />
            <h4 class="angle-title">STEP 1: LOOK DIRECTLY AT CAMERA</h4>
            <p class="angle-desc">ESP32 emits 1 beep and extracts frontal vector embedding...</p>
          </div>
          <div v-else-if="currentStep === 2" class="angle-box">
            <Smile :size="48" class="angle-symbol amber" />
            <h4 class="angle-title">STEP 2: SLIGHTLY TILT HEAD (ANGLE 1)</h4>
            <p class="angle-desc">ESP32 emits 2 beeps and samples secondary vector perspective...</p>
          </div>
          <div v-else-if="currentStep === 3" class="angle-box">
            <Smile :size="48" class="angle-symbol emerald" />
            <h4 class="angle-title">STEP 3: TILT HEAD (ANGLE 2)</h4>
            <p class="angle-desc">ESP32 emits 3 beeps and compiles final composite template...</p>
          </div>
        </div>

        <div class="capturing-status font-mono">
          <div class="mini-spinner"></div>
          <span>Synchronizing real-time telemetry from MCU Core 1...</span>
        </div>

        <button class="btn btn-secondary btn-cancel" @click="cancelEnrollment" :disabled="isCancelling">
          <XCircle :size="16" />
          <span>{{ isCancelling ? 'Cancelling on Device...' : 'Cancel Enrollment' }}</span>
        </button>
      </div>

      <!-- Phase 3: Bind Face Form (Step 4) -->
      <div v-else-if="currentStep === 4" class="step-body-form">
        <div class="success-banner">
          <CheckCircle2 :size="24" class="success-icon" />
          <div>
            <h4>Biometric Template Committed to Flash NVS</h4>
            <p class="font-mono text-xs">Assigned Hardware ID: <strong>#{{ enrolledFaceId }}</strong></p>
          </div>
        </div>

        <form @submit.prevent="saveFaceMetadata" class="bind-form">
          <div class="form-group">
            <label class="form-label">Full Name / Subject Label:</label>
            <input
              type="text"
              v-model="faceName"
              placeholder="e.g. John Doe, Operations Team"
              required
              class="input-control"
              autofocus
            />
          </div>

          <div class="form-group">
            <label class="form-label">Access Tier:</label>
            <div class="radio-card-grid">
              <label class="radio-card" :class="{ selected: roleType === 'PERMANENT' }">
                <input type="radio" value="PERMANENT" v-model="roleType" />
                <span class="radio-title">Permanent Access</span>
                <span class="radio-desc">Uncapped validity; resident or core operator</span>
              </label>

              <label class="radio-card" :class="{ selected: roleType === 'TEMPORARY' }">
                <input type="radio" value="TEMPORARY" v-model="roleType" />
                <span class="radio-title">Temporary Guest</span>
                <span class="radio-desc">Auto-purged via background cron upon expiry</span>
              </label>
            </div>
          </div>

          <div v-if="roleType === 'TEMPORARY'" class="form-group">
            <label class="form-label">Expiration Date & Time:</label>
            <input
              type="datetime-local"
              v-model="validUntil"
              required
              class="input-control font-mono"
            />
          </div>

          <div class="form-actions">
            <button type="button" class="btn btn-secondary" @click="handleClose">
              Skip Label
            </button>
            <button type="submit" class="btn btn-primary" :disabled="isSaving">
              <span>{{ isSaving ? 'Saving to Database...' : 'Complete & Save Profile' }}</span>
            </button>
          </div>
        </form>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch, onMounted } from 'vue';
import { useSystemStore } from '../stores/system';
import api from '../api/client';
import gsap from 'gsap';
import {
  X,
  Check,
  Camera,
  UserCheck,
  Smile,
  XCircle,
  CheckCircle2
} from 'lucide-vue-next';

const emit = defineEmits(['close', 'face-saved']);
const system = useSystemStore();

const modalCardRef = ref(null);
const isStarting = ref(false);
const isCancelling = ref(false);
const isSaving = ref(false);
const isCompleted = ref(false);

const faceName = ref('');
const roleType = ref('PERMANENT');
const validUntil = ref('');

const currentStep = computed(() => system.enrollStep);
const enrolledFaceId = computed(() => system.enrolledFaceId);

watch(() => system.enrollStep, (step) => {
  if (step === 4) {
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
    alert('Failed to initiate enrollment: ' + (err.response?.data?.message || err.message));
  } finally {
    isStarting.value = false;
  }
}

async function cancelEnrollment() {
  isCancelling.value = true;
  try {
    await api.post('/faces/cancel-enroll');
  } catch (err) {
    console.warn('Error cancelling enrollment:', err);
  } finally {
    isCancelling.value = false;
    system.enrollStep = 0;
    system.enrolledFaceId = null;
    emit('close');
  }
}

function handleClose() {
  if (currentStep.value >= 1 && currentStep.value <= 3) {
    if (confirm('Cancel ongoing face enrollment on the hardware device?')) {
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
    alert('Face ID profile successfully bound and saved!');
    system.enrollStep = 0;
    system.enrolledFaceId = null;
    emit('face-saved');
    emit('close');
  } catch (err) {
    alert('Save error: ' + (err.response?.data?.message || err.message));
  } finally {
    isSaving.value = false;
  }
}

onMounted(() => {
  if (modalCardRef.value) {
    gsap.fromTo(
      modalCardRef.value,
      { scale: 0.94, opacity: 0, y: 15 },
      { scale: 1, opacity: 1, y: 0, duration: 0.28, ease: 'back.out(1.4)' }
    );
  }
});
</script>

<style scoped>
.modal-backdrop {
  position: fixed;
  inset: 0;
  background: rgba(15, 23, 42, 0.6);
  backdrop-filter: blur(8px);
  -webkit-backdrop-filter: blur(8px);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
  padding: 20px;
}

.modal-content {
  width: 100%;
  max-width: 600px;
  padding: 24px;
  display: flex;
  flex-direction: column;
  gap: 20px;
  background: #ffffff;
  box-shadow: var(--shadow-modal);
}

.modal-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
}

.step-badge {
  font-size: 0.68rem;
  font-weight: 700;
  color: var(--color-primary);
  letter-spacing: 0.05em;
}

.modal-heading {
  font-size: 1.15rem;
  font-weight: 800;
  color: var(--text-main);
  margin-top: 2px;
}

.btn-close {
  background: transparent;
  border: none;
  cursor: pointer;
  color: var(--text-muted);
  padding: 4px;
  border-radius: var(--radius-sm);
}

.btn-close:hover {
  color: var(--text-main);
  background: var(--bg-subtle);
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
  max-width: 90px;
}

.step-circle {
  width: 32px;
  height: 32px;
  border-radius: 50%;
  background: var(--bg-subtle);
  border: 1.5px solid var(--border-subtle);
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: 700;
  color: var(--text-secondary);
  transition: all 0.25s ease;
}

.step-name {
  font-size: 0.68rem;
  font-weight: 600;
}

.step-item.active .step-circle {
  background: var(--color-primary);
  border-color: var(--color-primary);
  color: #ffffff;
  box-shadow: 0 0 10px rgba(37, 99, 235, 0.35);
}

.step-item.active .step-name {
  color: var(--color-primary);
}

.step-item.done .step-circle {
  background: var(--color-disarmed);
  border-color: var(--color-disarmed);
  color: #ffffff;
}

.step-line {
  flex: 1;
  height: 2px;
  background: var(--border-subtle);
  margin: 0 8px;
  margin-bottom: 22px;
  transition: background 0.25s ease;
}

.step-line.active {
  background: var(--color-primary);
}

/* Step Body: Init */
.step-body-init {
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  gap: 18px;
  padding: 16px 0;
}

.icon-circle-pulse {
  width: 72px;
  height: 72px;
  border-radius: 50%;
  background: var(--color-primary-subtle);
  color: var(--color-primary);
  display: flex;
  align-items: center;
  justify-content: center;
  border: 1px solid var(--color-primary-border);
}

.init-text-box h4 {
  font-size: 1.05rem;
  font-weight: 700;
  color: var(--text-main);
  margin-bottom: 6px;
}

.guide-text {
  font-size: 0.825rem;
  color: var(--text-secondary);
  max-width: 460px;
  line-height: 1.55;
}

/* Step Body: Capturing */
.step-body-capturing {
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  gap: 20px;
  padding: 24px 0;
}

.angle-box {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 10px;
}

.angle-symbol.blue { color: var(--color-primary); }
.angle-symbol.amber { color: var(--color-stay); }
.angle-symbol.emerald { color: var(--color-disarmed); }

.angle-title {
  font-size: 1rem;
  font-weight: 800;
  color: var(--text-main);
  letter-spacing: 0.02em;
}

.angle-desc {
  font-size: 0.8rem;
  color: var(--text-secondary);
  max-width: 380px;
}

.capturing-status {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 0.75rem;
  color: var(--text-muted);
}

.mini-spinner {
  width: 16px;
  height: 16px;
  border: 2px solid var(--border-subtle);
  border-top-color: var(--color-primary);
  border-radius: 50%;
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

.btn-cancel {
  color: var(--color-armed);
}

/* Step Body: Form */
.step-body-form {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.success-banner {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 16px;
  background: var(--color-disarmed-subtle);
  border: 1px solid var(--color-disarmed-border);
  border-radius: var(--radius-md);
  color: var(--color-disarmed-text);
}

.success-icon {
  color: var(--color-disarmed);
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

.form-label {
  font-size: 0.78rem;
  font-weight: 600;
  color: var(--text-main);
}

.radio-card-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 10px;
}

.radio-card {
  display: flex;
  flex-direction: column;
  padding: 10px 14px;
  background: var(--bg-subtle);
  border: 1.5px solid var(--border-subtle);
  border-radius: var(--radius-md);
  cursor: pointer;
  transition: all 0.15s ease;
}

.radio-card.selected {
  border-color: var(--color-primary);
  background: var(--color-primary-subtle);
}

.radio-card input {
  display: none;
}

.radio-title {
  font-size: 0.825rem;
  font-weight: 700;
  color: var(--text-main);
}

.radio-desc {
  font-size: 0.7rem;
  color: var(--text-secondary);
  margin-top: 2px;
}

.form-actions {
  display: flex;
  justify-content: flex-end;
  gap: 10px;
  margin-top: 8px;
}
</style>

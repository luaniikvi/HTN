<template>
  <div v-if="alert" class="modal-backdrop">
    <div ref="breachModalRef" class="clean-card breach-modal pulsing-alarm">
      <!-- Header -->
      <div class="breach-header">
        <div class="alert-icon-box">
          <AlertOctagon :size="28" class="alert-svg" />
        </div>
        <div class="breach-header-text">
          <span class="badge badge-armed">REAL-TIME INTRUSION ALERT</span>
          <h2 class="breach-title">Unauthorized Door Breach!</h2>
        </div>
      </div>

      <!-- Body -->
      <div class="breach-body">
        <p class="breach-desc">
          The MC-38 magnetic contact sensor detected a <strong>DOOR OPEN</strong> event while the system was secured in
          <span class="badge" :class="alert.mode === 'ARMED' ? 'badge-armed' : 'badge-stay'">{{ alert.mode }}</span>
          mode without valid Face ID biometric verification!
        </p>

        <div class="breach-telemetry font-mono">
          <div class="telemetry-row">
            <span>Device ID:</span>
            <strong>{{ alert.deviceId || 'dev_01' }}</strong>
          </div>
          <div class="telemetry-row">
            <span>Timestamp:</span>
            <strong>{{ formatTime(alert.timestamp) }}</strong>
          </div>
          <div class="telemetry-row">
            <span>Buzzer Status:</span>
            <strong :class="alert.mode === 'ARMED' ? 'text-danger' : 'text-warning'">
              {{ alert.mode === 'ARMED' ? 'BUZZER_ON (Continuous High-Output Siren)' : 'BUZZER_BEEP (Local Perimeter Reminder)' }}
            </strong>
          </div>
        </div>

        <!-- Snapshot Capture Preview -->
        <div v-if="alert.imageUrl" class="snapshot-container">
          <div class="snapshot-header">
            <span class="snapshot-label">📸 CAPTURED BREACH EVIDENCE (SVGA COLOR)</span>
            <a :href="alert.imageUrl" target="_blank" class="full-link">
              Open Full Size ↗
            </a>
          </div>
          <div class="snapshot-frame">
            <img :src="alert.imageUrl" alt="Intrusion Snapshot" class="snapshot-img" />
          </div>
        </div>

        <div v-else-if="alert.mode === 'ARMED'" class="snapshot-waiting font-mono">
          <span class="pulse-dot"></span>
          <span>Camera capturing and uploading evidence snapshot...</span>
        </div>
      </div>

      <!-- Actions -->
      <div class="breach-actions">
        <button class="btn btn-primary" @click="handleDisarm" :disabled="isProcessing">
          <ShieldOff :size="16" />
          <span>Switch to DISARMED (Open Freely)</span>
        </button>

        <button class="btn btn-danger" @click="handleSilenceAlarm" :disabled="isProcessing">
          <BellOff :size="16" />
          <span>Silence Siren Only</span>
        </button>

        <button class="btn btn-secondary btn-sm" @click="system.dismissBreachAlert">
          Dismiss Notice
        </button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, ref, onMounted, watch } from 'vue';
import { useSystemStore } from '../stores/system';
import gsap from 'gsap';
import {
  AlertOctagon,
  ShieldOff,
  BellOff
} from 'lucide-vue-next';

const system = useSystemStore();
const alert = computed(() => system.activeBreachAlert);
const isProcessing = ref(false);
const breachModalRef = ref(null);

watch(alert, (newVal) => {
  if (newVal && breachModalRef.value) {
    gsap.fromTo(
      breachModalRef.value,
      { scale: 0.9, opacity: 0 },
      { scale: 1, opacity: 1, duration: 0.3, ease: 'back.out(1.5)' }
    );
  }
});

async function handleDisarm() {
  isProcessing.value = true;
  try {
    await system.changeMode('DISARMED');
  } catch (err) {
    alert('Error: ' + err.message);
  } finally {
    isProcessing.value = false;
  }
}

async function handleSilenceAlarm() {
  isProcessing.value = true;
  try {
    await system.toggleAlarm(false);
  } catch (err) {
    alert('Error: ' + err.message);
  } finally {
    isProcessing.value = false;
  }
}

function formatTime(iso) {
  if (!iso) return 'Just now';
  return new Date(iso).toLocaleTimeString('en-US');
}

onMounted(() => {
  if (alert.value && breachModalRef.value) {
    gsap.fromTo(
      breachModalRef.value,
      { scale: 0.9, opacity: 0 },
      { scale: 1, opacity: 1, duration: 0.3, ease: 'back.out(1.5)' }
    );
  }
});
</script>

<style scoped>
.modal-backdrop {
  position: fixed;
  inset: 0;
  background: rgba(15, 23, 42, 0.75);
  backdrop-filter: blur(8px);
  -webkit-backdrop-filter: blur(8px);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 2000;
  padding: 20px;
}

.breach-modal {
  width: 100%;
  max-width: 540px;
  background: #ffffff;
  border: 2px solid var(--color-armed);
  border-radius: var(--radius-lg);
  padding: 24px;
  display: flex;
  flex-direction: column;
  gap: 18px;
  box-shadow: 0 20px 35px -5px rgba(239, 68, 68, 0.3);
}

.breach-header {
  display: flex;
  align-items: center;
  gap: 14px;
}

.alert-icon-box {
  width: 48px;
  height: 48px;
  border-radius: var(--radius-md);
  background: var(--color-armed-subtle);
  color: var(--color-armed);
  display: flex;
  align-items: center;
  justify-content: center;
  border: 1px solid var(--color-armed-border);
}

.breach-title {
  font-size: 1.25rem;
  font-weight: 800;
  color: var(--color-armed-text);
  margin-top: 2px;
}

.breach-body {
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.breach-desc {
  font-size: 0.875rem;
  line-height: 1.55;
  color: var(--text-main);
}

.breach-telemetry {
  background: var(--bg-subtle);
  border: 1px solid var(--border-subtle);
  border-radius: var(--radius-md);
  padding: 12px 14px;
  font-size: 0.78rem;
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.telemetry-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.text-danger { color: var(--color-armed); }
.text-warning { color: var(--color-stay); }

/* Snapshot Preview */
.snapshot-container {
  display: flex;
  flex-direction: column;
  gap: 8px;
  background: #0f172a;
  border-radius: var(--radius-md);
  padding: 10px;
}

.snapshot-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.snapshot-label {
  font-size: 0.7rem;
  font-weight: 700;
  color: #f87171;
  letter-spacing: 0.04em;
}

.full-link {
  font-size: 0.72rem;
  color: #60a5fa;
  text-decoration: none;
  font-weight: 600;
}

.full-link:hover {
  text-decoration: underline;
}

.snapshot-frame {
  width: 100%;
  max-height: 240px;
  border-radius: var(--radius-sm);
  overflow: hidden;
  display: flex;
  align-items: center;
  justify-content: center;
  background: #000;
}

.snapshot-img {
  width: 100%;
  height: auto;
  max-height: 240px;
  object-fit: contain;
}

.snapshot-waiting {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 10px 14px;
  background: var(--color-armed-subtle);
  border: 1px dashed var(--color-armed-border);
  border-radius: var(--radius-md);
  font-size: 0.78rem;
  color: var(--color-armed-text);
}

.pulse-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--color-armed);
  animation: pulse-dot-anim 1.2s infinite;
}

@keyframes pulse-dot-anim {
  0% { transform: scale(0.9); opacity: 0.6; }
  50% { transform: scale(1.3); opacity: 1; }
  100% { transform: scale(0.9); opacity: 0.6; }
}

.breach-actions {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-top: 4px;
}
</style>

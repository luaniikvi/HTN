<template>
  <div class="security-page">
    <!-- Header -->
    <div class="page-title-row">
      <div>
        <h1 class="page-title">Security & Alarm Control</h1>
        <p class="page-subtitle">Configure real-time protection modes, siren state, and door breach grace timeout</p>
      </div>

      <div class="header-badges">
        <span class="badge" :class="system.doorState === 'OPEN' ? 'badge-armed' : 'badge-disarmed'">
          Door: {{ system.doorState }}
        </span>
        <span class="badge" :class="system.status === 'ONLINE' ? 'badge-online' : 'badge-offline'">
          Device: {{ system.status }}
        </span>
      </div>
    </div>

    <!-- Security Mode Cards Grid (3 Core Modes) -->
    <div class="section-container">
      <h3 class="section-title">Active Protection Mode</h3>
      <div class="modes-grid">
        <!-- 1. DISARMED -->
        <div
          class="clean-card mode-card disarmed-card"
          :class="{ active: system.securityMode === 'DISARMED', disabled: isSubmitting }"
          @click="selectMode('DISARMED')"
        >
          <div class="card-radio-mark">
            <span class="radio-dot"></span>
          </div>
          <div class="mode-icon-box disarmed">
            <MorphIcon :icon="ShieldCheck" :size="24" spring="snappy" />
          </div>
          <div class="mode-info">
            <div class="mode-header-line">
              <h4 class="mode-name">DISARMED</h4>
              <span v-if="system.securityMode === 'DISARMED'" class="active-tag">Active</span>
            </div>
            <p class="mode-desc">
              Normal everyday operation. Doors can be opened freely without triggering buzzer alerts. All access events are still logged.
            </p>
          </div>
        </div>

        <!-- 2. STAY -->
        <div
          class="clean-card mode-card stay-card"
          :class="{ active: system.securityMode === 'STAY', disabled: isSubmitting }"
          @click="selectMode('STAY')"
        >
          <div class="card-radio-mark">
            <span class="radio-dot"></span>
          </div>
          <div class="mode-icon-box stay">
            <MorphIcon :icon="Home" :size="24" spring="snappy" />
          </div>
          <div class="mode-info">
            <div class="mode-header-line">
              <h4 class="mode-name">STAY</h4>
              <span v-if="system.securityMode === 'STAY'" class="active-tag">Active</span>
            </div>
            <p class="mode-desc">
              Perimeter protection while occupants are inside. Unauthorized door openings emit a periodic local reminder chime (~2s beep).
            </p>
          </div>
        </div>

        <!-- 3. ARMED -->
        <div
          class="clean-card mode-card armed-card"
          :class="{ active: system.securityMode === 'ARMED', disabled: isSubmitting }"
          @click="selectMode('ARMED')"
        >
          <div class="card-radio-mark">
            <span class="radio-dot"></span>
          </div>
          <div class="mode-icon-box armed">
            <MorphIcon :icon="ShieldAlert" :size="24" spring="snappy" />
          </div>
          <div class="mode-info">
            <div class="mode-header-line">
              <h4 class="mode-name">ARMED</h4>
              <span v-if="system.securityMode === 'ARMED'" class="active-tag">Active</span>
            </div>
            <p class="mode-desc">
              Maximum security. Valid Face ID recognition is strictly required before entry; unauthorized opening triggers continuous latching high-output siren.
            </p>
          </div>
        </div>
      </div>
    </div>

    <!-- Secondary Controls: Panic Siren & Grace Timeout -->
    <div class="secondary-grid">
      <!-- Panic Siren Control Card -->
      <div class="clean-card action-card">
        <div class="card-title-row">
          <div class="card-title-group">
            <Bell :size="18" class="title-icon red" />
            <h4>Emergency Siren Control</h4>
          </div>
          <span
            class="badge"
            :class="isAlarmActive ? 'badge-armed' : 'badge-offline'"
          >
            {{ isAlarmActive ? 'SIREN ACTIVE' : 'SIREN SILENT' }}
          </span>
        </div>

        <p class="card-text">
          Manually trigger or silence the physical 5V buzzer relay on the ESP32-S3. Useful during emergency drills or to silence an active breach alert.
        </p>

        <button
          class="btn siren-trigger-btn"
          :class="isAlarmActive ? 'btn-danger pulsing-alarm' : 'btn-secondary'"
          @click="handleToggleAlarm"
          :disabled="isSubmitting"
        >
          <MorphIcon :icon="isAlarmActive ? LucideBellRing : LucideBell" :size="18" spring="snappy" />
          <span>{{ isAlarmActive ? 'Silence Active Siren' : 'Trigger Panic Siren' }}</span>
        </button>
      </div>

      <!-- Door Contact & Grace Timeout Card -->
      <div class="clean-card action-card">
        <div class="card-title-row">
          <div class="card-title-group">
            <Clock :size="18" class="title-icon blue" />
            <h4>Door Breach Grace Period</h4>
          </div>
          <span class="badge badge-primary font-mono">
            {{ system.gracePeriod }}s Active
          </span>
        </div>

        <p class="card-text">
          Defines the allowable window (in seconds) between door opening and alarm trigger, allowing authorized individuals to present their face to the camera.
        </p>

        <div class="grace-config-row">
          <div class="quick-presets">
            <button
              v-for="preset in [5, 10, 15, 30]"
              :key="preset"
              class="btn btn-secondary btn-xs preset-btn"
              :class="{ selected: graceInput === preset }"
              @click="graceInput = preset"
            >
              {{ preset }}s
            </button>
          </div>

          <div class="input-inline-group">
            <input
              type="number"
              min="3"
              max="60"
              v-model.number="graceInput"
              class="input-control grace-input font-mono"
            />
            <span class="input-unit">seconds</span>
            <button class="btn btn-primary btn-sm" @click="saveGracePeriod" :disabled="isSubmitting">
              Save
            </button>
          </div>
        </div>

        <transition name="fade">
          <p v-if="saveMessage" class="feedback-msg">
            {{ saveMessage }}
          </p>
        </transition>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useSystemStore } from '../stores/system';
import { MorphIcon } from 'morphicons/vue';
import {
  ShieldCheck,
  Home,
  ShieldAlert,
  Bell as LucideBell,
  BellRing as LucideBellRing
} from 'lucide';
import {
  Bell,
  Clock
} from 'lucide-vue-next';

const system = useSystemStore();
const isSubmitting = ref(false);
const graceInput = ref(system.gracePeriod || 10);
const saveMessage = ref('');

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
    alert('Failed to change security mode: ' + (err.response?.data?.message || err.message));
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
    alert('Failed to toggle alarm: ' + (err.response?.data?.message || err.message));
  } finally {
    isSubmitting.value = false;
  }
}

async function saveGracePeriod() {
  if (graceInput.value < 3 || graceInput.value > 60) {
    alert('Grace period must be between 3 and 60 seconds.');
    return;
  }
  isSubmitting.value = true;
  try {
    await system.updateGracePeriod(graceInput.value);
    saveMessage.value = '✓ Grace timeout successfully updated';
    setTimeout(() => {
      saveMessage.value = '';
    }, 3000);
  } catch (err) {
    alert('Failed to update grace period: ' + err.message);
  } finally {
    isSubmitting.value = false;
  }
}
</script>

<style scoped>
.security-page {
  display: flex;
  flex-direction: column;
  gap: 26px;
}

.page-title-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 14px;
}

.page-title {
  font-size: 1.65rem;
  font-weight: 800;
  color: var(--text-main);
  letter-spacing: -0.02em;
}

.page-subtitle {
  font-size: 0.95rem;
  color: var(--text-secondary);
  margin-top: 4px;
}

.header-badges {
  display: flex;
  align-items: center;
  gap: 10px;
}

.section-container {
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.section-title {
  font-size: 1rem;
  font-weight: 700;
  color: var(--text-secondary);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

/* 3-Mode Selection Grid */
.modes-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 18px;
}

.mode-card {
  padding: 20px;
  display: flex;
  flex-direction: column;
  gap: 14px;
  cursor: pointer;
  position: relative;
  border-width: 2px;
  border-color: var(--border-subtle);
  transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
}

.mode-card:hover:not(.disabled) {
  transform: translateY(-2px);
  border-color: var(--border-strong);
  box-shadow: var(--shadow-card-hover);
}

.mode-card.disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.card-radio-mark {
  position: absolute;
  top: 18px;
  right: 18px;
  width: 18px;
  height: 18px;
  border-radius: 50%;
  border: 2px solid var(--border-strong);
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.2s ease;
}

.radio-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: transparent;
  transition: all 0.2s ease;
}

.mode-icon-box {
  width: 44px;
  height: 44px;
  border-radius: var(--radius-md);
  display: flex;
  align-items: center;
  justify-content: center;
}

.mode-icon-box.disarmed {
  background: var(--color-disarmed-subtle);
  color: var(--color-disarmed);
  border: 1px solid var(--color-disarmed-border);
}

.mode-icon-box.stay {
  background: var(--color-stay-subtle);
  color: var(--color-stay);
  border: 1px solid var(--color-stay-border);
}

.mode-icon-box.armed {
  background: var(--color-armed-subtle);
  color: var(--color-armed);
  border: 1px solid var(--color-armed-border);
}

.mode-info {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.mode-header-line {
  display: flex;
  align-items: center;
  gap: 10px;
}

.mode-name {
  font-size: 1.25rem;
  font-weight: 800;
  color: var(--text-main);
  letter-spacing: -0.01em;
}

.active-tag {
  font-size: 0.75rem;
  font-weight: 700;
  text-transform: uppercase;
  padding: 3px 9px;
  border-radius: var(--radius-sm);
  background: var(--bg-subtle);
  color: var(--text-secondary);
}

.mode-desc {
  font-size: 0.9rem;
  color: var(--text-secondary);
  line-height: 1.5;
}

/* Active State styling */
.disarmed-card.active {
  border-color: var(--color-disarmed);
  background: #ffffff;
  box-shadow: 0 4px 14px rgba(16, 185, 129, 0.15);
}

.disarmed-card.active .card-radio-mark {
  border-color: var(--color-disarmed);
}

.disarmed-card.active .radio-dot {
  background: var(--color-disarmed);
}

.disarmed-card.active .active-tag {
  background: var(--color-disarmed-subtle);
  color: var(--color-disarmed-text);
  border: 1px solid var(--color-disarmed-border);
}

.stay-card.active {
  border-color: var(--color-stay);
  background: #ffffff;
  box-shadow: 0 4px 14px rgba(245, 158, 11, 0.15);
}

.stay-card.active .card-radio-mark {
  border-color: var(--color-stay);
}

.stay-card.active .radio-dot {
  background: var(--color-stay);
}

.stay-card.active .active-tag {
  background: var(--color-stay-subtle);
  color: var(--color-stay-text);
  border: 1px solid var(--color-stay-border);
}

.armed-card.active {
  border-color: var(--color-armed);
  background: #ffffff;
  box-shadow: 0 4px 14px rgba(239, 68, 68, 0.15);
}

.armed-card.active .card-radio-mark {
  border-color: var(--color-armed);
}

.armed-card.active .radio-dot {
  background: var(--color-armed);
}

.armed-card.active .active-tag {
  background: var(--color-armed-subtle);
  color: var(--color-armed-text);
  border: 1px solid var(--color-armed-border);
}

/* Secondary Row */
.secondary-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 20px;
}

.action-card {
  padding: 24px;
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.card-title-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.card-title-group {
  display: flex;
  align-items: center;
  gap: 10px;
}

.card-title-group h4 {
  font-size: 1.15rem;
  font-weight: 700;
  color: var(--text-main);
}

.title-icon.red {
  color: var(--color-armed);
}

.title-icon.blue {
  color: var(--color-primary);
}

.card-text {
  font-size: 0.925rem;
  color: var(--text-secondary);
  line-height: 1.55;
}

.siren-trigger-btn {
  width: 100%;
  padding: 14px;
  font-size: 1.05rem;
}

/* Grace Config Row */
.grace-config-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 14px;
  flex-wrap: wrap;
}

.quick-presets {
  display: flex;
  gap: 8px;
}

.preset-btn.selected {
  background: var(--color-primary-subtle);
  color: var(--color-primary);
  border-color: var(--color-primary-border);
}

.input-inline-group {
  display: flex;
  align-items: center;
  gap: 10px;
}

.grace-input {
  width: 80px;
  padding: 8px 12px;
  text-align: center;
  font-size: 1rem;
}

.input-unit {
  font-size: 0.9rem;
  color: var(--text-muted);
}

.feedback-msg {
  font-size: 0.85rem;
  font-weight: 600;
  color: var(--color-disarmed);
}

@media (max-width: 960px) {
  .modes-grid {
    grid-template-columns: 1fr;
  }

  .secondary-grid {
    grid-template-columns: 1fr;
  }
}
</style>

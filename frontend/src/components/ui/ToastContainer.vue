<template>
  <div class="toast-container" aria-live="polite">
    <TransitionGroup name="toast" tag="div" class="toast-stack">
      <div
        v-for="toast in notify.toasts"
        :key="toast.id"
        class="toast-card"
        :class="[`toast-${toast.type}`]"
        role="alert"
      >
        <!-- Ambient Left Accent Pill -->
        <div class="toast-accent-pill"></div>

        <!-- Status Icon with Glow Box -->
        <div class="toast-icon-box">
          <AlertTriangle v-if="toast.type === 'warning'" :size="18" />
          <AlertOctagon v-else-if="toast.type === 'error'" :size="18" />
          <CheckCircle2 v-else-if="toast.type === 'success'" :size="18" />
          <Info v-else :size="18" />
        </div>

        <!-- Content (Title & Message) -->
        <div class="toast-body">
          <div class="toast-title">{{ toast.title }}</div>
          <div class="toast-message">{{ toast.message }}</div>
        </div>

        <!-- Close Action -->
        <button
          type="button"
          class="toast-close-btn"
          @click="notify.dismissToast(toast.id)"
          aria-label="Dismiss alert"
        >
          <X :size="14" />
        </button>

        <!-- Dynamic Timeout Progress Line -->
        <div
          v-if="toast.duration > 0"
          class="toast-progress-bar"
          :style="{ animationDuration: `${toast.duration}ms` }"
        ></div>
      </div>
    </TransitionGroup>
  </div>
</template>

<script setup>
import { useNotifyStore } from '../../stores/notify';
import {
  AlertTriangle,
  AlertOctagon,
  CheckCircle2,
  Info,
  X
} from 'lucide-vue-next';

const notify = useNotifyStore();
</script>

<style scoped>
.toast-container {
  position: fixed;
  top: 24px;
  right: 24px;
  z-index: 99999;
  max-width: 420px;
  width: calc(100vw - 32px);
  pointer-events: none;
}

.toast-stack {
  display: flex;
  flex-direction: column;
  gap: 12px;
  width: 100%;
}

.toast-card {
  position: relative;
  display: flex;
  align-items: flex-start;
  gap: 12px;
  padding: 14px 16px 16px;
  border-radius: 14px;
  background: rgba(15, 23, 42, 0.92);
  backdrop-filter: blur(20px) saturate(180%);
  -webkit-backdrop-filter: blur(20px) saturate(180%);
  border: 1px solid rgba(255, 255, 255, 0.1);
  box-shadow: 0 12px 32px -4px rgba(0, 0, 0, 0.65), 0 4px 12px rgba(0, 0, 0, 0.4);
  overflow: hidden;
  pointer-events: auto;
  user-select: none;
  width: 100%;
  box-sizing: border-box;
}

/* Transitions: Spring pop in and smooth slide out */
.toast-enter-from {
  opacity: 0;
  transform: translate3d(50px, 0, 0) scale(0.92);
}
.toast-enter-active {
  transition: all 0.35s cubic-bezier(0.175, 0.885, 0.32, 1.275);
}
.toast-enter-to {
  opacity: 1;
  transform: translate3d(0, 0, 0) scale(1);
}
.toast-leave-from {
  opacity: 1;
  transform: translate3d(0, 0, 0) scale(1);
}
.toast-leave-active {
  transition: all 0.22s cubic-bezier(0.4, 0, 1, 1);
}
.toast-leave-to {
  opacity: 0;
  transform: translate3d(60px, 0, 0) scale(0.88);
}
.toast-move {
  transition: transform 0.3s ease;
}

/* Left accent bar */
.toast-accent-pill {
  position: absolute;
  top: 10px;
  bottom: 10px;
  left: 0;
  width: 3.5px;
  border-radius: 0 4px 4px 0;
}

/* Icon Box */
.toast-icon-box {
  width: 32px;
  height: 32px;
  border-radius: 9px;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  margin-top: 1px;
}

/* Body */
.toast-body {
  flex: 1;
  min-width: 0;
}

.toast-title {
  font-size: 13px;
  font-weight: 700;
  letter-spacing: 0.3px;
  line-height: 1.3;
  margin-bottom: 3px;
}

.toast-message {
  font-size: 12.5px;
  line-height: 1.45;
  color: #94a3b8;
  word-break: break-word;
}

/* Close Button */
.toast-close-btn {
  background: transparent;
  border: none;
  color: #64748b;
  cursor: pointer;
  padding: 4px;
  border-radius: 6px;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.15s ease;
  flex-shrink: 0;
  margin-top: -2px;
  margin-right: -4px;
}

.toast-close-btn:hover {
  color: #f1f5f9;
  background: rgba(255, 255, 255, 0.08);
}

/* Progress bar at bottom */
.toast-progress-bar {
  position: absolute;
  bottom: 0;
  left: 0;
  right: 0;
  height: 2.5px;
  animation: toast-countdown linear forwards;
}

@keyframes toast-countdown {
  from {
    width: 100%;
  }
  to {
    width: 0%;
  }
}

/* Type variations */
/* Warning / Offline */
.toast-warning {
  border-color: rgba(245, 158, 11, 0.35);
  box-shadow: 0 12px 32px -4px rgba(0, 0, 0, 0.65), 0 0 24px -4px rgba(245, 158, 11, 0.25);
}
.toast-warning .toast-accent-pill {
  background: #f59e0b;
}
.toast-warning .toast-icon-box {
  background: rgba(245, 158, 11, 0.15);
  color: #fbbf24;
}
.toast-warning .toast-title {
  color: #fde68a;
}
.toast-warning .toast-progress-bar {
  background: #f59e0b;
}

/* Error */
.toast-error {
  border-color: rgba(239, 68, 68, 0.4);
  box-shadow: 0 12px 32px -4px rgba(0, 0, 0, 0.65), 0 0 24px -4px rgba(239, 68, 68, 0.25);
}
.toast-error .toast-accent-pill {
  background: #ef4444;
}
.toast-error .toast-icon-box {
  background: rgba(239, 68, 68, 0.16);
  color: #f87171;
}
.toast-error .toast-title {
  color: #fecaca;
}
.toast-error .toast-progress-bar {
  background: #ef4444;
}

/* Success */
.toast-success {
  border-color: rgba(16, 185, 129, 0.4);
  box-shadow: 0 12px 32px -4px rgba(0, 0, 0, 0.65), 0 0 24px -4px rgba(16, 185, 129, 0.25);
}
.toast-success .toast-accent-pill {
  background: #10b981;
}
.toast-success .toast-icon-box {
  background: rgba(16, 185, 129, 0.15);
  color: #34d399;
}
.toast-success .toast-title {
  color: #a7f3d0;
}
.toast-success .toast-progress-bar {
  background: #10b981;
}

/* Info */
.toast-info {
  border-color: rgba(59, 130, 246, 0.4);
  box-shadow: 0 12px 32px -4px rgba(0, 0, 0, 0.65), 0 0 24px -4px rgba(59, 130, 246, 0.25);
}
.toast-info .toast-accent-pill {
  background: #3b82f6;
}
.toast-info .toast-icon-box {
  background: rgba(59, 130, 246, 0.15);
  color: #60a5fa;
}
.toast-info .toast-title {
  color: #bfdbfe;
}
.toast-info .toast-progress-bar {
  background: #3b82f6;
}
</style>

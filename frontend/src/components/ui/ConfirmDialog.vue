<template>
  <Teleport to="body">
    <Transition name="confirm-fade">
      <div
        v-if="notify.confirmState.isOpen"
        class="confirm-backdrop"
        @click.self="cancel"
        @keydown.esc="cancel"
        tabindex="-1"
      >
        <div class="confirm-dialog" :class="[`dialog-${notify.confirmState.type}`]" role="dialog" aria-modal="true">
          <!-- Close button top-right -->
          <button type="button" class="dialog-close-btn" @click="cancel" aria-label="Close">
            <X :size="16" />
          </button>

          <!-- Header / Icon Box -->
          <div class="dialog-header">
            <div class="dialog-icon-circle">
              <Trash2 v-if="notify.confirmState.type === 'danger'" :size="24" />
              <AlertTriangle v-else-if="notify.confirmState.type === 'warning'" :size="24" />
              <HelpCircle v-else :size="24" />
            </div>
            <div class="dialog-title-group">
              <h3 class="dialog-title">{{ notify.confirmState.title }}</h3>
              <span class="dialog-badge">{{ (notify.confirmState.type || 'ACTION').toUpperCase() }} CONFIRMATION</span>
            </div>
          </div>

          <!-- Body Message -->
          <div class="dialog-body">
            <p class="dialog-message">{{ notify.confirmState.message }}</p>
          </div>

          <!-- Actions -->
          <div class="dialog-actions">
            <button type="button" class="btn btn-secondary btn-cancel" @click="cancel">
              {{ notify.confirmState.cancelText || 'Cancel' }}
            </button>
            <button
              type="button"
              class="btn btn-confirm"
              :class="notify.confirmState.type === 'danger' ? 'btn-danger' : 'btn-primary'"
              @click="confirm"
            >
              {{ notify.confirmState.confirmText || 'Confirm' }}
            </button>
          </div>
        </div>
      </div>
    </Transition>
  </Teleport>
</template>

<script setup>
import { useNotifyStore } from '../../stores/notify';
import {
  Trash2,
  AlertTriangle,
  HelpCircle,
  X
} from 'lucide-vue-next';

const notify = useNotifyStore();

function cancel() {
  notify.handleConfirmResponse(false);
}

function confirm() {
  notify.handleConfirmResponse(true);
}
</script>

<style scoped>
.confirm-backdrop {
  position: fixed;
  inset: 0;
  background: rgba(5, 9, 18, 0.78);
  backdrop-filter: blur(12px);
  -webkit-backdrop-filter: blur(12px);
  z-index: 100000;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 20px;
  outline: none;
}

.confirm-dialog {
  position: relative;
  width: 100%;
  max-width: 440px;
  background: linear-gradient(145deg, rgba(24, 31, 47, 0.96), rgba(15, 20, 32, 0.98));
  border: 1px solid rgba(255, 255, 255, 0.1);
  border-radius: 18px;
  padding: 24px;
  box-shadow: 0 25px 60px -10px rgba(0, 0, 0, 0.8), 0 0 0 1px rgba(255, 255, 255, 0.05);
  display: flex;
  flex-direction: column;
  gap: 18px;
  overflow: hidden;
  box-sizing: border-box;
}

/* Transitions */
.confirm-fade-enter-active {
  transition: opacity 0.25s ease;
}
.confirm-fade-enter-active .confirm-dialog {
  transition: all 0.32s cubic-bezier(0.175, 0.885, 0.32, 1.275);
}

.confirm-fade-leave-active {
  transition: opacity 0.2s ease;
}
.confirm-fade-leave-active .confirm-dialog {
  transition: all 0.18s cubic-bezier(0.4, 0, 1, 1);
}

.confirm-fade-enter-from {
  opacity: 0;
}
.confirm-fade-enter-from .confirm-dialog {
  opacity: 0;
  transform: scale(0.88) translateY(16px);
}

.confirm-fade-leave-to {
  opacity: 0;
}
.confirm-fade-leave-to .confirm-dialog {
  opacity: 0;
  transform: scale(0.92) translateY(10px);
}

/* Close button */
.dialog-close-btn {
  position: absolute;
  top: 16px;
  right: 16px;
  background: transparent;
  border: none;
  color: #64748b;
  cursor: pointer;
  padding: 6px;
  border-radius: 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.15s ease;
}

.dialog-close-btn:hover {
  color: #f1f5f9;
  background: rgba(255, 255, 255, 0.08);
}

/* Header */
.dialog-header {
  display: flex;
  align-items: center;
  gap: 14px;
}

.dialog-icon-circle {
  width: 48px;
  height: 48px;
  border-radius: 14px;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
}

.dialog-title-group {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.dialog-title {
  font-size: 17px;
  font-weight: 700;
  color: #f8fafc;
  line-height: 1.25;
  margin: 0;
}

.dialog-badge {
  font-size: 10px;
  font-weight: 700;
  letter-spacing: 0.6px;
  font-family: monospace;
}

/* Message */
.dialog-body {
  padding-left: 2px;
}

.dialog-message {
  font-size: 13.5px;
  line-height: 1.55;
  color: #94a3b8;
  margin: 0;
}

/* Actions */
.dialog-actions {
  display: flex;
  justify-content: flex-end;
  align-items: center;
  gap: 12px;
  margin-top: 6px;
}

.btn-cancel {
  padding: 9px 18px;
  font-size: 13px;
  font-weight: 600;
}

.btn-confirm {
  padding: 9px 20px;
  font-size: 13px;
  font-weight: 600;
}

/* Danger variation */
.dialog-danger {
  border-color: rgba(239, 68, 68, 0.25);
  box-shadow: 0 25px 60px -10px rgba(0, 0, 0, 0.8), 0 0 35px -5px rgba(239, 68, 68, 0.18);
}
.dialog-danger .dialog-icon-circle {
  background: rgba(239, 68, 68, 0.15);
  color: #f87171;
  border: 1px solid rgba(239, 68, 68, 0.3);
}
.dialog-danger .dialog-badge {
  color: #f87171;
}

/* Warning variation */
.dialog-warning {
  border-color: rgba(245, 158, 11, 0.25);
  box-shadow: 0 25px 60px -10px rgba(0, 0, 0, 0.8), 0 0 35px -5px rgba(245, 158, 11, 0.18);
}
.dialog-warning .dialog-icon-circle {
  background: rgba(245, 158, 11, 0.15);
  color: #fbbf24;
  border: 1px solid rgba(245, 158, 11, 0.3);
}
.dialog-warning .dialog-badge {
  color: #fbbf24;
}

/* Info variation */
.dialog-info {
  border-color: rgba(59, 130, 246, 0.25);
  box-shadow: 0 25px 60px -10px rgba(0, 0, 0, 0.8), 0 0 35px -5px rgba(59, 130, 246, 0.18);
}
.dialog-info .dialog-icon-circle {
  background: rgba(59, 130, 246, 0.15);
  color: #60a5fa;
  border: 1px solid rgba(59, 130, 246, 0.3);
}
.dialog-info .dialog-badge {
  color: #60a5fa;
}
</style>

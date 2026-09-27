<template>
  <div class="glass-card table-card">
    <div class="table-header">
      <div class="title-group">
        <h3>REGISTERED FACE DIRECTORY (NVS FLASH)</h3>
        <span class="count-badge font-mono">{{ faces.length }} records</span>
      </div>
      <div class="table-actions">
        <!-- Delete All Button -->
        <button
          v-if="faces.length > 0"
          class="btn btn-danger-outline btn-sm"
          @click="showDeleteAllModal = true"
          :disabled="isDeletingAll || !isDeviceReady"
          :title="!isDeviceReady ? 'ESP32 device is offline' : 'Delete all registered face profiles'"
        >
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class="btn-icon">
            <polyline points="3 6 5 6 21 6"></polyline>
            <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path>
          </svg>
          {{ isDeletingAll ? 'Deleting...' : 'Delete All' }}
        </button>

        <!-- Refresh Button -->
        <button class="btn btn-secondary btn-sm" @click="fetchFaces" :disabled="isLoading">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class="refresh-icon" :class="{ spinning: isLoading }">
            <polyline points="23 4 23 10 17 10"></polyline>
            <path d="M1 20 1 14 7 14"></path>
            <path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"></path>
          </svg>
          Refresh
        </button>
      </div>
    </div>

    <div class="table-wrapper">
      <table class="data-table">
        <thead>
          <tr>
            <th>FACE ID</th>
            <th>FULL NAME</th>
            <th>ACCESS TIER</th>
            <th>EXPIRATION</th>
            <th>STATUS</th>
            <th>ACTIONS</th>
          </tr>
        </thead>
        <tbody>
          <tr v-if="faces.length === 0">
            <td colspan="6" class="empty-cell">No faces currently registered in the system.</td>
          </tr>
          <tr v-for="face in faces" :key="face.id" :class="{ 'row-inactive': !face.is_active }">
            <td class="font-mono font-bold">#{{ face.face_id }}</td>
            <td class="name-cell">{{ face.name }}</td>
            <td>
              <span class="badge" :class="face.role_type === 'PERMANENT' ? 'badge-disarmed' : 'badge-stay'">
                {{ face.role_type }}
              </span>
            </td>
            <td class="font-mono time-cell">
              {{ face.role_type === 'PERMANENT' ? 'Permanent' : formatTime(face.valid_until) }}
            </td>
            <td>
              <!-- Switch Active / Inactive -->
              <div class="switch-wrapper">
                <button
                  type="button"
                  class="toggle-switch"
                  :class="{ 'is-active': face.is_active, 'is-loading': togglingId === face.face_id }"
                  role="switch"
                  :aria-checked="face.is_active"
                  @click="toggleActive(face)"
                  :disabled="togglingId === face.face_id || !isDeviceReady"
                  :title="!isDeviceReady ? 'ESP32 device is offline' : (face.is_active ? 'Click to disable profile' : 'Click to enable profile')"
                >
                  <span class="toggle-thumb"></span>
                </button>
                <span class="switch-label" :class="face.is_active ? 'label-active' : 'label-inactive'">
                  {{ face.is_active ? 'Active' : 'Inactive' }}
                </span>
              </div>
            </td>
            <td>
              <button
                class="btn btn-danger btn-xs"
                @click="confirmDelete(face)"
                :disabled="isDeletingId === face.face_id || !isDeviceReady"
                :title="!isDeviceReady ? 'ESP32 device is offline' : 'Delete profile'"
              >
                {{ isDeletingId === face.face_id ? 'Deleting...' : 'Delete' }}
              </button>
            </td>
          </tr>
        </tbody>
      </table>
    </div>

    <!-- Confirm Delete All Modal Dialog -->
    <Teleport to="body">
      <div v-if="showDeleteAllModal" class="modal-backdrop" @click.self="showDeleteAllModal = false">
        <div class="modal-dialog">
          <div class="modal-header">
            <div class="danger-icon-badge">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class="danger-icon">
                <polyline points="3 6 5 6 21 6"></polyline>
                <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path>
              </svg>
            </div>
            <div>
              <h3 class="modal-title">Delete All Face Profiles?</h3>
              <p class="modal-subtitle">
                This will permanently delete all <strong>{{ faces.length }} biometric profiles</strong> from the Database and erase Flash NVS memory on the ESP32-S3. This action cannot be undone.
              </p>
            </div>
          </div>

          <div class="modal-actions">
            <button class="btn btn-secondary btn-sm" @click="showDeleteAllModal = false" :disabled="isDeletingAll">
              Cancel
            </button>
            <button class="btn btn-danger btn-sm" @click="executeDeleteAll" :disabled="isDeletingAll">
              {{ isDeletingAll ? 'Deleting...' : 'Confirm Delete All' }}
            </button>
          </div>
        </div>
      </div>
    </Teleport>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue';
import api from '../api/client';
import { useSystemStore } from '../stores/system';
import { useNotifyStore } from '../stores/notify';

const system = useSystemStore();
const notify = useNotifyStore();
const isDeviceReady = computed(() => system.isOnline);

const faces = ref([]);
const isLoading = ref(false);
const isDeletingId = ref(null);
const isDeletingAll = ref(false);
const togglingId = ref(null);
const showDeleteAllModal = ref(false);

defineExpose({ fetchFaces });

async function fetchFaces() {
  isLoading.value = true;
  try {
    const res = await api.get('/faces');
    if (res.data.success) {
      faces.value = res.data.faces;
    }
  } catch (err) {
    console.error('Failed to fetch faces:', err);
  } finally {
    isLoading.value = false;
  }
}

// Toggle switch Active / Inactive
async function toggleActive(face) {
  if (!isDeviceReady.value) {
    notify.warning('Cannot modify face profiles while ESP32 device is offline.', 'Hardware Offline');
    return;
  }

  togglingId.value = face.face_id;
  const previousState = face.is_active;
  face.is_active = !previousState; // Optimistic update

  try {
    const res = await api.patch(`/faces/${face.face_id}/toggle`, { is_active: face.is_active });
    if (res.data && res.data.face) {
      face.is_active = res.data.face.is_active;
    }
    notify.info(
      `Profile "${face.name}" is now ${face.is_active ? 'Active' : 'Inactive'}.`,
      'Profile Updated'
    );
  } catch (err) {
    face.is_active = previousState; // Revert on failure
    notify.error('Failed to update status: ' + (err.response?.data?.message || err.message));
  } finally {
    togglingId.value = null;
  }
}

// Delete single face from DB and MCU
async function confirmDelete(face) {
  if (!isDeviceReady.value) {
    notify.warning('Cannot modify face profiles while ESP32 device is offline.', 'Hardware Offline');
    return;
  }

  const confirmed = await notify.confirm({
    title: 'Delete Face Profile',
    message: `Are you sure you want to permanently delete "${face.name}" (ID #${face.face_id}) from Database and ESP32-S3?`,
    confirmText: 'Delete Profile',
    cancelText: 'Cancel',
    type: 'danger'
  });

  if (!confirmed) return;

  isDeletingId.value = face.face_id;
  try {
    await api.delete(`/faces/${face.face_id}`);
    faces.value = faces.value.filter(f => f.face_id !== face.face_id);
    notify.success(`Profile "${face.name}" deleted successfully.`, 'Profile Deleted');
  } catch (err) {
    notify.error('Failed to delete face: ' + (err.response?.data?.message || err.message));
  } finally {
    isDeletingId.value = null;
  }
}

// Execute Delete All via Modal
async function executeDeleteAll() {
  if (!isDeviceReady.value) {
    notify.warning('Cannot modify face profiles while ESP32 device is offline.', 'Hardware Offline');
    return;
  }

  isDeletingAll.value = true;
  try {
    await api.delete('/faces/all');
    faces.value = [];
    showDeleteAllModal.value = false;
    notify.success('All face profiles have been deleted.', 'Directory Cleared');
  } catch (err) {
    notify.error('Failed to delete all profiles: ' + (err.response?.data?.message || err.message));
  } finally {
    isDeletingAll.value = false;
  }
}

function formatTime(iso) {
  if (!iso) return 'N/A';
  const d = new Date(iso);
  return d.toLocaleString('en-US', {
    month: 'short',
    day: 'numeric',
    hour: '2-digit',
    minute: '2-digit'
  });
}

onMounted(() => {
  fetchFaces();
});
</script>

<style scoped>
.table-card {
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.table-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 10px;
}

.title-group {
  display: flex;
  align-items: center;
  gap: 10px;
}

.table-actions {
  display: flex;
  align-items: center;
  gap: 8px;
}

.title-group h3 {
  font-size: 0.95rem;
  font-weight: 700;
  letter-spacing: 0.05em;
}

.count-badge {
  font-size: 0.75rem;
  color: var(--color-primary);
  background: rgba(59, 130, 246, 0.15);
  padding: 2px 8px;
  border-radius: 4px;
}

.table-wrapper {
  overflow-x: auto;
}

.data-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.85rem;
  text-align: left;
}

.data-table th {
  padding: 10px 12px;
  background: rgba(31, 41, 55, 0.4);
  color: var(--text-muted);
  font-weight: 600;
  border-bottom: 1px solid var(--border-color);
  font-size: 0.75rem;
  letter-spacing: 0.05em;
}

.data-table td {
  padding: 12px;
  border-bottom: 1px solid rgba(75, 85, 99, 0.2);
}

.data-table tr:hover td {
  background: rgba(31, 41, 55, 0.3);
}

.row-inactive td {
  opacity: 0.6;
}

.name-cell {
  font-weight: 600;
  color: #f3f4f6;
}

.time-cell {
  font-size: 0.75rem;
  color: #9ca3af;
}

.empty-cell {
  text-align: center;
  padding: 30px;
  color: var(--text-muted);
}

.btn-icon {
  width: 14px;
  height: 14px;
}

.btn-danger-outline {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  padding: 6px 12px;
  font-size: 0.78rem;
  font-weight: 600;
  border-radius: 6px;
  background: rgba(239, 68, 68, 0.12);
  color: #ef4444;
  border: 1px solid rgba(239, 68, 68, 0.3);
  cursor: pointer;
  will-change: transform;
  transition: transform 0.2s cubic-bezier(0.4, 0, 0.2, 1), background-color 0.2s ease, border-color 0.2s ease;
}

.btn-danger-outline:hover:not(:disabled) {
  background: rgba(239, 68, 68, 0.22);
  border-color: #ef4444;
  transform: translateY(-1px);
}

.btn-danger-outline:disabled {
  opacity: 0.4;
  cursor: not-allowed;
  transform: none;
}

.refresh-icon {
  width: 14px;
  height: 14px;
}

.spinning {
  animation: spin 0.8s linear infinite;
}

/* Switch - 60fps GPU acceleration */
.switch-wrapper {
  display: flex;
  align-items: center;
  gap: 8px;
}

.toggle-switch {
  position: relative;
  width: 38px;
  height: 20px;
  background: #475569;
  border-radius: 9999px;
  border: none;
  cursor: pointer;
  padding: 2px;
  outline: none;
  will-change: background-color;
  transition: background-color 0.2s ease;
}

.toggle-switch.is-active {
  background: #10b981;
}

.toggle-thumb {
  position: absolute;
  top: 2px;
  left: 2px;
  width: 16px;
  height: 16px;
  background: #ffffff;
  border-radius: 50%;
  will-change: transform;
  transform: translate3d(0, 0, 0);
  transition: transform 0.2s cubic-bezier(0.4, 0, 0.2, 1);
  box-shadow: 0 1px 2px rgba(0, 0, 0, 0.3);
}

.toggle-switch.is-active .toggle-thumb {
  transform: translate3d(18px, 0, 0);
}

.switch-label {
  font-size: 0.75rem;
  font-weight: 600;
}

.label-active {
  color: #10b981;
}

.label-inactive {
  color: #94a3b8;
}

.btn-xs {
  padding: 4px 10px;
  font-size: 0.75rem;
}

.btn-danger {
  background: #ef4444;
  color: #fff;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-weight: 600;
  will-change: transform;
  transition: transform 0.15s cubic-bezier(0.4, 0, 0.2, 1), background-color 0.15s ease;
}

.btn-danger:hover:not(:disabled) {
  background: #dc2626;
  transform: translateY(-1px);
}

.btn-danger:disabled {
  opacity: 0.5;
  cursor: not-allowed;
  transform: none;
}

/* Modal Confirmation Dialog */
.modal-backdrop {
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.75);
  backdrop-filter: blur(4px);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 999;
  padding: 20px;
  will-change: opacity;
  animation: fadeIn 0.2s cubic-bezier(0.16, 1, 0.3, 1);
}

.modal-dialog {
  max-width: 440px;
  width: 100%;
  background: #1e293b;
  border: 1px solid #334155;
  border-radius: 12px;
  padding: 22px;
  display: flex;
  flex-direction: column;
  gap: 18px;
  box-shadow: 0 20px 40px rgba(0, 0, 0, 0.5);
  will-change: transform, opacity;
  animation: scaleUp 0.2s cubic-bezier(0.16, 1, 0.3, 1);
}

@keyframes fadeIn {
  from { opacity: 0; }
  to { opacity: 1; }
}

@keyframes scaleUp {
  from { transform: scale3d(0.95, 0.95, 1); opacity: 0; }
  to { transform: scale3d(1, 1, 1); opacity: 1; }
}

.modal-header {
  display: flex;
  gap: 14px;
  align-items: flex-start;
}

.danger-icon-badge {
  width: 44px;
  height: 44px;
  border-radius: 10px;
  background: rgba(239, 68, 68, 0.15);
  color: #ef4444;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  border: 1px solid rgba(239, 68, 68, 0.3);
}

.danger-icon {
  width: 22px;
  height: 22px;
}

.modal-title {
  font-size: 1.1rem;
  font-weight: 700;
  color: #f8fafc;
  margin-bottom: 4px;
}

.modal-subtitle {
  font-size: 0.825rem;
  color: #94a3b8;
  line-height: 1.5;
}

.modal-actions {
  display: flex;
  justify-content: flex-end;
  gap: 10px;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}
</style>

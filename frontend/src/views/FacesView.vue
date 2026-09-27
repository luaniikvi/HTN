<template>
  <div class="faces-page">
    <!-- Header -->
    <div class="page-title-row">
      <div>
        <h1 class="page-title">Face ID Directory</h1>
        <p class="page-subtitle">Manage edge biometric profiles synchronized across Database and ESP32-S3 Flash NVS</p>
      </div>

      <div class="header-actions">
        <!-- Delete All Button -->
        <button
          class="btn btn-danger-outline"
          @click="openDeleteAllConfirm"
          :disabled="faces.length === 0 || isDeletingAll || !isDeviceReady"
          title="Permanently remove all faces from Database and Flash MCU"
        >
          <Trash2 :size="16" />
          <span>{{ isDeletingAll ? 'Deleting...' : 'Delete All' }}</span>
        </button>

        <!-- Refresh Button -->
        <button class="btn btn-secondary" @click="fetchFaces" :disabled="isLoading">
          <RefreshCw :size="16" :class="{ spinning: isLoading }" />
          <span>Refresh</span>
        </button>

        <!-- Enroll New Face CTA -->
        <button class="btn btn-primary" @click="openEnrollModal" :disabled="!isDeviceReady">
          <UserPlus :size="16" />
          <span>Enroll New Face</span>
        </button>
      </div>
    </div>

    <!-- Offline Warning Strip -->
    <div v-if="!isDeviceReady" class="offline-banner">
      <AlertTriangle :size="18" />
      <span>ESP32 device is currently <strong>OFFLINE</strong>. Face enrollment, toggle, and deletion are disabled until the device reconnects.</span>
    </div>

    <!-- Main Table Container -->
    <div class="clean-card table-card">
      <!-- Search & Status Filter Bar -->
      <div class="filter-bar">
        <div class="search-input-wrapper">
          <Search :size="16" class="search-icon" />
          <input
            type="text"
            v-model="searchQuery"
            placeholder="Search by name or Face ID..."
            class="input-control search-input"
          />
        </div>

        <div class="filter-pills">
          <button
            class="btn btn-xs filter-pill"
            :class="{ active: filterRole === 'ALL' }"
            @click="filterRole = 'ALL'"
          >
            All ({{ faces.length }})
          </button>
          <button
            class="btn btn-xs filter-pill"
            :class="{ active: filterRole === 'PERMANENT' }"
            @click="filterRole = 'PERMANENT'"
          >
            Permanent
          </button>
          <button
            class="btn btn-xs filter-pill"
            :class="{ active: filterRole === 'TEMPORARY' }"
            @click="filterRole = 'TEMPORARY'"
          >
            Temporary
          </button>
        </div>
      </div>

      <!-- Table View -->
      <div class="table-responsive">
        <table class="faces-table">
          <thead>
            <tr>
              <th>Face ID</th>
              <th>Full Name</th>
              <th>Access Tier</th>
              <th>Expiration</th>
              <th>Status</th>
              <th class="text-right">Actions</th>
            </tr>
          </thead>
          <tbody>
            <tr v-if="filteredFaces.length === 0">
              <td colspan="6" class="empty-state-cell">
                <div class="empty-state-box">
                  <UserX :size="32" class="empty-icon" />
                  <p class="empty-title">No registered faces found</p>
                  <span class="empty-desc">
                    {{ searchQuery ? 'Try adjusting your search criteria' : 'Click "Enroll New Face" to register your first biometric profile' }}
                  </span>
                </div>
              </td>
            </tr>

            <tr v-for="face in filteredFaces" :key="face.id" :class="{ 'row-inactive': !face.is_active }">
              <td class="font-mono face-id-cell">#{{ face.face_id }}</td>
              <td class="name-cell">
                <div class="avatar-cell">
                  <div class="avatar-circle">
                    {{ face.name?.charAt(0)?.toUpperCase() || 'U' }}
                  </div>
                  <span class="avatar-name">{{ face.name }}</span>
                </div>
              </td>
              <td>
                <span
                  class="badge"
                  :class="face.role_type === 'PERMANENT' ? 'badge-disarmed' : 'badge-stay'"
                >
                  {{ face.role_type }}
                </span>
              </td>
              <td class="font-mono text-muted text-xs">
                {{ face.role_type === 'PERMANENT' ? 'Permanent Access' : formatExpiry(face.valid_until) }}
              </td>
              <td>
                <!-- Toggle Switch Active/Inactive -->
                <div class="switch-container">
                  <button
                    type="button"
                    class="toggle-switch"
                    :class="{ 'is-active': face.is_active, 'is-loading': togglingId === face.face_id }"
                    role="switch"
                    :aria-checked="face.is_active"
                    @click="toggleActive(face)"
                    :disabled="togglingId === face.face_id || !isDeviceReady"
                    :title="!isDeviceReady ? 'Device is offline' : (face.is_active ? 'Click to disable profile' : 'Click to enable profile')"
                  >
                    <span class="toggle-track">
                      <span class="toggle-thumb">
                        <span v-if="togglingId === face.face_id" class="toggle-spinner"></span>
                      </span>
                    </span>
                  </button>
                  <span class="status-label" :class="face.is_active ? 'text-active' : 'text-inactive'">
                    {{ face.is_active ? 'Active' : 'Inactive' }}
                  </span>
                </div>
              </td>
              <td class="text-right">
                <button
                  class="btn btn-secondary btn-xs btn-delete"
                  @click="confirmDelete(face)"
                  :disabled="isDeletingId === face.face_id || !isDeviceReady"
                  :title="!isDeviceReady ? 'Device is offline' : 'Permanently delete from Database and Flash MCU'"
                >
                  <Trash2 :size="13" />
                  <span>{{ isDeletingId === face.face_id ? 'Deleting...' : 'Delete' }}</span>
                </button>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- Confirm Delete All Modal -->
    <Teleport to="body">
      <div v-if="showDeleteAllModal" class="modal-backdrop" @click.self="showDeleteAllModal = false">
        <div class="modal-dialog">
          <div class="modal-header">
            <div class="danger-icon-badge">
              <Trash2 :size="24" />
            </div>
            <div>
              <h3 class="modal-title">Delete All Face Profiles?</h3>
              <p class="modal-subtitle">
                This will permanently delete all <strong>{{ faces.length }} biometric profiles</strong> from the Database and wipe the ESP32-S3 Flash NVS memory. This action cannot be undone.
              </p>
            </div>
          </div>

          <div class="modal-actions">
            <button class="btn btn-secondary" @click="showDeleteAllModal = false" :disabled="isDeletingAll">
              Cancel
            </button>
            <button class="btn btn-danger" @click="executeDeleteAll" :disabled="isDeletingAll">
              <Trash2 :size="16" />
              <span>{{ isDeletingAll ? 'Deleting...' : 'Confirm Delete All' }}</span>
            </button>
          </div>
        </div>
      </div>
    </Teleport>

    <!-- 3-Step Enrollment Wizard Modal -->
    <Teleport to="body">
      <EnrollmentWizard
        v-if="isEnrollModalOpen"
        @close="isEnrollModalOpen = false"
        @face-saved="handleFaceSaved"
      />
    </Teleport>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue';
import api from '../api/client';
import { useSystemStore } from '../stores/system';
import { useNotifyStore } from '../stores/notify';
import {
  UserPlus,
  RefreshCw,
  Search,
  UserX,
  Trash2,
  AlertTriangle
} from 'lucide-vue-next';

import EnrollmentWizard from '../components/EnrollmentWizard.vue';

const system = useSystemStore();
const notify = useNotifyStore();
const isDeviceReady = computed(() => system.isOnline);

const faces = ref([]);
const isLoading = ref(false);
const isDeletingId = ref(null);
const isDeletingAll = ref(false);
const togglingId = ref(null);
const showDeleteAllModal = ref(false);
const isEnrollModalOpen = ref(false);
const searchQuery = ref('');
const filterRole = ref('ALL');

const filteredFaces = computed(() => {
  return faces.value.filter(face => {
    const matchesSearch =
      face.name.toLowerCase().includes(searchQuery.value.toLowerCase()) ||
      String(face.face_id).includes(searchQuery.value);

    const matchesRole =
      filterRole.value === 'ALL' || face.role_type === filterRole.value;

    return matchesSearch && matchesRole;
  });
});

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

function openEnrollModal() {
  if (!isDeviceReady.value) {
    notify.warning('Device is OFFLINE. Cannot enroll faces.', 'Hardware Offline');
    return;
  }
  isEnrollModalOpen.value = true;
}

// Toggle Active/Inactive state
async function toggleActive(face) {
  if (!isDeviceReady.value) {
    notify.warning('Device is OFFLINE. Cannot change face status.', 'Hardware Offline');
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
    face.is_active = previousState; // Revert on network failure
    notify.error('Failed to update status: ' + (err.response?.data?.message || err.message));
  } finally {
    togglingId.value = null;
  }
}

// Delete single face: direct DB purge & MCU sync
async function confirmDelete(face) {
  if (!isDeviceReady.value) {
    notify.warning('Device is OFFLINE. Cannot delete face.', 'Hardware Offline');
    return;
  }

  const confirmed = await notify.confirm({
    title: 'Delete Face Profile',
    message: `Are you sure you want to permanently delete "${face.name}" (ID #${face.face_id}) from Database and ESP32-S3 Flash memory?`,
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
    notify.error('Deletion error: ' + (err.response?.data?.message || err.message));
  } finally {
    isDeletingId.value = null;
  }
}

// Open Delete All Confirmation Modal
function openDeleteAllConfirm() {
  if (!isDeviceReady.value) {
    notify.warning('Device is OFFLINE. Cannot delete faces.', 'Hardware Offline');
    return;
  }
  if (faces.value.length === 0) return;
  showDeleteAllModal.value = true;
}

// Execute Delete All
async function executeDeleteAll() {
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

function handleFaceSaved() {
  fetchFaces();
}

function formatExpiry(iso) {
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
.faces-page {
  display: flex;
  flex-direction: column;
  gap: 24px;
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

.header-actions {
  display: flex;
  align-items: center;
  gap: 10px;
}

.spinning {
  animation: spin 0.8s linear infinite;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

/* Danger Outline Button */
.btn-danger-outline {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 8px 14px;
  font-size: 0.825rem;
  font-weight: 600;
  border-radius: var(--radius-sm, 8px);
  background: rgba(239, 68, 68, 0.1);
  color: #ef4444;
  border: 1px solid rgba(239, 68, 68, 0.3);
  cursor: pointer;
  will-change: transform, opacity;
  transition: transform 0.2s cubic-bezier(0.4, 0, 0.2, 1), background-color 0.2s ease, border-color 0.2s ease, box-shadow 0.2s ease;
}

.btn-danger-outline:hover:not(:disabled) {
  background: rgba(239, 68, 68, 0.2);
  border-color: #ef4444;
  box-shadow: 0 0 14px rgba(239, 68, 68, 0.25);
  transform: translateY(-1px);
}

.btn-danger-outline:disabled {
  opacity: 0.45;
  cursor: not-allowed;
  transform: none;
}

/* Table Card */
.table-card {
  padding: 0;
  overflow: hidden;
}

.filter-bar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 14px 18px;
  border-bottom: 1px solid var(--border-subtle);
  gap: 12px;
  flex-wrap: wrap;
}

.search-input-wrapper {
  position: relative;
  width: 280px;
}

.search-icon {
  position: absolute;
  left: 12px;
  top: 50%;
  transform: translateY(-50%);
  color: var(--text-muted);
}

.search-input {
  padding-left: 36px;
  padding-top: 7px;
  padding-bottom: 7px;
  font-size: 0.825rem;
}

.filter-pills {
  display: flex;
  gap: 6px;
}

.filter-pill {
  color: var(--text-secondary);
  border-color: var(--border-subtle);
}

.filter-pill.active {
  background: var(--color-primary-subtle);
  color: var(--color-primary);
  border-color: var(--color-primary-border);
}

/* Table Styles */
.table-responsive {
  overflow-x: auto;
}

.faces-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.825rem;
  text-align: left;
}

.faces-table th {
  padding: 12px 18px;
  background: var(--bg-subtle);
  color: var(--text-secondary);
  font-weight: 600;
  font-size: 0.72rem;
  text-transform: uppercase;
  letter-spacing: 0.05em;
  border-bottom: 1px solid var(--border-subtle);
}

.faces-table td {
  padding: 14px 18px;
  border-bottom: 1px solid var(--border-subtle);
  color: var(--text-main);
  vertical-align: middle;
}

.faces-table tr:last-child td {
  border-bottom: none;
}

.faces-table tr:hover td {
  background: var(--bg-card-hover);
}

.row-inactive td {
  opacity: 0.65;
}

.face-id-cell {
  font-weight: 700;
  color: var(--color-primary);
}

.avatar-cell {
  display: flex;
  align-items: center;
  gap: 10px;
}

.avatar-circle {
  width: 32px;
  height: 32px;
  border-radius: 50%;
  background: var(--color-primary-subtle);
  color: var(--color-primary);
  font-weight: 700;
  font-size: 0.8rem;
  display: flex;
  align-items: center;
  justify-content: center;
  border: 1px solid var(--color-primary-border);
}

.avatar-name {
  font-weight: 600;
  color: var(--text-main);
}

/* Toggle Switch - Pure Transform / Opacity for 60fps */
.switch-container {
  display: flex;
  align-items: center;
  gap: 10px;
}

.toggle-switch {
  position: relative;
  width: 44px;
  height: 24px;
  background: #334155;
  border-radius: 9999px;
  border: 1px solid rgba(255, 255, 255, 0.1);
  cursor: pointer;
  padding: 2px;
  outline: none;
  display: inline-flex;
  align-items: center;
  will-change: background-color, border-color, box-shadow;
  transition: background-color 0.25s ease, border-color 0.25s ease, box-shadow 0.25s ease;
}

.toggle-switch:focus-visible {
  box-shadow: 0 0 0 2px var(--bg-card), 0 0 0 4px var(--color-primary);
}

.toggle-switch.is-active {
  background: var(--color-primary, #10b981);
  border-color: rgba(16, 185, 129, 0.4);
  box-shadow: 0 0 10px rgba(16, 185, 129, 0.35);
}

.toggle-switch:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.toggle-track {
  width: 100%;
  height: 100%;
  position: relative;
  display: block;
}

.toggle-thumb {
  position: absolute;
  top: 1px;
  left: 1px;
  width: 18px;
  height: 18px;
  background: #ffffff;
  border-radius: 50%;
  will-change: transform;
  transform: translate3d(0, 0, 0);
  transition: transform 0.25s cubic-bezier(0.4, 0, 0.2, 1);
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.4);
  display: flex;
  align-items: center;
  justify-content: center;
}

.toggle-switch.is-active .toggle-thumb {
  transform: translate3d(18px, 0, 0);
}

.toggle-spinner {
  width: 10px;
  height: 10px;
  border: 2px solid #cbd5e1;
  border-top-color: #3b82f6;
  border-radius: 50%;
  animation: spin 0.6s linear infinite;
}

.status-label {
  font-size: 0.78rem;
  font-weight: 600;
  letter-spacing: 0.02em;
}

.text-active {
  color: var(--color-primary, #10b981);
}

.text-inactive {
  color: var(--text-muted, #64748b);
}

.text-right {
  text-align: right;
}

.btn-delete {
  color: var(--color-armed, #ef4444);
}

.btn-delete:hover:not(:disabled) {
  background: var(--color-armed-subtle, rgba(239, 68, 68, 0.15));
  border-color: var(--color-armed-border, rgba(239, 68, 68, 0.3));
}

/* Empty State */
.empty-state-cell {
  padding: 48px 20px;
  text-align: center;
}

.empty-state-box {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
}

.empty-icon {
  color: var(--text-muted);
}

.empty-title {
  font-size: 0.95rem;
  font-weight: 700;
  color: var(--text-main);
}

.empty-desc {
  font-size: 0.78rem;
  color: var(--text-secondary);
}

/* Modal Backdrop & Dialog - GPU accelerated transform/opacity */
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
  max-width: 480px;
  width: 100%;
  background: var(--bg-card, #1e293b);
  border: 1px solid var(--border-subtle, #334155);
  border-radius: 12px;
  padding: 24px;
  display: flex;
  flex-direction: column;
  gap: 20px;
  box-shadow: 0 20px 40px rgba(0, 0, 0, 0.5);
  will-change: transform, opacity;
  animation: scaleUp 0.22s cubic-bezier(0.16, 1, 0.3, 1);
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
  gap: 16px;
  align-items: flex-start;
}

.danger-icon-badge {
  width: 48px;
  height: 48px;
  border-radius: 12px;
  background: rgba(239, 68, 68, 0.15);
  color: #ef4444;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  border: 1px solid rgba(239, 68, 68, 0.3);
}

.modal-title {
  font-size: 1.15rem;
  font-weight: 700;
  color: var(--text-main, #f8fafc);
  margin-bottom: 6px;
}

.modal-subtitle {
  font-size: 0.85rem;
  color: var(--text-secondary, #94a3b8);
  line-height: 1.5;
}

.modal-actions {
  display: flex;
  justify-content: flex-end;
  gap: 10px;
}

.btn-danger {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 8px 16px;
  background: #ef4444;
  color: #fff;
  border: none;
  border-radius: var(--radius-sm, 8px);
  font-weight: 600;
  cursor: pointer;
  will-change: transform;
  transition: transform 0.2s cubic-bezier(0.4, 0, 0.2, 1), background-color 0.2s ease, box-shadow 0.2s ease;
}

.btn-danger:hover:not(:disabled) {
  background: #dc2626;
  box-shadow: 0 0 14px rgba(239, 68, 68, 0.4);
  transform: translateY(-1px);
}

.btn-danger:disabled {
  opacity: 0.5;
  cursor: not-allowed;
  transform: none;
}
</style>

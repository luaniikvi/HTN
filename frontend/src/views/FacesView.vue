<template>
  <div class="faces-page">
    <!-- Header -->
    <div class="page-title-row">
      <div>
        <h1 class="page-title">Face ID Directory</h1>
        <p class="page-subtitle">Manage edge biometric templates stored on ESP32-S3 NVS Flash memory</p>
      </div>

      <div class="header-actions">
        <!-- Refresh Button -->
        <button class="btn btn-secondary" @click="fetchFaces" :disabled="isLoading">
          <RefreshCw :size="16" :class="{ spinning: isLoading }" />
          <span>Refresh</span>
        </button>

        <!-- Enroll New Face CTA -->
        <button class="btn btn-primary" @click="isEnrollModalOpen = true">
          <UserPlus :size="16" />
          <span>Enroll New Face</span>
        </button>
      </div>
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
              <th>MCU Status</th>
              <th class="text-right">Action</th>
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
                <span class="badge" :class="face.is_active ? 'badge-online' : 'badge-offline'">
                  {{ face.is_active ? 'ACTIVE' : 'DELETED' }}
                </span>
              </td>
              <td class="text-right">
                <button
                  v-if="face.is_active"
                  class="btn btn-secondary btn-xs btn-delete"
                  @click="confirmDelete(face)"
                  :disabled="isDeletingId === face.face_id"
                >
                  <Trash2 :size="13" />
                  <span>{{ isDeletingId === face.face_id ? 'Deleting...' : 'Delete' }}</span>
                </button>
                <span v-else class="text-muted text-xs">Purged</span>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- 3-Step Enrollment Wizard Modal -->
    <EnrollmentWizard
      v-if="isEnrollModalOpen"
      @close="isEnrollModalOpen = false"
      @face-saved="handleFaceSaved"
    />
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue';
import api from '../api/client';
import {
  UserPlus,
  RefreshCw,
  Search,
  UserX,
  Trash2
} from 'lucide-vue-next';

import EnrollmentWizard from '../components/EnrollmentWizard.vue';

const faces = ref([]);
const isLoading = ref(false);
const isDeletingId = ref(null);
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

async function confirmDelete(face) {
  if (!confirm(`Are you sure you want to delete "${face.name}" (ID #${face.face_id}) from ESP32-S3 Flash NVS?`)) {
    return;
  }

  isDeletingId.value = face.face_id;
  try {
    await api.delete(`/faces/${face.face_id}`);
    alert(`Deletion command sent to ESP32-S3. Record will update upon device ACK.`);
    setTimeout(fetchFaces, 1500);
  } catch (err) {
    alert('Deletion error: ' + (err.response?.data?.message || err.message));
  } finally {
    isDeletingId.value = null;
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
  opacity: 0.5;
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

.text-right {
  text-align: right;
}

.btn-delete {
  color: var(--color-armed);
}

.btn-delete:hover:not(:disabled) {
  background: var(--color-armed-subtle);
  border-color: var(--color-armed-border);
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
</style>
